#include "sessionwatchdog.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include "../libslmlogin/slmconfigmanager.h"
#include "../libslmlogin/slmlogindefs.h"
#include "../libslmlogin/slmsessionstate.h"

namespace Slm::Login {

SessionWatchdog::SessionWatchdog(QObject *parent)
    : QObject(parent)
{
    int healthyTimeoutMs = kHealthySessionSeconds * 1000;
    bool ok = false;
    const int configuredTimeoutMs = qEnvironmentVariableIntValue(
        "SLM_WATCHDOG_HEALTHY_TIMEOUT_MS", &ok);
    if (ok && configuredTimeoutMs > 0) {
        healthyTimeoutMs = configuredTimeoutMs;
    }

    m_timer.setSingleShot(true);
    m_timer.setInterval(healthyTimeoutMs);
    connect(&m_timer, &QTimer::timeout, this, &SessionWatchdog::onHealthyTimeout);
    m_timer.start();

    qInfo("slm-watchdog: started — will mark session healthy in %dms",
          healthyTimeoutMs);
}

void SessionWatchdog::onHealthyTimeout()
{
    QString reason;
    if (!isSessionHealthy(&reason)) {
        qWarning("slm-watchdog: session unhealthy, skip healthy mark: %s",
                 qUtf8Printable(reason));
        QCoreApplication::quit();
        return;
    }
    markSessionHealthy();
    QCoreApplication::quit();
}

bool SessionWatchdog::isSessionHealthy(QString *reason) const
{
    const QString runtimeDir     = QString::fromLocal8Bit(qgetenv("XDG_RUNTIME_DIR"));
    const QString waylandDisplay = QString::fromLocal8Bit(qgetenv("WAYLAND_DISPLAY"));
    if (runtimeDir.isEmpty() || waylandDisplay.isEmpty()) {
        if (reason) *reason = QStringLiteral("WAYLAND_DISPLAY or XDG_RUNTIME_DIR missing");
        return false;
    }
    const QString socketPath = runtimeDir + QStringLiteral("/") + waylandDisplay;
    if (!QFileInfo::exists(socketPath)) {
        if (reason) *reason = QStringLiteral("wayland socket missing: ") + socketPath;
        return false;
    }

    // Lifecycle file written by slm-desktop and slm-recovery-app.
    const QString lifecyclePath = runtimeDir + QStringLiteral("/slm-shell-lifecycle.json");

    const QString sessionMode = QString::fromLocal8Bit(qgetenv("SLM_SESSION_MODE"));
    if (sessionMode == QStringLiteral("recovery")) {
        // Recovery app must have rendered its first frame AND kept it visible for
        // at least kRecoveryFirstFrameStableMs — proof the UI was actually usable,
        // not just a flash before a crash.
        QFile f(lifecyclePath);
        if (f.open(QIODevice::ReadOnly)) {
            const QJsonObject lc = QJsonDocument::fromJson(f.readAll()).object();
            const QString ts = lc.value(QStringLiteral("firstFrame")).toString();
            if (!ts.isEmpty()) {
                const QDateTime t      = QDateTime::fromString(ts, Qt::ISODateWithMs);
                const qint64    ageMs  = t.msecsTo(QDateTime::currentDateTimeUtc());
                if (ageMs >= kRecoveryFirstFrameStableMs) {
                    qInfo("slm-watchdog: recovery firstFrame stable for %lldms (>=%dms) — healthy",
                          (long long)ageMs, kRecoveryFirstFrameStableMs);
                    return true;
                }
                qInfo("slm-watchdog: recovery firstFrame present but only %lldms old (need >=%dms)",
                      (long long)ageMs, kRecoveryFirstFrameStableMs);
                if (reason) *reason = QStringLiteral("recovery firstFrame not stable yet");
                return false;
            }
        }
        qInfo("slm-watchdog: recovery mode — no firstFrame marker written yet");
        if (reason) *reason = QStringLiteral("recovery firstFrame not written");
        return false;
    }

    // Normal/safe mode: shell must have rendered at least one frame.
    if (QFileInfo::exists(lifecyclePath)) {
        QFile f(lifecyclePath);
        if (f.open(QIODevice::ReadOnly)) {
            const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
            if (!obj.value(QStringLiteral("firstFrame")).toString().isEmpty()) {
                qInfo("slm-watchdog: lifecycle marker confirms first frame rendered");
                return true;
            }
        }
    }

    // Fallback: pgrep for the shell process (slow hardware may not have written
    // the lifecycle marker yet within the watchdog window).
    if (!QStandardPaths::findExecutable(QStringLiteral("pgrep")).isEmpty()) {
        auto pgrepExists = [](const QString &name) -> bool {
            QProcess p;
            p.start(QStringLiteral("pgrep"), {QStringLiteral("-x"), name});
            return p.waitForFinished(1500) && p.exitCode() == 0;
        };
        if (pgrepExists(QStringLiteral("appSlm_Desktop"))
            || pgrepExists(QStringLiteral("slm-shell"))) {
            qInfo("slm-watchdog: shell confirmed alive via pgrep (no first-frame marker yet)");
            return true;
        }
    }

    if (reason) *reason = QStringLiteral("shell process not found and no first-frame marker");
    return false;
}

void SessionWatchdog::markSessionHealthy()
{
    QString err;
    SessionState state;
    if (!SessionStateIO::load(state, err)) {
        qWarning("slm-watchdog: could not load state: %s", qUtf8Printable(err));
    }

    state.crashCount     = 0;
    state.configPending  = false;
    state.safeModeForced = false;
    state.lastBootStatus = QStringLiteral("healthy");
    state.recoveryReason.clear();
    state.lastCrashReason.clear();
    state.lastUpdated    = QDateTime::currentDateTimeUtc();

    ConfigManager config;
    config.load();

    // Capture immutable snapshot id for crash-loop rollback path.
    const QString snapshotId = config.snapshot(&err);
    if (snapshotId.isEmpty()) {
        qWarning("slm-watchdog: could not snapshot last-good config: %s",
                 qUtf8Printable(err));
    } else {
        state.lastGoodSnapshot = snapshotId;
        qInfo("slm-watchdog: last_good_snapshot=%s", qUtf8Printable(snapshotId));
    }

    // Promote current config as safe baseline.
    if (!config.promoteToLastGood(&err)) {
        qWarning("slm-watchdog: could not promote config to last-good: %s",
                 qUtf8Printable(err));
    } else {
        qInfo("slm-watchdog: config promoted to safe baseline");
    }

    if (!SessionStateIO::save(state, err)) {
        qWarning("slm-watchdog: could not save healthy state: %s", qUtf8Printable(err));
    } else {
        qInfo("slm-watchdog: session marked healthy (crash_count reset to 0)");
    }

    const QString markerPath = ConfigManager::configDir()
            + QStringLiteral("/recovery-partition-request.json");
    if (QFileInfo::exists(markerPath) && !QFile::remove(markerPath)) {
        qWarning("slm-watchdog: could not clear recovery marker: %s",
                 qUtf8Printable(markerPath));
    }
}

} // namespace Slm::Login
