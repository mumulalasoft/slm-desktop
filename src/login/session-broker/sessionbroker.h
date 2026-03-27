#pragma once
#include <QObject>
#include <QProcess>
#include "src/login/libslmlogin/slmconfigmanager.h"
#include "src/login/libslmlogin/slmlogindefs.h"
#include "src/login/libslmlogin/slmplatformcheck.h"
#include "src/login/libslmlogin/slmsessionstate.h"

namespace Slm::Login {

class SessionBroker : public QObject
{
    Q_OBJECT
public:
    explicit SessionBroker(StartupMode requestedMode, QObject *parent = nullptr);

    // Blocking: returns 0 on clean session end, non-zero on failure.
    int run();

private:
    void         readState();
    StartupMode  evaluateMode(const PlatformStatus &platform);
    void         performRollback(StartupMode mode);
    void         prepareEnvironment(StartupMode mode);
    bool         waitCompositorSocket();
    bool         launchCompositor();
    void         launchShell();
    void         launchWatchdog();
    void         writeStartupFailed(const QString &reason);
    void         writeSessionEnded(bool crashed);

    StartupMode   m_requestedMode;
    StartupMode   m_finalMode = StartupMode::Normal;
    SessionState  m_state;
    ConfigManager m_config;
    QProcess      m_compositorProcess;
};

} // namespace Slm::Login
