#include "sessionbroker.h"
#include "sessionrollback.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QThread>
#include <sys/types.h>
#include <unistd.h>

namespace Slm::Login {

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

    // Ensure core session env exists before integrity checks.
    qputenv("SLM_OFFICIAL_SESSION", "1");
    qputenv("XDG_SESSION_TYPE", "wayland");
    qputenv("XDG_CURRENT_DESKTOP", "SLM");
    if (qgetenv("XDG_RUNTIME_DIR").isEmpty()) {
        const QString runtimeDir = QStringLiteral("/run/user/") + QString::number(::getuid());
        qputenv("XDG_RUNTIME_DIR", runtimeDir.toLocal8Bit());
    }

    const PlatformChecker checker;
    const PlatformStatus platform = checker.checkAll(m_config.compositor(), m_config.shell());
    if (!platform.ok) {
        qWarning("slm-session-broker: platform integrity issues:");
        for (const QString &issue : platform.issues) {
            qWarning("  - %s", qUtf8Printable(issue));
        }
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

    if (!launchCompositor()) {
        writeStartupFailed(QStringLiteral("compositor-launch-failed"));
        return 1;
    }

    if (!waitCompositorSocket()) {
        qCritical("slm-session-broker: compositor socket did not appear within timeout");
        m_compositorProcess.kill();
        m_compositorProcess.waitForFinished(3000);
        writeStartupFailed(QStringLiteral("compositor-socket-timeout"));
        return 1;
    }

    launchShell();
    launchWatchdog();

    qInfo("slm-session-broker: waiting for compositor to exit…");
    m_compositorProcess.waitForFinished(-1);

    const int exitCode = m_compositorProcess.exitCode();
    const bool crashed = (m_compositorProcess.exitStatus() == QProcess::CrashExit)
                         || (exitCode != 0);
    writeSessionEnded(crashed);

    qInfo("slm-session-broker: session ended (exit=%d, crashed=%s)",
          exitCode, crashed ? "yes" : "no");
    return 0;
}

// ── Private ───────────────────────────────────────────────────────────────────

void SessionBroker::readState()
{
    QString err;
    if (!SessionStateIO::load(m_state, err)) {
        qWarning("slm-session-broker: state load error: %s (using defaults)", qUtf8Printable(err));
    }

    // Delayed-commit rollback: if previous session ended without health
    // confirmation and the config is still marked pending, roll it back now
    // before we increment the crash counter.
    if (m_state.configPending && m_state.lastBootStatus != QStringLiteral("healthy")) {
        QString err;
        if (m_config.hasPreviousConfig()) {
            if (m_config.rollbackToPrevious(&err)) {
                qInfo("slm-session-broker: config rolled back (configPending + unconfirmed session)");
            } else {
                qWarning("slm-session-broker: configPending rollback failed: %s", qUtf8Printable(err));
            }
        }
    }

    // Crash sentinel: increment now. Watchdog will reset on healthy session.
    m_state.crashCount++;
    m_state.lastBootStatus = QStringLiteral("started");
    m_state.lastMode       = m_requestedMode;
    m_state.lastUpdated    = QDateTime::currentDateTimeUtc();

    if (!SessionStateIO::save(m_state, err)) {
        qWarning("slm-session-broker: state save error: %s", qUtf8Printable(err));
    }
}

StartupMode SessionBroker::evaluateMode(const PlatformStatus &platform)
{
    // Priority: forced > crash loop > platform failure > requested > normal.
    if (m_state.safeModeForced) {
        return StartupMode::Safe;
    }
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

void SessionBroker::performRollback(StartupMode mode)
{
    QString err;
    switch (mode) {
    case StartupMode::Normal:
        break;
    case StartupMode::Safe:
        if (!m_config.rollbackToSafe(&err)) {
            qWarning("slm-session-broker: safe rollback failed: %s", qUtf8Printable(err));
        } else {
            qInfo("slm-session-broker: rolled back config to safe baseline");
        }
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

    if (qgetenv("XDG_RUNTIME_DIR").isEmpty()) {
        const QString runtimeDir = QStringLiteral("/run/user/") + QString::number(::getuid());
        qputenv("XDG_RUNTIME_DIR", runtimeDir.toLocal8Bit());
    }

    // Mark config as pending confirmation. Watchdog clears this on healthy session.
    // On next boot, if still pending + not healthy, we roll back the config.
    m_state.configPending = (mode == StartupMode::Normal);
    QString err;
    if (!SessionStateIO::save(m_state, err)) {
        qWarning("slm-session-broker: failed to persist configPending: %s", qUtf8Printable(err));
    }
}

QString SessionBroker::preflightMissingComponentReason() const
{
    const QFileInfo compositorInfo(m_config.compositor());
    if (!compositorInfo.exists() || !compositorInfo.isExecutable()) {
        return QStringLiteral("missing-component:compositor:%1").arg(m_config.compositor());
    }

    if (m_finalMode != StartupMode::Recovery) {
        const QFileInfo shellInfo(m_config.shell());
        if (!shellInfo.exists() || !shellInfo.isExecutable()) {
            return QStringLiteral("missing-component:shell:%1").arg(m_config.shell());
        }
    }

    if (QStandardPaths::findExecutable(QStringLiteral("slm-watchdog")).isEmpty()) {
        return QStringLiteral("missing-component:slm-watchdog");
    }

    if (m_finalMode == StartupMode::Recovery
            && QStandardPaths::findExecutable(QStringLiteral("slm-recovery-app")).isEmpty()) {
        return QStringLiteral("missing-component:slm-recovery-app");
    }

    return QString();
}

bool SessionBroker::launchCompositor()
{
    // Guard against stale socket from a previously crashed session causing
    // false "socket ready" detection.
    const QString runtimeDir = QString::fromLocal8Bit(qgetenv("XDG_RUNTIME_DIR"));
    const QString socketPath = runtimeDir + QStringLiteral("/wayland-0");
    if (!runtimeDir.isEmpty() && QFileInfo::exists(socketPath)) {
        if (QFile::remove(socketPath)) {
            qWarning("slm-session-broker: removed pre-existing compositor socket: %s",
                     qUtf8Printable(socketPath));
        }
    }

    m_compositorProcess.setProgram(m_config.compositor());
    m_compositorProcess.setArguments(m_config.compositorArgs());
    m_compositorProcess.setProcessChannelMode(QProcess::ForwardedChannels);
    m_compositorProcess.start();
    if (!m_compositorProcess.waitForStarted(5000)) {
        qCritical("slm-session-broker: failed to start compositor '%s': %s",
                  qUtf8Printable(m_config.compositor()),
                  qUtf8Printable(m_compositorProcess.errorString()));
        return false;
    }
    qInfo("slm-session-broker: compositor '%s' started (pid=%lld)",
          qUtf8Printable(m_config.compositor()),
          (long long)m_compositorProcess.processId());
    return true;
}

bool SessionBroker::waitCompositorSocket()
{
    const QString runtimeDir = QString::fromLocal8Bit(qgetenv("XDG_RUNTIME_DIR"));
    const QString socketPath = runtimeDir + QStringLiteral("/wayland-0");

    const int maxAttempts = kCompositorSocketTimeoutMs / 100;
    for (int i = 0; i < maxAttempts; ++i) {
        if (m_compositorProcess.state() != QProcess::Running) {
            qWarning("slm-session-broker: compositor exited before socket became ready");
            return false;
        }
        if (QFileInfo::exists(socketPath)) {
            qputenv("WAYLAND_DISPLAY", "wayland-0");
            qInfo("slm-session-broker: compositor socket ready after %dms", i * 100);
            return true;
        }
        QThread::msleep(100);
    }
    return false;
}

void SessionBroker::launchShell()
{
    // In Recovery mode, launch the recovery app instead of the normal shell.
    if (m_finalMode == StartupMode::Recovery) {
        qint64 pid = 0;
        if (QProcess::startDetached(QStringLiteral("slm-recovery-app"), {}, QString(), &pid)) {
            qInfo("slm-session-broker: recovery app started (pid=%lld)", (long long)pid);
        } else {
            qWarning("slm-session-broker: failed to start slm-recovery-app");
        }
        return;
    }

    qint64 pid = 0;
    const bool ok = QProcess::startDetached(m_config.shell(), m_config.shellArgs(), QString(), &pid);
    if (ok) {
        qInfo("slm-session-broker: shell '%s' started (pid=%lld)",
              qUtf8Printable(m_config.shell()), (long long)pid);
    } else {
        qWarning("slm-session-broker: failed to start shell '%s'",
                 qUtf8Printable(m_config.shell()));
    }
}

void SessionBroker::launchWatchdog()
{
    qint64 pid = 0;
    if (QProcess::startDetached(QStringLiteral("slm-watchdog"), {}, QString(), &pid)) {
        qInfo("slm-session-broker: watchdog started (pid=%lld)", (long long)pid);
    } else {
        qWarning("slm-session-broker: failed to start slm-watchdog");
    }
}

void SessionBroker::writeStartupFailed(const QString &reason)
{
    QString err;
    m_state.lastBootStatus = QStringLiteral("crashed");
    m_state.recoveryReason = reason;
    m_state.lastUpdated    = QDateTime::currentDateTimeUtc();
    if (!SessionStateIO::save(m_state, err)) {
        qWarning("slm-session-broker: failed to save startup-failed state: %s",
                 qUtf8Printable(err));
    }
}

void SessionBroker::writeSessionEnded(bool crashed)
{
    QString err;
    SessionStateIO::load(m_state, err); // reload — watchdog may have updated

    m_state.lastBootStatus = crashed ? QStringLiteral("crashed")
                                     : QStringLiteral("ended");
    m_state.lastUpdated    = QDateTime::currentDateTimeUtc();
    if (crashed) {
        // Guard: ensure crash_count is at least what we set at startup.
        // (Watchdog resets it on healthy sessions; if we reach here before
        //  watchdog fires, the counter stays elevated for next boot.)
    }
    if (!SessionStateIO::save(m_state, err)) {
        qWarning("slm-session-broker: failed to save session-ended state: %s",
                 qUtf8Printable(err));
    }
}

} // namespace Slm::Login
