#pragma once
#include <QString>
#include <QStringList>

namespace Slm::Login {

struct PlatformStatus {
    bool        ok = true;
    QStringList issues; // human-readable, empty when ok == true
    QStringList warnings;
    QString     reportPath;
    int         checksRun = 0;
    int         checksFailed = 0;
    int         checksWarn = 0;
};

class PlatformChecker
{
public:
    static const QStringList kAllowedCompositors;

    // Required SLM service binaries that must be on PATH.
    static const QStringList kRequiredServiceBinaries;

    // Runs all checks. compositorBinary and shellBinary may be bare names or
    // absolute paths.
    PlatformStatus checkAll(const QString &compositorBinary,
                            const QString &shellBinary) const;

    // ~/.config/slm-desktop/compatibility-report.json
    static QString compatibilityReportPath();

private:
    bool checkCompositorAllowed(const QString &compositor,
                                QStringList &issues) const;

    // Verifies SLM_OFFICIAL_SESSION=1 in the environment.
    bool checkOfficialSession(QStringList &issues) const;

    // Verifies that compositor and shell are findable/executable.
    bool checkBinariesExist(const QString &compositor,
                            const QString &shell,
                            QStringList &issues) const;

    // Verifies required SLM service binaries are available.
    bool checkRequiredServices(QStringList &issues) const;

    // Verifies XDG_RUNTIME_DIR is set and the directory exists.
    bool checkRuntimeDir(QStringList &issues) const;

    // Verifies baseline env expected by desktop apps.
    void checkSessionEnvironment(QStringList &warnings) const;

    // Verifies logind session metadata when loginctl/session id is available.
    void checkLogindSession(QStringList &warnings) const;

    // Verifies preconditions for Wayland/X11 app compatibility.
    void checkWaylandX11Compatibility(QStringList &issues, QStringList &warnings) const;

    // Verifies DBus and portal expectations for sandboxed apps.
    void checkPortalAndBusCompatibility(QStringList &warnings) const;

    static void writeCompatibilityReport(const QString &compositorBinary,
                                         const QString &shellBinary,
                                         PlatformStatus *status);
};

} // namespace Slm::Login
