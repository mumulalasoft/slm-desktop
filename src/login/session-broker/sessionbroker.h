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

enum class CompositorBackend { KWin, Unknown };
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
    bool                 launchShell(QString *failureReason = nullptr);
    bool                 launchWatchdog(QString *failureReason = nullptr);
    QString              monitorSession();
    QString              monitorRecoverySession();
    void                 terminateCompositor(const QString &reason);
    QString              compositorExitReason(const QString &prefix) const;
    QString              shellExitReason(const QString &prefix) const;
    QString              compositorLogHint() const;
    void                 logFileTail(const QString &label, const QString &path, qint64 offset) const;

    QString              lifecycleFilePath() const;
    bool                 readLifecycleMarker(const QString &phase) const;
    QString              crashReportFilePath() const;
    void                 writeCrashReport(const QString &phase, const QString &reason) const;

    void                 validateLogindSession();
    void                 writeStartupFailed(const QString &reason);
    void                 writeSessionEnded(const QString &crashReason);

    StartupMode          m_requestedMode;
    StartupMode          m_finalMode             = StartupMode::Normal;
    qint64               m_compositorLaunchTimeMs = 0;
    qint64               m_shellLaunchTimeMs     = 0;
    qint64               m_compositorLogStartOffset = 0;
    qint64               m_shellLogStartOffset   = 0;
    bool                 m_compositorStopRequested = false;
    QString              m_compositorStopReason;
    SessionState         m_state;
    ConfigManager        m_config;
    CompositorLaunchPlan m_plan;
    QString              m_activeWaylandDisplay;
    QProcess             m_compositorProcess;
    QProcess             m_shellProcess;
};

} // namespace Slm::Login
