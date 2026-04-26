#pragma once
#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSet>
#include "../libslmlogin/slmconfigmanager.h"
#include "../libslmlogin/slmlogindefs.h"
#include "../libslmlogin/slmplatformcheck.h"
#include "../libslmlogin/slmsessionstate.h"

namespace Slm::Login {

enum class CompositorBackend { KWin, Sway, Unknown };
enum class SocketStrategy    { Fixed, Scan };

struct CompositorLaunchPlan {
    QString             binary;
    QStringList         args;
    QProcessEnvironment env;
    CompositorBackend   backend        = CompositorBackend::Unknown;
    SocketStrategy      socketStrategy = SocketStrategy::Scan;
    QString             socketName;               // expected socket name (Fixed strategy)
    QSet<QString>       preExistingSockets;        // snapshot before launch (Scan strategy)
};

class SessionBroker : public QObject
{
    Q_OBJECT
public:
    explicit SessionBroker(StartupMode requestedMode, QObject *parent = nullptr);

    // Blocking: returns 0 on clean session end, non-zero on failure.
    int run();

private:
    void                readState();
    StartupMode         evaluateMode(const PlatformStatus &platform);
    void                performRollback(StartupMode mode);
    void                prepareEnvironment(StartupMode mode);
    bool                hasRecoveryPartitionRequest(QString *reason = nullptr) const;
    QString             preflightMissingComponentReason() const;

    CompositorLaunchPlan buildCompositorPlan();
    static QString       kwinGetCustomSocketFlag(const QString &bin);
    void                 removeStaleSlmSockets();
    bool                 launchCompositor();
    QString              detectWaylandSocket();
    QString              scanNewWaylandSocket(const QString &runtimeDir, qint64 startMs);

    QProcessEnvironment  buildShellEnvironment() const;
    void                 launchShell();
    void                 launchWatchdog();

    QString              lifecycleFilePath() const;
    bool                 readLifecycleMarker(const QString &phase) const;

    void                 validateLogindSession();
    void                 writeStartupFailed(const QString &reason);
    void                 writeSessionEnded(bool compositorCrashed);

    StartupMode          m_requestedMode;
    StartupMode          m_finalMode             = StartupMode::Normal;
    qint64               m_shellLaunchTimeMs     = 0;
    SessionState         m_state;
    ConfigManager        m_config;
    CompositorLaunchPlan m_plan;
    QString              m_activeWaylandDisplay;
    QProcess             m_compositorProcess;
    QProcess             m_shellProcess;
};

} // namespace Slm::Login
