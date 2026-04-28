#include "sessionbroker.h"
#include "sessionrollback.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThread>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

namespace Slm::Login {

namespace {

constexpr auto kCompositorLogPath = "/tmp/slm-compositor.log";
constexpr auto kShellLogPath = "/tmp/slm-shell.log";

QString backendName(CompositorBackend backend)
{
    switch (backend) {
    case CompositorBackend::KWin:
        return QStringLiteral("kwin");
    case CompositorBackend::Unknown:
        return QStringLiteral("unknown");
    }
    return QStringLiteral("unknown");
}

QString processExitStatusName(QProcess::ExitStatus status)
{
    return status == QProcess::CrashExit ? QStringLiteral("crash")
                                         : QStringLiteral("normal");
}

QByteArray readFileFromOffset(const QString &path, qint64 offset)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    if (offset > 0 && !file.seek(offset)) {
        return {};
    }
    return file.readAll();
}

QStringList tailLines(const QByteArray &data, int maxLines)
{
    const QList<QByteArray> rawLines = data.split('\n');
    QStringList lines;
    const int start = qMax(0, rawLines.size() - maxLines);
    for (int i = start; i < rawLines.size(); ++i) {
        const QByteArray line = rawLines.at(i).trimmed();
        if (!line.isEmpty()) {
            lines.append(QString::fromLocal8Bit(line));
        }
    }
    return lines;
}

bool acceptsRegularWaylandSocketForTests()
{
    return qEnvironmentVariableIsSet("SLM_TEST_ACCEPT_REGULAR_WAYLAND_SOCKET");
}

} // namespace

SessionBroker::SessionBroker(StartupMode requestedMode, QObject *parent)
    : QObject(parent)
    , m_requestedMode(requestedMode)
{
}

// ── Public ────────────────────────────────────────────────────────────────────

int SessionBroker::run()
{
    readState();
    m_config.load();
    qInfo("config: path=%s compositor=%s",
          qUtf8Printable(ConfigManager::activePath()),
          qUtf8Printable(m_config.compositor()));

    // Ensure core session env exists before integrity checks.
    qputenv("SLM_OFFICIAL_SESSION", "1");
    qputenv("XDG_SESSION_TYPE", "wayland");
    qputenv("XDG_CURRENT_DESKTOP", "SLM");
    if (qgetenv("XDG_RUNTIME_DIR").isEmpty()) {
        const QString runtimeDir = QStringLiteral("/run/user/") + QString::number(::getuid());
        qputenv("XDG_RUNTIME_DIR", runtimeDir.toLocal8Bit());
    }

    validateLogindSession();

    const PlatformChecker checker;
    const PlatformStatus platform = checker.checkAll(m_config.compositor(), m_config.shell());
    if (!platform.ok) {
        qWarning("slm-session-broker: platform integrity issues:");
        for (const QString &issue : platform.issues)
            qWarning("  - %s", qUtf8Printable(issue));
    }

    m_finalMode = evaluateMode(platform);
    qInfo("slm-session-broker: startup mode = %s (requested = %s, crashes = %d)",
          qUtf8Printable(startupModeToString(m_finalMode)),
          qUtf8Printable(startupModeToString(m_requestedMode)),
          m_state.crashCount);

    performRollback(m_finalMode);
    prepareEnvironment(m_finalMode);

    const QString missingReason = preflightMissingComponentReason();
    if (!missingReason.isEmpty()) {
        qCritical("slm-session-broker: missing required component: %s",
                  qUtf8Printable(missingReason));
        writeStartupFailed(missingReason);
        return 1;
    }

    m_plan = buildCompositorPlan();

    if (!launchCompositor()) {
        const QString reason = QStringLiteral("compositor-launch-failed:%1:%2")
                                   .arg(m_plan.binary, m_compositorProcess.errorString());
        writeStartupFailed(reason);
        return 1;
    }

    m_activeWaylandDisplay = detectWaylandSocket();
    if (m_activeWaylandDisplay.isEmpty()) {
        QString reason;
        if (m_compositorProcess.state() == QProcess::NotRunning) {
            reason = compositorExitReason(QStringLiteral("compositor-exited-before-socket"));
        } else {
            reason = QStringLiteral("compositor-socket-timeout");
            const QString hint = compositorLogHint();
            if (!hint.isEmpty()) {
                reason += QStringLiteral(":") + hint;
            }
        }
        qCritical("slm-session-broker: startup failed: %s", qUtf8Printable(reason));
        terminateCompositor(reason);
        writeStartupFailed(reason);
        return 1;
    }

    // Expose active socket for watchdog and any other subprocesses we spawn.
    qputenv("WAYLAND_DISPLAY", m_activeWaylandDisplay.toLocal8Bit());

    QString failureReason;
    if (!launchShell(&failureReason)) {
        terminateCompositor(failureReason);
        writeStartupFailed(failureReason);
        return 1;
    }
    if (!launchWatchdog(&failureReason)) {
        terminateCompositor(failureReason);
        writeStartupFailed(failureReason);
        return 1;
    }

    qInfo("session: monitoring compositor and shell exits…");
    QString crashReason = (m_finalMode == StartupMode::Recovery)
            ? monitorRecoverySession()
            : monitorSession();

    const int  exitCode = m_compositorProcess.exitCode();
    const bool isCrash  = (m_compositorProcess.exitStatus() == QProcess::CrashExit);
    const bool crashed  = !m_compositorStopRequested && (isCrash || (exitCode != 0));

    if (crashReason.isEmpty() && crashed) {
        crashReason = compositorExitReason(QStringLiteral("compositor-exited-abnormally"));
    }

    if (isCrash)
        qWarning("compositor: exited via signal/crash exit_code=%d", exitCode);
    else
        qInfo("compositor: exited normally exit_code=%d", exitCode);

    // Give shell a moment to flush its own exit — it is killed by compositor teardown.
    if (m_shellProcess.state() != QProcess::NotRunning)
        m_shellProcess.waitForFinished(3000);

    logFileTail(QStringLiteral("compositor"), QString::fromLatin1(kCompositorLogPath),
                m_compositorLogStartOffset);
    logFileTail(QStringLiteral("shell"), QString::fromLatin1(kShellLogPath),
                m_shellLogStartOffset);

    writeSessionEnded(crashReason);
    qInfo("session: ended (crashed=%s reason=%s)",
          crashReason.isEmpty() ? "no" : "yes",
          qPrintable(crashReason.isEmpty() ? QStringLiteral("<none>") : crashReason));
    return crashReason.isEmpty() ? 0 : 1;
}

// ── Private ───────────────────────────────────────────────────────────────────

void SessionBroker::readState()
{
    QString err;
    if (!SessionStateIO::load(m_state, err))
        qWarning("slm-session-broker: state load error: %s (using defaults)", qUtf8Printable(err));

    qInfo("slm-session-broker: prev state — crashCount=%d lastBootStatus=%s "
          "configPending=%s lastMode=%s recoveryReason=%s lastCrashReason=%s",
          m_state.crashCount,
          qPrintable(m_state.lastBootStatus),
          m_state.configPending ? "yes" : "no",
          qPrintable(startupModeToString(m_state.lastMode)),
          m_state.recoveryReason.isEmpty() ? "<none>" : qPrintable(m_state.recoveryReason),
          m_state.lastCrashReason.isEmpty() ? "<none>" : qPrintable(m_state.lastCrashReason));

    if (m_state.configPending && m_state.lastBootStatus != QStringLiteral("healthy")) {
        if (m_config.hasPreviousConfig()) {
            if (m_config.rollbackToPrevious(&err))
                qInfo("slm-session-broker: config rolled back (configPending + unconfirmed session)");
            else
                qWarning("slm-session-broker: configPending rollback failed: %s", qUtf8Printable(err));
        }
    }

    const int prevCount = m_state.crashCount;
    if (m_state.crashCount < kCrashLoopThreshold + 1)
        m_state.crashCount++;
    qInfo("slm-session-broker: crash counter: %d → %d (threshold=%d, recovery_at>=%d)",
          prevCount, m_state.crashCount, kCrashLoopThreshold, kCrashLoopThreshold);

    m_state.lastBootStatus = QStringLiteral("started");
    m_state.lastMode       = m_requestedMode;
    m_state.lastUpdated    = QDateTime::currentDateTimeUtc();

    if (!SessionStateIO::save(m_state, err))
        qWarning("slm-session-broker: state save error: %s", qUtf8Printable(err));
}

StartupMode SessionBroker::evaluateMode(const PlatformStatus &platform)
{
    QString partitionReason;
    if (hasRecoveryPartitionRequest(&partitionReason)) {
        m_state.recoveryReason = partitionReason.isEmpty()
                ? QStringLiteral("recovery-partition-requested")
                : partitionReason;
        return StartupMode::Recovery;
    }
    if (m_state.safeModeForced)
        return StartupMode::Safe;
    if (m_state.crashCount >= kCrashLoopThreshold) {
        m_state.recoveryReason = QStringLiteral("crash loop detected (%1 crashes)")
                                     .arg(m_state.crashCount);
        return StartupMode::Recovery;
    }
    if (!platform.ok && m_requestedMode == StartupMode::Normal) {
        m_state.recoveryReason = platform.issues.first();
        return StartupMode::Safe;
    }
    return m_requestedMode;
}

bool SessionBroker::hasRecoveryPartitionRequest(QString *reason) const
{
    if (reason) reason->clear();
    const QString markerPath = ConfigManager::configDir()
            + QStringLiteral("/recovery-partition-request.json");
    QFile marker(markerPath);
    if (!marker.exists()) return false;
    if (!marker.open(QIODevice::ReadOnly)) {
        if (reason) *reason = QStringLiteral("recovery-partition-request-unreadable");
        return true;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(marker.readAll());
    if (!doc.isObject()) {
        if (reason) *reason = QStringLiteral("recovery-partition-request-invalid");
        return true;
    }
    const QJsonObject obj = doc.object();
    if (reason) *reason = obj.value(QStringLiteral("reason")).toString().trimmed();
    return obj.value(QStringLiteral("requested")).toBool(true);
}

void SessionBroker::performRollback(StartupMode mode)
{
    QString err;
    switch (mode) {
    case StartupMode::Normal:
        break;
    case StartupMode::Safe:
        if (!m_config.rollbackToSafe(&err))
            qWarning("slm-session-broker: safe rollback failed: %s", qUtf8Printable(err));
        else
            qInfo("slm-session-broker: rolled back config to safe baseline");
        break;
    case StartupMode::Recovery:
        switch (rollbackRecoveryConfig(m_config, m_state, &err)) {
        case RecoveryRollbackSource::LastGoodSnapshot:
            qInfo("slm-session-broker: rolled back config to last_good_snapshot (%s)",
                  qUtf8Printable(m_state.lastGoodSnapshot));
            break;
        case RecoveryRollbackSource::Previous:
            qInfo("slm-session-broker: rolled back config to previous");
            break;
        case RecoveryRollbackSource::Safe:
            qInfo("slm-session-broker: rolled back config to safe baseline (fallback)");
            break;
        case RecoveryRollbackSource::None:
            qWarning("slm-session-broker: recovery rollback failed: %s", qUtf8Printable(err));
            break;
        }
        break;
    }
}

void SessionBroker::prepareEnvironment(StartupMode mode)
{
    qputenv("SLM_SESSION_MODE",     startupModeToString(mode).toLocal8Bit());
    qputenv("SLM_OFFICIAL_SESSION", "1");
    qputenv("XDG_SESSION_TYPE",     "wayland");
    qputenv("XDG_CURRENT_DESKTOP",  "SLM");
    // WAYLAND_DISPLAY is intentionally NOT set here — it is set after the compositor
    // socket is confirmed ready and the actual socket name is known.

    if (qgetenv("LIBSEAT_BACKEND").isEmpty())
        qputenv("LIBSEAT_BACKEND", "logind");
    if (qgetenv("XDG_RUNTIME_DIR").isEmpty()) {
        const QString runtimeDir = QStringLiteral("/run/user/") + QString::number(::getuid());
        qputenv("XDG_RUNTIME_DIR", runtimeDir.toLocal8Bit());
    }

    m_state.configPending = (mode == StartupMode::Normal);
    QString err;
    if (!SessionStateIO::save(m_state, err))
        qWarning("slm-session-broker: failed to persist configPending: %s", qUtf8Printable(err));
}

static bool executableExists(const QString &bin)
{
    if (bin.startsWith(QLatin1Char('/'))) {
        const QFileInfo fi(bin);
        return fi.exists() && fi.isExecutable();
    }
    return !QStandardPaths::findExecutable(bin).isEmpty();
}

QString SessionBroker::preflightMissingComponentReason() const
{
    if (!executableExists(m_config.compositor()))
        return QStringLiteral("missing-component:compositor:%1").arg(m_config.compositor());
    if (m_finalMode != StartupMode::Recovery && !executableExists(m_config.shell()))
        return QStringLiteral("missing-component:shell:%1").arg(m_config.shell());
    if (QStandardPaths::findExecutable(QStringLiteral("slm-watchdog")).isEmpty())
        return QStringLiteral("missing-component:slm-watchdog");
    if (m_finalMode == StartupMode::Recovery
            && QStandardPaths::findExecutable(QStringLiteral("slm-recovery-app")).isEmpty())
        return QStringLiteral("missing-component:slm-recovery-app");
    return {};
}

// ── Compositor plan ───────────────────────────────────────────────────────────

QString SessionBroker::kwinGetCustomSocketFlag(const QString &bin)
{
    QProcess p;
    p.start(bin, {QStringLiteral("--help")});
    if (!p.waitForFinished(3000)) return {};
    const QString help = QString::fromLocal8Bit(p.readAllStandardOutput())
                       + QString::fromLocal8Bit(p.readAllStandardError());
    if (help.contains(QStringLiteral("--socket")))
        return QStringLiteral("--socket");
    if (help.contains(QStringLiteral("--wayland-display")))
        return QStringLiteral("--wayland-display");
    return {};
}

CompositorLaunchPlan SessionBroker::buildCompositorPlan()
{
    CompositorLaunchPlan plan;
    plan.env = QProcessEnvironment::systemEnvironment();

    // Apply config-level env overrides.
    for (const QString &kv : m_config.compositorEnv()) {
        const int eq = kv.indexOf(QLatin1Char('='));
        if (eq > 0)
            plan.env.insert(kv.left(eq), kv.mid(eq + 1));
    }
    if (!m_config.compositorEnv().isEmpty()) {
        qInfo("compositor: config env overrides: %s",
              qUtf8Printable(m_config.compositorEnv().join(QLatin1Char(' '))));
    }

    QString compositorBin  = m_config.compositor();
    QStringList compositorArgs = m_config.compositorArgs();

    // kwin_wayland requires a DRM render node (/dev/dri/renderD128+).
    // In KWin-only mode we keep that failure visible instead of silently
    // switching to a different compositor.
    auto drmNodeExists = [](const QString &prefix, int lo, int hi) -> bool {
        for (int i = lo; i < hi; ++i)
            if (QFileInfo::exists(prefix + QString::number(i))) return true;
        return false;
    };
    const bool hasDrmCard   = drmNodeExists(QStringLiteral("/dev/dri/card"),    0,   8);
    const bool hasDrmRender = drmNodeExists(QStringLiteral("/dev/dri/renderD"), 128, 136);
    qInfo("compositor: drm_card=%s drm_render=%s",
          hasDrmCard   ? "yes" : "no",
          hasDrmRender ? "yes" : "no");

    QString binName = QFileInfo(compositorBin).fileName();
    if (binName != QStringLiteral("kwin_wayland")) {
        qWarning("compositor: unsupported configured compositor '%s' — forcing kwin_wayland",
                 qPrintable(compositorBin));
        compositorBin = QStringLiteral("kwin_wayland");
        compositorArgs = {};
        binName = QStringLiteral("kwin_wayland");
    }

    if (!hasDrmRender) {
        qWarning("compositor: no DRM render node — kwin_wayland may fail to open DRM");
        qWarning("compositor: KWin-only mode has no alternate compositor fallback; fix DRM/logind seat setup");
    }

    plan.binary = compositorBin;
    plan.args   = compositorArgs;

    if (binName == QStringLiteral("kwin_wayland")) {
        plan.backend = CompositorBackend::KWin;
        const QString socketFlag = kwinGetCustomSocketFlag(compositorBin);
        if (!socketFlag.isEmpty()) {
            plan.args << socketFlag << QString::fromLatin1(kSlmWaylandSocketName);
            plan.socketStrategy = SocketStrategy::Fixed;
            plan.socketName     = QString::fromLatin1(kSlmWaylandSocketName);
            qInfo("compositor: kwin socket flag '%s' → using fixed socket '%s'",
                  qPrintable(socketFlag), kSlmWaylandSocketName);
        } else {
            qWarning("compositor: kwin_wayland has no custom socket flag — "
                     "will scan for socket after launch");
            plan.socketStrategy = SocketStrategy::Scan;
        }
    } else {
        plan.backend        = CompositorBackend::Unknown;
        plan.socketStrategy = SocketStrategy::Scan;
        qWarning("compositor: unknown compositor '%s' — will scan for socket",
                 qPrintable(compositorBin));
    }

    // For scan strategy, snapshot which wayland-* sockets already exist so we
    // can identify the one the compositor creates.
    if (plan.socketStrategy == SocketStrategy::Scan) {
        const QString runtimeDir = QString::fromLocal8Bit(qgetenv("XDG_RUNTIME_DIR"));
        const QDir dir(runtimeDir);
        const QStringList entries = dir.entryList(
            QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot);
        for (const QString &e : entries) {
            if (e.startsWith(QStringLiteral("wayland-")))
                plan.preExistingSockets.insert(e);
        }
        qInfo("compositor: pre-existing wayland sockets: [%s]",
              qPrintable(QStringList(plan.preExistingSockets.begin(),
                                    plan.preExistingSockets.end()).join(QLatin1Char(','))));
    }

    return plan;
}

// ── Socket helpers ────────────────────────────────────────────────────────────

static bool tryConnectUnixSocket(const QString &path)
{
    const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return false;
    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    const QByteArray pathBytes = path.toLocal8Bit();
    if (pathBytes.size() >= static_cast<int>(sizeof(addr.sun_path))) {
        ::close(fd);
        return false;
    }
    ::memcpy(addr.sun_path, pathBytes.constData(), static_cast<size_t>(pathBytes.size()));
    const bool ok = ::connect(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == 0;
    ::close(fd);
    return ok;
}

static bool isUnixSocket(const QString &path)
{
    struct stat st{};
    return (::stat(path.toLocal8Bit().constData(), &st) == 0) && S_ISSOCK(st.st_mode);
}

static bool isWaylandSocketReady(const QString &path)
{
    if (isUnixSocket(path) && tryConnectUnixSocket(path)) {
        return true;
    }
    return acceptsRegularWaylandSocketForTests() && QFileInfo::exists(path);
}

void SessionBroker::removeStaleSlmSockets()
{
    const QString runtimeDir = QString::fromLocal8Bit(qgetenv("XDG_RUNTIME_DIR"));
    if (runtimeDir.isEmpty()) return;
    const QDir dir(runtimeDir);
    const QStringList entries = dir.entryList(
        QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        if (!entry.startsWith(QStringLiteral("slm-wayland-"))) continue;
        const QString socketPath = runtimeDir + QStringLiteral("/") + entry;
        if (isUnixSocket(socketPath) && QFile::remove(socketPath))
            qWarning("compositor: removed stale slm socket: %s", qPrintable(socketPath));
    }
}

// ── Compositor launch & socket detection ──────────────────────────────────────

bool SessionBroker::launchCompositor()
{
    removeStaleSlmSockets();

    m_compositorProcess.setProgram(m_plan.binary);
    m_compositorProcess.setArguments(m_plan.args);
    m_compositorProcess.setProcessEnvironment(m_plan.env);
    m_compositorProcess.setProcessChannelMode(QProcess::MergedChannels);
    m_compositorLogStartOffset = QFileInfo(QString::fromLatin1(kCompositorLogPath)).size();
    m_compositorProcess.setStandardOutputFile(
        QString::fromLatin1(kCompositorLogPath), QIODeviceBase::Append);

    const char *strategyStr = (m_plan.socketStrategy == SocketStrategy::Fixed) ? "fixed" : "scan";
    qInfo("compositor: launching '%s' %s (backend=%s strategy=%s socket=%s)",
          qPrintable(m_plan.binary),
          qPrintable(m_plan.args.join(QLatin1Char(' '))),
          qPrintable(backendName(m_plan.backend)), strategyStr,
          qPrintable(m_plan.socketName.isEmpty() ? QStringLiteral("(scan)") : m_plan.socketName));
    qInfo("compositor: log -> %s", kCompositorLogPath);

    m_compositorProcess.start();
    if (!m_compositorProcess.waitForStarted(5000)) {
        qCritical("compositor: failed to start '%s': %s",
                  qPrintable(m_plan.binary),
                  qPrintable(m_compositorProcess.errorString()));
        return false;
    }
    m_compositorLaunchTimeMs = QDateTime::currentMSecsSinceEpoch();
    qInfo("compositor: started (pid=%lld)", (long long)m_compositorProcess.processId());
    return true;
}

QString SessionBroker::detectWaylandSocket()
{
    const QString runtimeDir = QString::fromLocal8Bit(qgetenv("XDG_RUNTIME_DIR"));
    const qint64  startMs    = QDateTime::currentMSecsSinceEpoch();

    if (m_plan.socketStrategy == SocketStrategy::Fixed) {
        const QString socketPath = runtimeDir + QStringLiteral("/") + m_plan.socketName;
        qInfo("compositor: waiting for fixed socket '%s' (timeout %dms)…",
              qPrintable(socketPath), kCompositorSocketTimeoutMs);
        while (true) {
            if (m_compositorProcess.waitForFinished(0)
                || m_compositorProcess.state() != QProcess::Running) {
                qWarning("compositor: exited before socket became ready");
                return {};
            }
            if (isWaylandSocketReady(socketPath)) {
                const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - startMs;
                qInfo("compositor: socket '%s' ready after %lldms",
                      qPrintable(m_plan.socketName), (long long)elapsed);
                return m_plan.socketName;
            }
            if (QDateTime::currentMSecsSinceEpoch() - startMs >= kCompositorSocketTimeoutMs)
                break;
            QThread::msleep(50);
        }
        qCritical("compositor: socket '%s' not ready after %dms (file_exists=%s)",
                  qPrintable(m_plan.socketName), kCompositorSocketTimeoutMs,
                  QFileInfo::exists(socketPath) ? "yes (not connectable)" : "no");
        return {};
    }

    return scanNewWaylandSocket(runtimeDir, startMs);
}

QString SessionBroker::scanNewWaylandSocket(const QString &runtimeDir, qint64 startMs)
{
    qInfo("compositor: scanning '%s' for new wayland socket (timeout %dms)…",
          qPrintable(runtimeDir), kCompositorSocketTimeoutMs);
    const QDir dir(runtimeDir);
    while (true) {
        if (m_compositorProcess.waitForFinished(0)
            || m_compositorProcess.state() != QProcess::Running) {
            qWarning("compositor: exited before any wayland socket appeared");
            return {};
        }
        const QStringList entries = dir.entryList(
            QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot);
        for (const QString &entry : entries) {
            if (!entry.startsWith(QStringLiteral("wayland-"))) continue;
            if (m_plan.preExistingSockets.contains(entry)) continue;
            const QString socketPath = runtimeDir + QStringLiteral("/") + entry;
            if (isWaylandSocketReady(socketPath)) {
                const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - startMs;
                qInfo("compositor: found socket '%s' after %lldms (scan)",
                      qPrintable(entry), (long long)elapsed);
                return entry;
            }
        }
        if (QDateTime::currentMSecsSinceEpoch() - startMs >= kCompositorSocketTimeoutMs)
            break;
        QThread::msleep(50);
    }
    qCritical("compositor: no new wayland socket found after %dms (scan strategy)",
              kCompositorSocketTimeoutMs);
    return {};
}

// ── Shell launch ──────────────────────────────────────────────────────────────

QProcessEnvironment SessionBroker::buildShellEnvironment() const
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    env.insert(QStringLiteral("WAYLAND_DISPLAY"),      m_activeWaylandDisplay);
    env.insert(QStringLiteral("XDG_SESSION_TYPE"),     QStringLiteral("wayland"));
    env.insert(QStringLiteral("XDG_CURRENT_DESKTOP"),  QStringLiteral("SLM"));
    env.insert(QStringLiteral("QT_QPA_PLATFORM"),      QStringLiteral("wayland"));
    env.insert(QStringLiteral("SLM_OFFICIAL_SESSION"), QStringLiteral("1"));
    env.insert(QStringLiteral("SLM_SESSION_MODE"),     startupModeToString(m_finalMode));

    if (env.value(QStringLiteral("GDK_BACKEND")).isEmpty())
        env.insert(QStringLiteral("GDK_BACKEND"),        QStringLiteral("wayland,x11"));
    if (env.value(QStringLiteral("MOZ_ENABLE_WAYLAND")).isEmpty())
        env.insert(QStringLiteral("MOZ_ENABLE_WAYLAND"), QStringLiteral("1"));
    if (env.value(QStringLiteral("LIBSEAT_BACKEND")).isEmpty())
        env.insert(QStringLiteral("LIBSEAT_BACKEND"),    QStringLiteral("logind"));

    return env;
}

bool SessionBroker::launchShell(QString *failureReason)
{
    if (failureReason) {
        failureReason->clear();
    }
    QFile::remove(lifecycleFilePath());

    const QProcessEnvironment shellEnv = buildShellEnvironment();
    qInfo("shell env: WAYLAND_DISPLAY=%s QT_QPA_PLATFORM=%s XDG_SESSION_TYPE=%s "
          "SLM_SESSION_MODE=%s",
          qPrintable(shellEnv.value(QStringLiteral("WAYLAND_DISPLAY"))),
          qPrintable(shellEnv.value(QStringLiteral("QT_QPA_PLATFORM"))),
          qPrintable(shellEnv.value(QStringLiteral("XDG_SESSION_TYPE"))),
          qPrintable(shellEnv.value(QStringLiteral("SLM_SESSION_MODE"))));

    if (m_finalMode == StartupMode::Recovery)
        m_shellProcess.setProgram(QStringLiteral("slm-recovery-app"));
    else
        m_shellProcess.setProgram(m_config.shell());

    m_shellProcess.setArguments(
        m_finalMode == StartupMode::Recovery ? QStringList{} : m_config.shellArgs());
    m_shellProcess.setProcessEnvironment(shellEnv);
    m_shellProcess.setProcessChannelMode(QProcess::MergedChannels);
    m_shellLogStartOffset = QFileInfo(QString::fromLatin1(kShellLogPath)).size();
    m_shellProcess.setStandardOutputFile(
        QString::fromLatin1(kShellLogPath), QIODeviceBase::Append);

    m_shellProcess.start();
    if (!m_shellProcess.waitForStarted(5000)) {
        const QString reason = QStringLiteral("shell-launch-failed:%1:%2")
                                   .arg(m_shellProcess.program(),
                                        m_shellProcess.errorString());
        qWarning("shell: failed to start '%s': %s",
                 qPrintable(m_shellProcess.program()),
                 qPrintable(m_shellProcess.errorString()));
        if (failureReason) {
            *failureReason = reason;
        }
        return false;
    }
    m_shellLaunchTimeMs = QDateTime::currentMSecsSinceEpoch();
    qInfo("shell: '%s' started (pid=%lld)",
          qPrintable(m_shellProcess.program()),
          (long long)m_shellProcess.processId());
    return true;
}

bool SessionBroker::launchWatchdog(QString *failureReason)
{
    if (failureReason) {
        failureReason->clear();
    }
    qint64 pid = 0;
    if (QProcess::startDetached(QStringLiteral("slm-watchdog"), {}, QString(), &pid)) {
        qInfo("slm-session-broker: watchdog started (pid=%lld)", (long long)pid);
        return true;
    }
    const QString reason = QStringLiteral("watchdog-launch-failed:slm-watchdog");
    qWarning("slm-session-broker: failed to start slm-watchdog");
    if (failureReason) {
        *failureReason = reason;
    }
    return false;
}

QString SessionBroker::monitorSession()
{
    auto watchdogMarkedHealthy = []() {
        SessionState state;
        QString err;
        return SessionStateIO::load(state, err)
                && state.lastBootStatus == QStringLiteral("healthy");
    };

    while (m_compositorProcess.state() == QProcess::Running) {
        if (m_compositorProcess.waitForFinished(250)) {
            break;
        }

        if (m_shellProcess.waitForFinished(0)
            || m_shellProcess.state() == QProcess::NotRunning) {
            const bool shellRendered = readLifecycleMarker(QStringLiteral("firstFrame"));
            const bool healthy = watchdogMarkedHealthy();
            const qint64 uptimeMs = m_shellLaunchTimeMs > 0
                    ? QDateTime::currentMSecsSinceEpoch() - m_shellLaunchTimeMs
                    : -1;
            const bool shellFailed = !healthy
                    && (m_shellProcess.exitStatus() == QProcess::CrashExit
                        || m_shellProcess.exitCode() != 0
                        || (uptimeMs >= 0 && uptimeMs < kShellFastExitMs)
                        || !shellRendered);

            const QString reason = shellFailed
                    ? shellExitReason(QStringLiteral("shell-ended-before-session-healthy"))
                    : QString{};
            if (shellFailed) {
                qCritical("session: shell failure detected: %s", qUtf8Printable(reason));
            } else {
                qInfo("session: shell exited after healthy proof (uptime=%lldms first_frame=%s)",
                      (long long)uptimeMs, shellRendered ? "yes" : "no");
            }
            terminateCompositor(reason.isEmpty() ? QStringLiteral("shell-ended") : reason);
            return reason;
        }

        if (m_shellLaunchTimeMs > 0
            && !readLifecycleMarker(QStringLiteral("firstFrame"))
            && !watchdogMarkedHealthy()
            && QDateTime::currentMSecsSinceEpoch() - m_shellLaunchTimeMs
                    >= kShellFirstFrameTimeoutMs) {
            const QString reason = QStringLiteral("shell-first-frame-timeout:%1ms")
                                       .arg(kShellFirstFrameTimeoutMs);
            qCritical("session: shell did not render first frame within %dms",
                      kShellFirstFrameTimeoutMs);
            terminateCompositor(reason);
            return reason;
        }
    }
    return {};
}

QString SessionBroker::monitorRecoverySession()
{
    while (m_compositorProcess.state() == QProcess::Running) {
        if (m_compositorProcess.waitForFinished(500)) {
            break;
        }
        if (m_shellProcess.waitForFinished(0)
            || m_shellProcess.state() == QProcess::NotRunning) {
            const bool failed = m_shellProcess.exitStatus() == QProcess::CrashExit
                    || m_shellProcess.exitCode() != 0;
            const QString reason = failed
                    ? shellExitReason(QStringLiteral("recovery-shell-exited-abnormally"))
                    : QString{};
            if (failed) {
                qCritical("session: recovery shell failure detected: %s",
                          qUtf8Printable(reason));
            } else {
                qInfo("compositor: recovery shell exited (code=%d) — terminating compositor",
                      m_shellProcess.exitCode());
            }
            terminateCompositor(reason.isEmpty() ? QStringLiteral("recovery-shell-ended")
                                                 : reason);
            return reason;
        }
    }
    return {};
}

void SessionBroker::terminateCompositor(const QString &reason)
{
    if (m_compositorProcess.state() == QProcess::NotRunning) {
        return;
    }
    m_compositorStopRequested = true;
    m_compositorStopReason = reason;
    qInfo("compositor: terminating (reason=%s)", qUtf8Printable(reason));
    m_compositorProcess.terminate();
    if (!m_compositorProcess.waitForFinished(3000)) {
        qWarning("compositor: SIGTERM ignored — sending SIGKILL");
        m_compositorProcess.kill();
        m_compositorProcess.waitForFinished(2000);
    }
}

QString SessionBroker::compositorExitReason(const QString &prefix) const
{
    const qint64 uptimeMs = m_compositorLaunchTimeMs > 0
            ? QDateTime::currentMSecsSinceEpoch() - m_compositorLaunchTimeMs
            : -1;
    QString reason = QStringLiteral("%1:%2 exit_status=%3 exit_code=%4 uptime_ms=%5")
                         .arg(prefix,
                              m_plan.binary,
                              processExitStatusName(m_compositorProcess.exitStatus()),
                              QString::number(m_compositorProcess.exitCode()),
                              QString::number(uptimeMs));
    const QString hint = compositorLogHint();
    if (!hint.isEmpty()) {
        reason += QStringLiteral(" ") + hint;
    }
    if (!m_compositorStopReason.isEmpty()) {
        reason += QStringLiteral(" stop_reason=") + m_compositorStopReason;
    }
    return reason;
}

QString SessionBroker::shellExitReason(const QString &prefix) const
{
    const qint64 uptimeMs = m_shellLaunchTimeMs > 0
            ? QDateTime::currentMSecsSinceEpoch() - m_shellLaunchTimeMs
            : -1;
    return QStringLiteral("%1:%2 exit_status=%3 exit_code=%4 uptime_ms=%5 first_frame=%6")
            .arg(prefix,
                 m_shellProcess.program(),
                 processExitStatusName(m_shellProcess.exitStatus()),
                 QString::number(m_shellProcess.exitCode()),
                 QString::number(uptimeMs),
                 readLifecycleMarker(QStringLiteral("firstFrame")) ? QStringLiteral("yes")
                                                                   : QStringLiteral("no"));
}

QString SessionBroker::compositorLogHint() const
{
    const QByteArray data = readFileFromOffset(QString::fromLatin1(kCompositorLogPath),
                                               m_compositorLogStartOffset);
    const QString lower = QString::fromLocal8Bit(data).toLower();
    if (lower.isEmpty()) {
        return {};
    }
    if ((lower.contains(QStringLiteral("permission denied"))
         || lower.contains(QStringLiteral("operation not permitted")))
            && lower.contains(QStringLiteral("/dev/dri"))) {
        return QStringLiteral("hint=kwin-drm-permission-denied");
    }
    if (lower.contains(QStringLiteral("failed to open drm"))
            || lower.contains(QStringLiteral("could not open drm"))
            || lower.contains(QStringLiteral("no drm"))
            || lower.contains(QStringLiteral("render node"))) {
        return QStringLiteral("hint=kwin-drm-node-unavailable");
    }
    if (lower.contains(QStringLiteral("session not active"))
            || (lower.contains(QStringLiteral("logind"))
                && lower.contains(QStringLiteral("seat")))) {
        return QStringLiteral("hint=logind-seat-session-not-active");
    }
    if (lower.contains(QStringLiteral("qt.qpa.plugin"))
            || lower.contains(QStringLiteral("could not load the qt platform plugin"))) {
        return QStringLiteral("hint=qt-platform-plugin-load-failed");
    }
    if (lower.contains(QStringLiteral("failed to create backend"))
            || lower.contains(QStringLiteral("no backend"))) {
        return QStringLiteral("hint=kwin-backend-create-failed");
    }
    return {};
}

void SessionBroker::logFileTail(const QString &label, const QString &path, qint64 offset) const
{
    const QByteArray data = readFileFromOffset(path, offset);
    const QStringList lines = tailLines(data, 30);
    if (lines.isEmpty()) {
        qInfo("%s: no new log output in %s", qPrintable(label), qPrintable(path));
        return;
    }
    qInfo("%s: last %d new lines from %s:",
          qPrintable(label), static_cast<int>(lines.size()), qPrintable(path));
    for (const QString &line : lines) {
        qInfo("  | %s", qUtf8Printable(line));
    }
}

// ── Lifecycle helpers ─────────────────────────────────────────────────────────

QString SessionBroker::lifecycleFilePath() const
{
    const QString runtimeDir = QString::fromLocal8Bit(qgetenv("XDG_RUNTIME_DIR"));
    if (!runtimeDir.isEmpty())
        return runtimeDir + QStringLiteral("/slm-shell-lifecycle.json");
    return QStringLiteral("/tmp/slm-shell-lifecycle.json");
}

bool SessionBroker::readLifecycleMarker(const QString &phase) const
{
    QFile f(lifecycleFilePath());
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
    return !obj.value(phase).toString().isEmpty();
}

QString SessionBroker::crashReportFilePath() const
{
    return ConfigManager::configDir() + QStringLiteral("/last-crash.json");
}

void SessionBroker::writeCrashReport(const QString &phase, const QString &reason) const
{
    if (reason.isEmpty()) {
        return;
    }

    QJsonObject compositor;
    compositor.insert(QStringLiteral("binary"), m_plan.binary);
    compositor.insert(QStringLiteral("args"), QJsonArray::fromStringList(m_plan.args));
    compositor.insert(QStringLiteral("backend"), backendName(m_plan.backend));
    compositor.insert(QStringLiteral("exit_status"),
                      processExitStatusName(m_compositorProcess.exitStatus()));
    compositor.insert(QStringLiteral("exit_code"), m_compositorProcess.exitCode());
    compositor.insert(QStringLiteral("stop_requested"), m_compositorStopRequested);
    compositor.insert(QStringLiteral("stop_reason"), m_compositorStopReason);

    QJsonObject shell;
    shell.insert(QStringLiteral("binary"), m_shellProcess.program());
    shell.insert(QStringLiteral("args"), QJsonArray::fromStringList(m_shellProcess.arguments()));
    shell.insert(QStringLiteral("exit_status"),
                 processExitStatusName(m_shellProcess.exitStatus()));
    shell.insert(QStringLiteral("exit_code"), m_shellProcess.exitCode());
    shell.insert(QStringLiteral("first_frame"),
                 readLifecycleMarker(QStringLiteral("firstFrame")));

    QJsonObject logs;
    logs.insert(QStringLiteral("broker"), QStringLiteral("/tmp/slm-session-broker.log"));
    logs.insert(QStringLiteral("compositor"), QString::fromLatin1(kCompositorLogPath));
    logs.insert(QStringLiteral("shell"), QString::fromLatin1(kShellLogPath));

    QJsonObject obj;
    obj.insert(QStringLiteral("timestamp"),
               QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    obj.insert(QStringLiteral("phase"), phase);
    obj.insert(QStringLiteral("reason"), reason);
    obj.insert(QStringLiteral("mode"), startupModeToString(m_finalMode));
    obj.insert(QStringLiteral("requested_mode"), startupModeToString(m_requestedMode));
    obj.insert(QStringLiteral("crash_count"), m_state.crashCount);
    obj.insert(QStringLiteral("wayland_display"), m_activeWaylandDisplay);
    obj.insert(QStringLiteral("compositor"), compositor);
    obj.insert(QStringLiteral("shell"), shell);
    obj.insert(QStringLiteral("logs"), logs);

    const QString path = crashReportFilePath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        qWarning("slm-session-broker: failed to create crash-report directory: %s",
                 qUtf8Printable(QFileInfo(path).absolutePath()));
        return;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning("slm-session-broker: failed to open crash report: %s",
                 qUtf8Printable(path));
        return;
    }
    const QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size()) {
        file.cancelWriting();
        qWarning("slm-session-broker: failed to write complete crash report: %s",
                 qUtf8Printable(path));
        return;
    }
    if (!file.commit()) {
        qWarning("slm-session-broker: failed to commit crash report: %s",
                 qUtf8Printable(path));
        return;
    }
    qCritical("CRASH_REASON phase=%s reason=%s report=%s",
              qUtf8Printable(phase), qUtf8Printable(reason), qUtf8Printable(path));
}

// ── State helpers ─────────────────────────────────────────────────────────────

void SessionBroker::writeStartupFailed(const QString &reason)
{
    QString err;
    m_state.lastBootStatus = QStringLiteral("crashed");
    m_state.recoveryReason = reason;
    m_state.lastCrashReason = reason;
    m_state.lastUpdated    = QDateTime::currentDateTimeUtc();
    writeCrashReport(QStringLiteral("startup"), reason);
    if (!SessionStateIO::save(m_state, err))
        qWarning("slm-session-broker: failed to save startup-failed state: %s",
                 qUtf8Printable(err));
}

void SessionBroker::writeSessionEnded(const QString &crashReason)
{
    QString err;
    SessionStateIO::load(m_state, err); // reload — watchdog may have updated
    const bool sessionCrashed = !crashReason.isEmpty();

    const bool shellReachedFirstFrame = readLifecycleMarker(QStringLiteral("firstFrame"));
    const qint64 shellUptimeMs = (m_shellLaunchTimeMs > 0)
        ? QDateTime::currentMSecsSinceEpoch() - m_shellLaunchTimeMs : -1;

    const int shellCode = m_shellProcess.exitCode();
    const bool shellCrash = (m_shellProcess.exitStatus() == QProcess::CrashExit);
    qInfo("shell: exit_code=%d crashed=%s uptime=%lldms first_frame=%s",
          shellCode, shellCrash ? "yes" : "no",
          (long long)shellUptimeMs, shellReachedFirstFrame ? "yes" : "no");

    qInfo("session: ended — crashed=%s mode=%s watchdog_status=%s crashCount=%d reason=%s",
          sessionCrashed ? "yes" : "no",
          qPrintable(startupModeToString(m_finalMode)),
          qPrintable(m_state.lastBootStatus),
          m_state.crashCount,
          qPrintable(sessionCrashed ? crashReason : QStringLiteral("<none>")));

    if (sessionCrashed) {
        m_state.lastBootStatus = QStringLiteral("crashed");
        m_state.recoveryReason = crashReason;
        m_state.lastCrashReason = crashReason;
        writeCrashReport(QStringLiteral("session"), crashReason);
    } else if (m_state.lastBootStatus != QStringLiteral("healthy")) {
        m_state.lastBootStatus = QStringLiteral("ended");
    }

    // Normal/safe: reset crash counter if shell rendered at least one frame.
    if (!sessionCrashed && shellReachedFirstFrame
        && m_state.lastBootStatus != QStringLiteral("healthy")) {
        qInfo("session: shell rendered first frame — resetting crash counter %d → 0",
              m_state.crashCount);
        m_state.crashCount = 0;
        m_state.recoveryReason.clear();
        m_state.lastCrashReason.clear();
    }

    // Recovery: if the user exits recovery cleanly, allow the next broker run to
    // try normal mode again even if no watchdog health update was written.
    if (m_finalMode == StartupMode::Recovery && !sessionCrashed
        && m_state.lastBootStatus != QStringLiteral("healthy")) {
        qInfo("session: recovery mode ended cleanly — resetting crash counter %d → 0",
              m_state.crashCount);
        m_state.crashCount = 0;
        m_state.recoveryReason.clear();
        m_state.lastCrashReason.clear();
    }

    qInfo("session: saving — lastBootStatus=%s crashCount=%d",
          qPrintable(m_state.lastBootStatus), m_state.crashCount);

    m_state.lastUpdated = QDateTime::currentDateTimeUtc();
    if (!SessionStateIO::save(m_state, err))
        qWarning("slm-session-broker: failed to save session-ended state: %s",
                 qUtf8Printable(err));
}

void SessionBroker::validateLogindSession()
{
    const QString sessionId = QString::fromLocal8Bit(qgetenv("XDG_SESSION_ID"));
    if (sessionId.isEmpty()) {
        qWarning("SLM-SESSION: XDG_SESSION_ID not set — pam_systemd.so may not have run");
        qWarning("SLM-SESSION: ensure /etc/pam.d/slm (or /etc/pam.d/greetd) contains:"
                 " 'session required pam_systemd.so'");
    } else {
        qInfo("SLM-SESSION: XDG_SESSION_ID=%s", qPrintable(sessionId));
    }

    qInfo("SLM-SESSION: env snapshot:");
    for (const char *key : {"XDG_SESSION_ID", "XDG_SESSION_TYPE", "XDG_SESSION_CLASS",
                             "XDG_SEAT", "XDG_VTNR", "XDG_RUNTIME_DIR",
                             "XDG_CURRENT_DESKTOP", "DBUS_SESSION_BUS_ADDRESS",
                             "LIBSEAT_BACKEND"}) {
        const QByteArray val = qgetenv(key);
        qInfo("  %s=%s", key, val.isEmpty() ? "<unset>" : val.constData());
    }

    if (!sessionId.isEmpty()) {
        QProcess lc;
        lc.start(QStringLiteral("loginctl"),
                 {QStringLiteral("show-session"), sessionId,
                  QStringLiteral("-p"), QStringLiteral("Type"),
                  QStringLiteral("-p"), QStringLiteral("Class"),
                  QStringLiteral("-p"), QStringLiteral("Seat"),
                  QStringLiteral("-p"), QStringLiteral("Active"),
                  QStringLiteral("-p"), QStringLiteral("Remote"),
                  QStringLiteral("-p"), QStringLiteral("Service")});
        if (lc.waitForFinished(3000)) {
            const QString out = QString::fromLocal8Bit(lc.readAllStandardOutput()).trimmed();
            qInfo("SLM-SESSION: loginctl show-session %s:\n%s",
                  qPrintable(sessionId), qPrintable(out));
            if (!out.contains(QStringLiteral("Seat=seat0")))
                qWarning("SLM-SESSION: session does NOT have Seat=seat0 — "
                         "kwin_wayland will fail to open DRM node");
            if (out.contains(QStringLiteral("Remote=yes")))
                qWarning("SLM-SESSION: session is marked Remote=yes — "
                         "compositor cannot access DRM from a remote session");
            if (!out.contains(QStringLiteral("Type=wayland")))
                qWarning("SLM-SESSION: session Type is not 'wayland' — "
                         "set XDG_SESSION_TYPE=wayland in PAM env before pam_open_session");
        } else {
            qWarning("SLM-SESSION: loginctl show-session failed or timed out");
        }
    }

    QProcess ls;
    ls.start(QStringLiteral("ls"), {QStringLiteral("-la"), QStringLiteral("/dev/dri")});
    if (ls.waitForFinished(2000))
        qInfo("SLM-SESSION: /dev/dri:\n%s",
              ls.readAllStandardOutput().trimmed().constData());
}

} // namespace Slm::Login
