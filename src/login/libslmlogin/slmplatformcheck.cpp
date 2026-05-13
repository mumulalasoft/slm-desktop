#include "slmplatformcheck.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>

#include <sys/stat.h>
#include <unistd.h>

namespace Slm::Login {

namespace {

QStringList commandOutputLines(const QString &program,
                               const QStringList &arguments,
                               int timeoutMs,
                               int *exitCodeOut = nullptr)
{
    QProcess proc;
    proc.start(program, arguments);
    if (!proc.waitForFinished(timeoutMs)) {
        proc.kill();
        proc.waitForFinished(200);
        if (exitCodeOut) {
            *exitCodeOut = -1;
        }
        return {};
    }

    if (exitCodeOut) {
        *exitCodeOut = proc.exitCode();
    }

    const QString stdoutText = QString::fromLocal8Bit(proc.readAllStandardOutput()).trimmed();
    if (stdoutText.isEmpty()) {
        return {};
    }
    return stdoutText.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}

QString propertyFromLines(const QStringList &lines, const QString &property)
{
    const QString prefix = property + QLatin1Char('=');
    for (const QString &line : lines) {
        if (line.startsWith(prefix)) {
            return line.mid(prefix.size()).trimmed();
        }
    }
    return {};
}

bool pathLooksLikeUserRuntimeDir(const QString &path, uid_t uid)
{
    const QString expected = QStringLiteral("/run/user/%1").arg(static_cast<qulonglong>(uid));
    return QFileInfo(path).absoluteFilePath() == expected;
}

bool isUnixSocketPath(const QString &path)
{
    struct stat st {};
    return ::lstat(path.toLocal8Bit().constData(), &st) == 0 && S_ISSOCK(st.st_mode);
}

QJsonArray toJsonArray(const QStringList &lines)
{
    QJsonArray arr;
    for (const QString &line : lines) {
        arr.append(line);
    }
    return arr;
}

QStringList buildRecoverySuggestions(const QStringList &issues, const QStringList &warnings)
{
    QStringList suggestions;
    const QString joined = (issues + warnings).join(QLatin1Char('\n')).toLower();
    auto add = [&](const QString &line) {
        if (!suggestions.contains(line)) {
            suggestions.append(line);
        }
    };

    if (joined.contains(QStringLiteral("xdg_runtime_dir"))) {
        add(QStringLiteral("Ensure PAM uses pam_systemd.so so /run/user/<uid> is created with owner=<uid> and mode 0700."));
    }
    if (joined.contains(QStringLiteral("dbus")) || joined.contains(QStringLiteral("busctl"))) {
        add(QStringLiteral("Ensure systemd --user and user DBus are active before launching compositor/shell."));
    }
    if (joined.contains(QStringLiteral("portal"))) {
        add(QStringLiteral("Verify portal services: systemctl --user status xdg-desktop-portal xdg-desktop-portal-gtk."));
    }
    if (joined.contains(QStringLiteral("wayland-0")) || joined.contains(QStringLiteral("wayland_display"))) {
        add(QStringLiteral("Expose /run/user/<uid>/wayland-0 as a valid socket path (real socket or valid symlink target)."));
    }
    if (joined.contains(QStringLiteral("xauthority")) || joined.contains(QStringLiteral("display"))) {
        add(QStringLiteral("Export DISPLAY and a valid XAUTHORITY file so XWayland clients can authenticate."));
    }
    if (joined.contains(QStringLiteral("loginctl")) || joined.contains(QStringLiteral("session"))) {
        add(QStringLiteral("Validate loginctl session: Type=wayland, Class=user, Seat=seat0, Active=yes, Remote=no."));
    }

    add(QStringLiteral("Collect diagnostics with: loginctl show-session $XDG_SESSION_ID, busctl --user, systemctl --user status xdg-desktop-portal."));
    return suggestions;
}

} // namespace

const QStringList PlatformChecker::kAllowedCompositors = {
    QStringLiteral("kwin_wayland"),
};

const QStringList PlatformChecker::kRequiredServiceBinaries = {
    QStringLiteral("desktopd"),
    QStringLiteral("slm-svcmgrd"),
    QStringLiteral("slm-loggerd"),
};

QString PlatformChecker::compatibilityReportPath()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return base + QStringLiteral("/slm-desktop/compatibility-report.json");
}

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
    checkSessionEnvironment(status.warnings);
    checkLogindSession(status.warnings);
    checkWaylandX11Compatibility(status.issues, status.warnings);
    checkPortalAndBusCompatibility(status.warnings);

    status.ok = status.issues.isEmpty();
    writeCompatibilityReport(compositorBinary, shellBinary, &status);
    status.checksFailed = status.issues.size();
    status.checksWarn = status.warnings.size();
    if (status.checksRun <= 0) {
        status.checksRun = status.checksFailed + status.checksWarn;
    }
    return status;
}

// ── Private ───────────────────────────────────────────────────────────────────

bool PlatformChecker::checkCompositorAllowed(const QString &compositor,
                                             QStringList &issues) const
{
    const QString name = QFileInfo(compositor).fileName();
    if (!kAllowedCompositors.contains(name) && !kAllowedCompositors.contains(compositor)) {
        issues.append(QStringLiteral("[env] compositor not in allowed list: ") + compositor
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
            "[session] session broker not launched via official SLM session "
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
                issues.append(QStringLiteral("[env] binary not found or not executable: ") + binary);
                ok = false;
            }
        } else {
            if (QStandardPaths::findExecutable(binary).isEmpty()) {
                issues.append(QStringLiteral("[env] binary not found on PATH: ") + binary);
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
            issues.append(QStringLiteral("[env] required SLM service not found on PATH: ") + binary);
            ok = false;
        }
    }
    return ok;
}

bool PlatformChecker::checkRuntimeDir(QStringList &issues) const
{
    const QByteArray runtimeDirEnv = qgetenv("XDG_RUNTIME_DIR");
    if (runtimeDirEnv.isEmpty()) {
        issues.append(QStringLiteral("[env] XDG_RUNTIME_DIR is not set"));
        return false;
    }

    const QString runtimeDir = QString::fromLocal8Bit(runtimeDirEnv);
    const QFileInfo info(runtimeDir);
    if (!info.exists() || !info.isDir()) {
        issues.append(QStringLiteral("[env] XDG_RUNTIME_DIR does not exist as directory: ") + runtimeDir);
        return false;
    }

    const bool allowNonStandardForTests = qEnvironmentVariableIsSet("SLM_TEST_ALLOW_NONSTANDARD_RUNTIME_DIR")
        || qEnvironmentVariableIsSet("SLM_TEST_ACCEPT_REGULAR_WAYLAND_SOCKET");
    if (allowNonStandardForTests) {
        return true;
    }

    const uid_t uid = ::getuid();
    if (!pathLooksLikeUserRuntimeDir(runtimeDir, uid)) {
        issues.append(QStringLiteral("[env] XDG_RUNTIME_DIR is non-standard: %1 (expected /run/user/%2)")
                          .arg(runtimeDir)
                          .arg(static_cast<qulonglong>(uid)));
    }

    if (info.ownerId() != static_cast<uint>(uid)) {
        issues.append(QStringLiteral("[env] XDG_RUNTIME_DIR owner mismatch: %1 (uid=%2 expected=%3)")
                          .arg(runtimeDir)
                          .arg(info.ownerId())
                          .arg(static_cast<qulonglong>(uid)));
    }

    const QFileDevice::Permissions perms = QFile::permissions(runtimeDir);
    const QFileDevice::Permissions ownerMask = QFileDevice::ReadOwner
        | QFileDevice::WriteOwner
        | QFileDevice::ExeOwner;
    const QFileDevice::Permissions groupOtherMask = QFileDevice::ReadGroup
        | QFileDevice::WriteGroup
        | QFileDevice::ExeGroup
        | QFileDevice::ReadOther
        | QFileDevice::WriteOther
        | QFileDevice::ExeOther;
    const bool hasOwnerPerms = (perms & ownerMask) == ownerMask;
    const bool hasExtraPerms = (perms & groupOtherMask) != QFileDevice::Permissions();
    if (!hasOwnerPerms || hasExtraPerms) {
        issues.append(QStringLiteral("[env] XDG_RUNTIME_DIR permissions are not 0700-compatible: %1")
                          .arg(runtimeDir));
    }

    return true;
}

void PlatformChecker::checkSessionEnvironment(QStringList &warnings) const
{
    const QString sessionType = QString::fromLocal8Bit(qgetenv("XDG_SESSION_TYPE")).trimmed();
    if (sessionType.isEmpty()) {
        warnings.append(QStringLiteral("[session] XDG_SESSION_TYPE is unset"));
    } else if (sessionType != QStringLiteral("wayland")) {
        warnings.append(QStringLiteral("[session] XDG_SESSION_TYPE is '%1' (expected 'wayland')").arg(sessionType));
    }

    const QString sessionClass = QString::fromLocal8Bit(qgetenv("XDG_SESSION_CLASS")).trimmed();
    if (!sessionClass.isEmpty() && sessionClass != QStringLiteral("user")) {
        warnings.append(QStringLiteral("[session] XDG_SESSION_CLASS is '%1' (expected 'user')").arg(sessionClass));
    }

    const QString desktop = QString::fromLocal8Bit(qgetenv("XDG_CURRENT_DESKTOP")).trimmed();
    if (desktop.isEmpty()) {
        warnings.append(QStringLiteral("[session] XDG_CURRENT_DESKTOP is unset"));
    }

    const QString dbusAddress = QString::fromLocal8Bit(qgetenv("DBUS_SESSION_BUS_ADDRESS")).trimmed();
    if (dbusAddress.isEmpty()) {
        warnings.append(QStringLiteral("[portal] DBUS_SESSION_BUS_ADDRESS is unset"));
    } else if (!dbusAddress.startsWith(QStringLiteral("unix:path="))) {
        warnings.append(QStringLiteral("[portal] DBUS_SESSION_BUS_ADDRESS is non-standard: %1").arg(dbusAddress));
    }

    const QString display = QString::fromLocal8Bit(qgetenv("DISPLAY")).trimmed();
    if (!display.isEmpty() && !display.startsWith(QLatin1Char(':'))) {
        warnings.append(QStringLiteral("[x11] DISPLAY format is unusual: %1").arg(display));
    }

    const QString xauth = QString::fromLocal8Bit(qgetenv("XAUTHORITY")).trimmed();
    if (!xauth.isEmpty() && !QFileInfo::exists(xauth)) {
        warnings.append(QStringLiteral("[x11] XAUTHORITY points to missing path: %1").arg(xauth));
    }

    const QString qtNoPortal = QString::fromLocal8Bit(qgetenv("QT_NO_XDG_DESKTOP_PORTAL")).trimmed();
    if (qtNoPortal == QStringLiteral("1")) {
        warnings.append(QStringLiteral("[portal] QT_NO_XDG_DESKTOP_PORTAL=1 disables standard portal integration"));
    }
    const QString gtkUsePortal = QString::fromLocal8Bit(qgetenv("GTK_USE_PORTAL")).trimmed();
    if (gtkUsePortal == QStringLiteral("0")) {
        warnings.append(QStringLiteral("[portal] GTK_USE_PORTAL=0 disables GTK portal integration"));
    }
    const QString gioUsePortals = QString::fromLocal8Bit(qgetenv("GIO_USE_PORTALS")).trimmed();
    if (gioUsePortals == QStringLiteral("0")) {
        warnings.append(QStringLiteral("[portal] GIO_USE_PORTALS=0 disables GIO portal integration"));
    }
}

void PlatformChecker::checkLogindSession(QStringList &warnings) const
{
    const QString sessionId = QString::fromLocal8Bit(qgetenv("XDG_SESSION_ID")).trimmed();
    if (sessionId.isEmpty()) {
        warnings.append(QStringLiteral("[session] XDG_SESSION_ID is unset (pam_systemd may be missing)"));
        return;
    }

    const QString loginctl = QStandardPaths::findExecutable(QStringLiteral("loginctl"));
    if (loginctl.isEmpty()) {
        warnings.append(QStringLiteral("[session] loginctl not found on PATH; cannot validate logind properties"));
        return;
    }

    int exitCode = -1;
    const QStringList out = commandOutputLines(
        loginctl,
        {QStringLiteral("show-session"), sessionId,
         QStringLiteral("-p"), QStringLiteral("Type"),
         QStringLiteral("-p"), QStringLiteral("Class"),
         QStringLiteral("-p"), QStringLiteral("Seat"),
         QStringLiteral("-p"), QStringLiteral("Active"),
         QStringLiteral("-p"), QStringLiteral("Remote"),
         QStringLiteral("-p"), QStringLiteral("State")},
        3000,
        &exitCode);

    if (exitCode != 0 || out.isEmpty()) {
        warnings.append(QStringLiteral("[session] loginctl show-session failed for session '%1'").arg(sessionId));
        return;
    }

    const QString type = propertyFromLines(out, QStringLiteral("Type"));
    const QString klass = propertyFromLines(out, QStringLiteral("Class"));
    const QString seat = propertyFromLines(out, QStringLiteral("Seat"));
    const QString active = propertyFromLines(out, QStringLiteral("Active"));
    const QString remote = propertyFromLines(out, QStringLiteral("Remote"));
    const QString state = propertyFromLines(out, QStringLiteral("State"));

    if (!type.isEmpty() && type != QStringLiteral("wayland")) {
        warnings.append(QStringLiteral("[session] loginctl Type=%1 (expected wayland)").arg(type));
    }
    if (!klass.isEmpty() && klass != QStringLiteral("user")) {
        warnings.append(QStringLiteral("[session] loginctl Class=%1 (expected user)").arg(klass));
    }
    if (!seat.isEmpty() && seat != QStringLiteral("seat0")) {
        warnings.append(QStringLiteral("[session] loginctl Seat=%1 (expected seat0)").arg(seat));
    }
    if (!active.isEmpty() && active != QStringLiteral("yes")) {
        warnings.append(QStringLiteral("[session] loginctl Active=%1 (expected yes)").arg(active));
    }
    if (!remote.isEmpty() && remote != QStringLiteral("no")) {
        warnings.append(QStringLiteral("[session] loginctl Remote=%1 (expected no)").arg(remote));
    }
    if (!state.isEmpty() && state != QStringLiteral("active")) {
        warnings.append(QStringLiteral("[session] loginctl State=%1 (expected active)").arg(state));
    }
}

void PlatformChecker::checkWaylandX11Compatibility(QStringList &issues, QStringList &warnings) const
{
    const QString runtimeDir = QString::fromLocal8Bit(qgetenv("XDG_RUNTIME_DIR")).trimmed();
    if (runtimeDir.isEmpty()) {
        return;
    }

    const QString wayland0Path = runtimeDir + QStringLiteral("/wayland-0");
    const QFileInfo wayland0Info(wayland0Path);
    if (wayland0Info.exists() || wayland0Info.isSymLink()) {
        if (wayland0Info.isSymLink()) {
            const QString target = wayland0Info.symLinkTarget();
            if (target.isEmpty() || !QFileInfo::exists(target)) {
                issues.append(QStringLiteral("[wayland] wayland-0 symlink target missing: %1 -> %2")
                                  .arg(wayland0Path, target));
            } else if (!isUnixSocketPath(target)
                       && !qEnvironmentVariableIsSet("SLM_TEST_ACCEPT_REGULAR_WAYLAND_SOCKET")) {
                issues.append(QStringLiteral("[wayland] wayland-0 symlink target is not a UNIX socket: %1 -> %2")
                                  .arg(wayland0Path, target));
            }
        } else if (!isUnixSocketPath(wayland0Path)
                   && !qEnvironmentVariableIsSet("SLM_TEST_ACCEPT_REGULAR_WAYLAND_SOCKET")) {
            issues.append(QStringLiteral("[wayland] wayland-0 exists but is not a UNIX socket: %1")
                              .arg(wayland0Path));
        }
    } else {
        warnings.append(QStringLiteral("[wayland] wayland-0 not present yet in runtime dir: %1").arg(wayland0Path));
    }

    const QString waylandDisplay = QString::fromLocal8Bit(qgetenv("WAYLAND_DISPLAY")).trimmed();
    if (!waylandDisplay.isEmpty() && waylandDisplay != QStringLiteral("wayland-0")) {
        if (!(QFileInfo::exists(wayland0Path) || QFileInfo(wayland0Path).isSymLink())) {
            warnings.append(QStringLiteral("[wayland] WAYLAND_DISPLAY=%1 without wayland-0 compatibility alias")
                                .arg(waylandDisplay));
        }
    }

    const QString display = QString::fromLocal8Bit(qgetenv("DISPLAY")).trimmed();
    if (!display.isEmpty() && display.startsWith(QLatin1Char(':'))) {
        const QString displayName = display.mid(1);
        const QString x11Socket = QStringLiteral("/tmp/.X11-unix/X%1").arg(displayName);
        if (!QFileInfo::exists(x11Socket)) {
            warnings.append(QStringLiteral("[x11] DISPLAY points to missing X11 socket: %1").arg(x11Socket));
        }
        const QString xauth = QString::fromLocal8Bit(qgetenv("XAUTHORITY")).trimmed();
        if (xauth.isEmpty()) {
            warnings.append(QStringLiteral("[x11] DISPLAY is set but XAUTHORITY is unset"));
        }
    }
}

void PlatformChecker::checkPortalAndBusCompatibility(QStringList &warnings) const
{
    const bool graphicalEnvReady =
        !qgetenv("WAYLAND_DISPLAY").trimmed().isEmpty()
        || !qgetenv("DISPLAY").trimmed().isEmpty();

    const QString busctl = QStandardPaths::findExecutable(QStringLiteral("busctl"));
    if (busctl.isEmpty()) {
        warnings.append(QStringLiteral("[portal] busctl not found; cannot validate user bus services"));
    } else {
        int exitCode = -1;
        const QStringList out = commandOutputLines(
            busctl, {QStringLiteral("--user"), QStringLiteral("list")}, 2500, &exitCode);
        if (exitCode != 0 || out.isEmpty()) {
            warnings.append(QStringLiteral("[portal] busctl --user list failed; user DBus may be unavailable"));
        } else {
            bool portalNamePresent = false;
            for (const QString &line : out) {
                if (line.contains(QStringLiteral("org.freedesktop.portal.Desktop"))) {
                    portalNamePresent = true;
                    break;
                }
            }
            if (!portalNamePresent) {
                warnings.append(QStringLiteral("[portal] org.freedesktop.portal.Desktop is not reachable on user bus"));
            }
        }
    }

    if (!graphicalEnvReady) {
        warnings.append(QStringLiteral("[portal] portal runtime check deferred until graphical environment is ready"));
        return;
    }

    const QString systemctl = QStandardPaths::findExecutable(QStringLiteral("systemctl"));
    if (systemctl.isEmpty()) {
        warnings.append(QStringLiteral("[portal] systemctl not found; cannot validate portal unit state"));
        return;
    }

    const QString runtimeDir = QString::fromLocal8Bit(qgetenv("XDG_RUNTIME_DIR")).trimmed();
    if (runtimeDir.isEmpty()) {
        warnings.append(QStringLiteral("[portal] XDG_RUNTIME_DIR unset; cannot query systemctl --user"));
        return;
    }

    QProcess userState;
    userState.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    userState.start(systemctl, {QStringLiteral("--user"), QStringLiteral("is-active"),
                                QStringLiteral("xdg-desktop-portal.service")});
    if (!userState.waitForFinished(2500)) {
        userState.kill();
        userState.waitForFinished(200);
        warnings.append(QStringLiteral("[portal] systemctl --user is-active xdg-desktop-portal timed out"));
        return;
    }

    const QString active = QString::fromLocal8Bit(userState.readAllStandardOutput()).trimmed();
    if (userState.exitCode() != 0 || active != QStringLiteral("active")) {
        warnings.append(QStringLiteral("[portal] xdg-desktop-portal is not active (state=%1)").arg(active));
    }

    int activeBackends = 0;
    const QStringList backendUnits{
        QStringLiteral("slm-portald.service"),
        QStringLiteral("xdg-desktop-portal-gtk.service"),
        QStringLiteral("xdg-desktop-portal-kde.service"),
    };
    for (const QString &unit : backendUnits) {
        QProcess backendState;
        backendState.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
        backendState.start(systemctl, {QStringLiteral("--user"), QStringLiteral("is-active"), unit});
        if (!backendState.waitForFinished(2000)) {
            backendState.kill();
            backendState.waitForFinished(200);
            continue;
        }
        const QString state = QString::fromLocal8Bit(backendState.readAllStandardOutput()).trimmed();
        if (backendState.exitCode() == 0 && state == QStringLiteral("active")) {
            ++activeBackends;
        }
    }
    if (activeBackends == 0) {
        warnings.append(QStringLiteral("[portal] no active portal backend (expected one of: slm-portald, xdg-desktop-portal-gtk, xdg-desktop-portal-kde)"));
    }
}

void PlatformChecker::writeCompatibilityReport(const QString &compositorBinary,
                                               const QString &shellBinary,
                                               PlatformStatus *status)
{
    if (!status) {
        return;
    }

    const QString reportPath = compatibilityReportPath();
    status->reportPath = reportPath;

    QJsonArray checklist;
    checklist.append(QJsonObject{{QStringLiteral("id"), QStringLiteral("session.logind")},
                                 {QStringLiteral("requirement"), QStringLiteral("loginctl session should be wayland/user/seat0/active/non-remote")}});
    checklist.append(QJsonObject{{QStringLiteral("id"), QStringLiteral("env.runtime_dir")},
                                 {QStringLiteral("requirement"), QStringLiteral("XDG_RUNTIME_DIR must be /run/user/<uid> with owner uid and 0700 permissions")}});
    checklist.append(QJsonObject{{QStringLiteral("id"), QStringLiteral("env.dbus")},
                                 {QStringLiteral("requirement"), QStringLiteral("DBUS_SESSION_BUS_ADDRESS should resolve to user session bus")}});
    checklist.append(QJsonObject{{QStringLiteral("id"), QStringLiteral("wayland.socket")},
                                 {QStringLiteral("requirement"), QStringLiteral("wayland-0 compatibility socket path must exist and be valid")}});
    checklist.append(QJsonObject{{QStringLiteral("id"), QStringLiteral("x11.fallback")},
                                 {QStringLiteral("requirement"), QStringLiteral("DISPLAY/XAUTHORITY/X11 socket should be valid for XWayland fallback")}});
    checklist.append(QJsonObject{{QStringLiteral("id"), QStringLiteral("portal.desktop")},
                                 {QStringLiteral("requirement"), QStringLiteral("xdg-desktop-portal should be active for sandboxed app flows")}});

    QJsonObject envSnapshot{
        {QStringLiteral("XDG_SESSION_ID"), QString::fromLocal8Bit(qgetenv("XDG_SESSION_ID"))},
        {QStringLiteral("XDG_SESSION_TYPE"), QString::fromLocal8Bit(qgetenv("XDG_SESSION_TYPE"))},
        {QStringLiteral("XDG_SESSION_CLASS"), QString::fromLocal8Bit(qgetenv("XDG_SESSION_CLASS"))},
        {QStringLiteral("XDG_SEAT"), QString::fromLocal8Bit(qgetenv("XDG_SEAT"))},
        {QStringLiteral("XDG_RUNTIME_DIR"), QString::fromLocal8Bit(qgetenv("XDG_RUNTIME_DIR"))},
        {QStringLiteral("DBUS_SESSION_BUS_ADDRESS"), QString::fromLocal8Bit(qgetenv("DBUS_SESSION_BUS_ADDRESS"))},
        {QStringLiteral("WAYLAND_DISPLAY"), QString::fromLocal8Bit(qgetenv("WAYLAND_DISPLAY"))},
        {QStringLiteral("DISPLAY"), QString::fromLocal8Bit(qgetenv("DISPLAY"))},
        {QStringLiteral("XAUTHORITY"), QString::fromLocal8Bit(qgetenv("XAUTHORITY"))},
    };

    const QStringList suggestions = buildRecoverySuggestions(status->issues, status->warnings);

    QJsonObject report{
        {QStringLiteral("policy"), QStringLiteral("SLM Desktop Compatibility Policy (Wayland/X11/Snap/Flatpak)")},
        {QStringLiteral("policyVersion"), QStringLiteral("1.0")},
        {QStringLiteral("timestampUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {QStringLiteral("ok"), status->ok},
        {QStringLiteral("compositor"), compositorBinary},
        {QStringLiteral("shell"), shellBinary},
        {QStringLiteral("issues"), toJsonArray(status->issues)},
        {QStringLiteral("warnings"), toJsonArray(status->warnings)},
        {QStringLiteral("recoverySuggestions"), toJsonArray(suggestions)},
        {QStringLiteral("checklist"), checklist},
        {QStringLiteral("environment"), envSnapshot},
        {QStringLiteral("diagnosticCommands"), QJsonArray{
             QStringLiteral("loginctl show-session $XDG_SESSION_ID"),
             QStringLiteral("busctl --user"),
             QStringLiteral("systemctl --user status xdg-desktop-portal xdg-desktop-portal-gtk"),
             QStringLiteral("ls -l /run/user/$UID/wayland-0"),
             QStringLiteral("echo DISPLAY=$DISPLAY XAUTHORITY=$XAUTHORITY"),
         }},
    };

    status->checksRun = checklist.size();

    const QString reportDir = QFileInfo(reportPath).absolutePath();
    QDir().mkpath(reportDir);
    QSaveFile out(reportPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        status->warnings.append(QStringLiteral("[report] failed to open compatibility report path: ") + reportPath);
        return;
    }
    const QByteArray payload = QJsonDocument(report).toJson(QJsonDocument::Indented);
    if (out.write(payload) != payload.size()) {
        out.cancelWriting();
        status->warnings.append(QStringLiteral("[report] failed to write compatibility report: ") + reportPath);
        return;
    }
    if (!out.commit()) {
        status->warnings.append(QStringLiteral("[report] failed to commit compatibility report: ") + reportPath);
    }
}

} // namespace Slm::Login
