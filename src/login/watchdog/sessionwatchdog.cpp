#include "sessionwatchdog.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include "src/login/libslmlogin/slmconfigmanager.h"
#include "src/login/libslmlogin/slmlogindefs.h"
#include "src/login/libslmlogin/slmsessionstate.h"

namespace Slm::Login {

SessionWatchdog::SessionWatchdog(QObject *parent)
    : QObject(parent)
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(kHealthySessionSeconds * 1000);
    connect(&m_timer, &QTimer::timeout, this, &SessionWatchdog::onHealthyTimeout);
    m_timer.start();

    qInfo("slm-watchdog: started — will mark session healthy in %ds",
          kHealthySessionSeconds);
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
    const QString runtimeDir = QString::fromLocal8Bit(qgetenv("XDG_RUNTIME_DIR"));
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

    // Minimal shell liveness check (best effort): shell process name should exist.
    if (QStandardPaths::findExecutable(QStringLiteral("pgrep")).isEmpty()) {
        if (reason) *reason = QStringLiteral("missing-component:pgrep");
        return false;
    }

    QProcess pgrep;
    pgrep.start(QStringLiteral("pgrep"), {QStringLiteral("-x"), QStringLiteral("appSlm_Desktop")});
    if (!pgrep.waitForFinished(1500)) {
        if (reason) *reason = QStringLiteral("pgrep timeout");
        return false;
    }
    if (pgrep.exitCode() != 0) {
        // fallback for renamed binary in install.
        QProcess pgrepAlt;
        pgrepAlt.start(QStringLiteral("pgrep"), {QStringLiteral("-x"), QStringLiteral("slm-shell")});
        if (!pgrepAlt.waitForFinished(1500) || pgrepAlt.exitCode() != 0) {
            if (reason) *reason = QStringLiteral("shell process not found");
            return false;
        }
    }
    return true;
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
