#pragma once
#include <QString>
#include <QStringList>

namespace Slm::Login {

struct PlatformStatus {
    bool        ok = true;
    QStringList issues; // human-readable, empty when ok == true
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
};

} // namespace Slm::Login
