#include "slmplatformcheck.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace Slm::Login {

const QStringList PlatformChecker::kAllowedCompositors = {
    QStringLiteral("kwin_wayland"),
    QStringLiteral("sway"),
    QStringLiteral("wayfire"),
    QStringLiteral("weston"),
};

const QStringList PlatformChecker::kRequiredServiceBinaries = {
    QStringLiteral("desktopd"),
    QStringLiteral("slm-svcmgrd"),
    QStringLiteral("slm-loggerd"),
};

// ── Public ────────────────────────────────────────────────────────────────────

PlatformStatus PlatformChecker::checkAll(const QString &compositorBinary,
                                         const QString &shellBinary) const
{
    PlatformStatus status;
    checkCompositorAllowed(compositorBinary, status.issues);
    checkOfficialSession(status.issues);
    checkBinariesExist(compositorBinary, shellBinary, status.issues);
    checkRequiredServices(status.issues);
    checkRuntimeDir(status.issues);
    status.ok = status.issues.isEmpty();
    return status;
}

// ── Private ───────────────────────────────────────────────────────────────────

bool PlatformChecker::checkCompositorAllowed(const QString &compositor,
                                              QStringList &issues) const
{
    // Accept either a bare name or an absolute path whose filename matches.
    const QString name = QFileInfo(compositor).fileName();
    if (!kAllowedCompositors.contains(name) && !kAllowedCompositors.contains(compositor)) {
        issues.append(QStringLiteral("compositor not in allowed list: ") + compositor
                      + QStringLiteral(" (allowed: ") + kAllowedCompositors.join(QStringLiteral(", "))
                      + QStringLiteral(")"));
        return false;
    }
    return true;
}

bool PlatformChecker::checkOfficialSession(QStringList &issues) const
{
    if (qgetenv("SLM_OFFICIAL_SESSION") != "1") {
        issues.append(QStringLiteral(
            "session broker not launched via official SLM session "
            "(SLM_OFFICIAL_SESSION != 1)"));
        return false;
    }
    return true;
}

bool PlatformChecker::checkBinariesExist(const QString &compositor,
                                          const QString &shell,
                                          QStringList &issues) const
{
    bool ok = true;
    for (const QString &binary : {compositor, shell}) {
        if (binary.startsWith(QStringLiteral("/"))) {
            const QFileInfo fi(binary);
            if (!fi.exists() || !fi.isExecutable()) {
                issues.append(QStringLiteral("binary not found or not executable: ") + binary);
                ok = false;
            }
        } else {
            if (QStandardPaths::findExecutable(binary).isEmpty()) {
                issues.append(QStringLiteral("binary not found on PATH: ") + binary);
                ok = false;
            }
        }
    }
    return ok;
}

bool PlatformChecker::checkRequiredServices(QStringList &issues) const
{
    bool ok = true;
    for (const QString &binary : kRequiredServiceBinaries) {
        if (QStandardPaths::findExecutable(binary).isEmpty()) {
            issues.append(QStringLiteral("required SLM service not found on PATH: ") + binary);
            ok = false;
        }
    }
    return ok;
}

bool PlatformChecker::checkRuntimeDir(QStringList &issues) const
{
    const QByteArray runtimeDirEnv = qgetenv("XDG_RUNTIME_DIR");
    if (runtimeDirEnv.isEmpty()) {
        issues.append(QStringLiteral("XDG_RUNTIME_DIR is not set"));
        return false;
    }
    const QString runtimeDir = QString::fromLocal8Bit(runtimeDirEnv);
    if (!QDir(runtimeDir).exists()) {
        issues.append(QStringLiteral("XDG_RUNTIME_DIR does not exist: ") + runtimeDir);
        return false;
    }
    return true;
}

} // namespace Slm::Login
