#include "sessionwatchdog.h"

#include <QCoreApplication>
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
    markSessionHealthy();
    QCoreApplication::quit();
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
    state.lastBootStatus = QStringLiteral("healthy");
    state.recoveryReason.clear();
    state.lastUpdated    = QDateTime::currentDateTimeUtc();

    if (!SessionStateIO::save(state, err)) {
        qWarning("slm-watchdog: could not save healthy state: %s", qUtf8Printable(err));
    } else {
        qInfo("slm-watchdog: session marked healthy (crash_count reset to 0)");
    }

    // Promote current config as last known good.
    ConfigManager config;
    config.load();
    if (!config.promoteToLastGood(&err)) {
        qWarning("slm-watchdog: could not promote config to last-good: %s",
                 qUtf8Printable(err));
    } else {
        qInfo("slm-watchdog: config promoted to safe baseline");
    }
}

} // namespace Slm::Login
