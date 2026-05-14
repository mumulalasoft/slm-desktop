#include "applauncher.h"

#include <QDebug>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringList>

#ifdef signals
#undef signals
#define APPLAUNCHER_RESTORE_QT_SIGNALS
#endif
extern "C" {
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
}
#ifdef APPLAUNCHER_RESTORE_QT_SIGNALS
#define signals Q_SIGNALS
#undef APPLAUNCHER_RESTORE_QT_SIGNALS
#endif

static QString fromUtf8(const char *value)
{
    return value ? QString::fromUtf8(value) : QString();
}

namespace {

QString takeKeyFileString(GKeyFile *keyFile,
                          const char *group,
                          const char *key,
                          bool *found = nullptr)
{
    if (found) {
        *found = false;
    }

    GError *error = nullptr;
    gchar *value = g_key_file_get_string(keyFile, group, key, &error);
    if (error) {
        g_error_free(error);
        return {};
    }

    if (found) {
        *found = true;
    }
    const QString result = fromUtf8(value);
    g_free(value);
    return result;
}

bool keyFileBool(GKeyFile *keyFile, const char *group, const char *key, bool defaultValue = false)
{
    GError *error = nullptr;
    const gboolean value = g_key_file_get_boolean(keyFile, group, key, &error);
    if (error) {
        g_error_free(error);
        return defaultValue;
    }
    return value;
}

QString quoteDesktopField(const QString &value)
{
    QString quoted = value;
    quoted.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    quoted.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(quoted);
}

bool isFlatpakToken(const QString &token)
{
    return QFileInfo(token).fileName() == QStringLiteral("flatpak");
}

QString sanitizeFlatpakForwardingCommand(const QString &command, bool removedFilePlaceholders)
{
    if (!removedFilePlaceholders) {
        return command.simplified();
    }

    QStringList parts = QProcess::splitCommand(command.simplified());
    if (parts.isEmpty()) {
        return command.simplified();
    }

    int flatpakPos = -1;
    for (int i = 0; i < parts.size(); ++i) {
        if (isFlatpakToken(parts.at(i))) {
            flatpakPos = i;
            break;
        }
    }
    if (flatpakPos < 0) {
        return command.simplified();
    }

    bool hasRunSubcommand = false;
    for (int i = flatpakPos + 1; i < parts.size(); ++i) {
        const QString token = parts.at(i);
        if (token.startsWith(QLatin1Char('-'))) {
            continue;
        }
        hasRunSubcommand = token == QStringLiteral("run");
        break;
    }
    if (!hasRunSubcommand) {
        return command.simplified();
    }

    QStringList cleaned;
    cleaned.reserve(parts.size());
    for (int i = 0; i < parts.size(); ++i) {
        const QString token = parts.at(i);
        if (i > flatpakPos) {
            if (token == QStringLiteral("--file-forwarding")
                || token == QStringLiteral("@@")
                || token == QStringLiteral("@@u")) {
                continue;
            }
        }
        cleaned.append(token);
    }
    while (!cleaned.isEmpty() && cleaned.last() == QStringLiteral("--")) {
        cleaned.removeLast();
    }

    if (cleaned.isEmpty()) {
        return command.simplified();
    }

    QStringList rendered;
    rendered.reserve(cleaned.size());
    for (const QString &part : cleaned) {
        rendered.append(part.contains(QRegularExpression(QStringLiteral("\\s"))) || part.isEmpty()
                            ? quoteDesktopField(part)
                            : part);
    }
    return rendered.join(QLatin1Char(' '));
}

QString expandDesktopExecTemplate(QString execTemplate,
                                  const QString &desktopFilePath,
                                  const QString &displayName = QString(),
                                  const QString &iconName = QString())
{
    if (execTemplate.trimmed().isEmpty()) {
        return {};
    }

    const QString desktopPath = QFileInfo(desktopFilePath).absoluteFilePath();
    const QString percentToken = QString::fromLatin1("\x01");
    const bool hasFilePlaceholder =
        execTemplate.contains(QStringLiteral("%F"))
        || execTemplate.contains(QStringLiteral("%f"))
        || execTemplate.contains(QStringLiteral("%U"))
        || execTemplate.contains(QStringLiteral("%u"));

    execTemplate.replace(QStringLiteral("%%"), percentToken);
    execTemplate.replace(QStringLiteral("%F"), QString());
    execTemplate.replace(QStringLiteral("%f"), QString());
    execTemplate.replace(QStringLiteral("%U"), QString());
    execTemplate.replace(QStringLiteral("%u"), QString());
    execTemplate.replace(QStringLiteral("%i"),
                         iconName.trimmed().isEmpty()
                             ? QString()
                             : QStringLiteral("--icon %1").arg(quoteDesktopField(iconName.trimmed())));
    execTemplate.replace(QStringLiteral("%c"),
                         displayName.trimmed().isEmpty()
                             ? QString()
                             : quoteDesktopField(displayName.trimmed()));
    execTemplate.replace(QStringLiteral("%k"), quoteDesktopField(desktopPath));
    execTemplate.replace(QRegularExpression(QStringLiteral("%[dDnNvVmM]")), QString());
    execTemplate.replace(percentToken, QStringLiteral("%"));
    return sanitizeFlatpakForwardingCommand(execTemplate.simplified(), hasFilePlaceholder);
}

bool executableExists(const QString &program)
{
    const QString trimmed = program.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    const QFileInfo info(trimmed);
    if (info.isAbsolute() || trimmed.contains(QLatin1Char('/'))) {
        return info.exists() && info.isFile() && info.isExecutable();
    }

    return !QStandardPaths::findExecutable(trimmed).isEmpty();
}

QString tryExecFailure(const QString &tryExec)
{
    const QString trimmed = tryExec.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    return executableExists(trimmed)
        ? QString()
        : QStringLiteral("TryExec not available: %1").arg(trimmed);
}

QString terminalProgram()
{
    static const QStringList candidates{
        QStringLiteral("x-terminal-emulator"),
        QStringLiteral("konsole"),
        QStringLiteral("gnome-terminal"),
        QStringLiteral("kgx"),
        QStringLiteral("kitty"),
        QStringLiteral("alacritty"),
        QStringLiteral("foot"),
        QStringLiteral("xterm"),
    };

    for (const QString &candidate : candidates) {
        const QString path = QStandardPaths::findExecutable(candidate);
        if (!path.isEmpty()) {
            return path;
        }
    }
    return {};
}

bool startProgram(const QString &program,
                  const QStringList &arguments,
                  const QProcessEnvironment &env,
                  qint64 *pidOut)
{
    QProcess proc;
    proc.setProcessEnvironment(env);
    proc.setProgram(program);
    proc.setArguments(arguments);
    return proc.startDetached(pidOut);
}

QString joinCommandForLog(const QStringList &parts)
{
    QStringList rendered;
    rendered.reserve(parts.size());
    for (const QString &part : parts) {
        rendered.append(part.contains(QRegularExpression(QStringLiteral("\\s"))) || part.isEmpty()
                            ? quoteDesktopField(part)
                            : part);
    }
    return rendered.join(QLatin1Char(' '));
}

bool startExpandedCommand(const QString &command,
                          const QProcessEnvironment &env,
                          qint64 *pidOut,
                          QString *resolvedCommandOut = nullptr,
                          bool dbusActivatable = false,
                          bool *privateSessionBusOut = nullptr)
{
    Q_UNUSED(dbusActivatable);

    if (privateSessionBusOut) {
        *privateSessionBusOut = false;
    }

    const QString expanded = command.trimmed();
    if (expanded.isEmpty()) {
        return false;
    }

    QStringList parts = QProcess::splitCommand(expanded);
    if (parts.isEmpty()) {
        return false;
    }

    if (resolvedCommandOut) {
        *resolvedCommandOut = joinCommandForLog(parts);
    }

    return startProgram(parts.first(), parts.mid(1), env, pidOut);
}

bool tryLaunchTerminalCommand(const QString &command,
                              const QProcessEnvironment &env,
                              qint64 *pidOut,
                              QString *errorOut)
{
    const QString terminal = terminalProgram();
    if (terminal.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Terminal=true but no supported terminal emulator was found");
        }
        return false;
    }

    const QString terminalName = QFileInfo(terminal).fileName();
    QStringList args;
    if (terminalName == QStringLiteral("gnome-terminal") || terminalName == QStringLiteral("kgx")) {
        args << QStringLiteral("--") << QStringLiteral("/bin/sh") << QStringLiteral("-lc") << command;
    } else if (terminalName == QStringLiteral("konsole")) {
        args << QStringLiteral("-e") << QStringLiteral("/bin/sh") << QStringLiteral("-lc") << command;
    } else {
        args << QStringLiteral("-e") << QStringLiteral("/bin/sh") << QStringLiteral("-lc") << command;
    }
    return startProgram(terminal, args, env, pidOut);
}

bool launchGioDetached(const QString &desktopFilePath,
                       const QProcessEnvironment &env,
                       qint64 *pidOut,
                       QString *errorOut)
{
    const QString gio = QStandardPaths::findExecutable(QStringLiteral("gio"));
    if (gio.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("gio executable not found");
        }
        return false;
    }

    if (startProgram(gio, {QStringLiteral("launch"), desktopFilePath}, env, pidOut)) {
        return true;
    }

    if (errorOut) {
        *errorOut = QStringLiteral("Failed to start gio launch fallback");
    }
    return false;
}

bool envFlagEnabled(const QProcessEnvironment &env, const QString &name)
{
    const QString value = env.value(name).trimmed().toLower();
    return value == QStringLiteral("1")
        || value == QStringLiteral("true")
        || value == QStringLiteral("yes")
        || value == QStringLiteral("on");
}

bool hasWaylandCompatibilitySocket(const QProcessEnvironment &env)
{
    const QString runtimeDir = env.value(QStringLiteral("XDG_RUNTIME_DIR")).trimmed();
    if (runtimeDir.isEmpty()) {
        return false;
    }

    const QFileInfo compatibilitySocket(runtimeDir + QStringLiteral("/wayland-0"));
    if (!(compatibilitySocket.exists() || compatibilitySocket.isSymLink())) {
        return false;
    }

    if (compatibilitySocket.isSymLink()) {
        const QString target = compatibilitySocket.symLinkTarget();
        return !target.isEmpty() && QFileInfo::exists(target);
    }

    return true;
}

bool isSlmSession(const QProcessEnvironment &env)
{
    const QString currentDesktop = env.value(QStringLiteral("XDG_CURRENT_DESKTOP"));
    const QString sessionDesktop = env.value(QStringLiteral("XDG_SESSION_DESKTOP"));
    return env.value(QStringLiteral("SLM_OFFICIAL_SESSION")) == QStringLiteral("1")
        || currentDesktop.split(QLatin1Char(':'), Qt::SkipEmptyParts)
               .contains(QStringLiteral("SLM"), Qt::CaseInsensitive)
        || sessionDesktop.compare(QStringLiteral("slm"), Qt::CaseInsensitive) == 0;
}

bool tryLaunchDesktopFileDirect(const QString &desktopFilePath,
                                const QProcessEnvironment &env,
                                AppLaunchResult *result)
{
    const QString path = QFileInfo(desktopFilePath).absoluteFilePath();
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        return false;
    }

    GKeyFile *keyFile = g_key_file_new();
    GError *keyError = nullptr;
    if (!g_key_file_load_from_file(keyFile, path.toUtf8().constData(), G_KEY_FILE_NONE, &keyError)) {
        if (keyError) {
            g_error_free(keyError);
        }
        g_key_file_free(keyFile);
        return false;
    }

    const QString type = takeKeyFileString(keyFile, "Desktop Entry", "Type").trimmed();
    if (!type.isEmpty() && type != QStringLiteral("Application")) {
        g_key_file_free(keyFile);
        if (result) {
            result->error = QStringLiteral("Desktop entry is not an application");
        }
        return false;
    }

    if (keyFileBool(keyFile, "Desktop Entry", "Hidden", false)) {
        g_key_file_free(keyFile);
        if (result) {
            result->error = QStringLiteral("Desktop entry is hidden");
        }
        return false;
    }

    bool hasExec = false;
    const QString execTemplate = takeKeyFileString(keyFile, "Desktop Entry", "Exec", &hasExec);
    const QString displayName = takeKeyFileString(keyFile, "Desktop Entry", "Name");
    const QString iconName = takeKeyFileString(keyFile, "Desktop Entry", "Icon");
    const QString tryExec = takeKeyFileString(keyFile, "Desktop Entry", "TryExec");
    const QString xFlatpak = takeKeyFileString(keyFile, "Desktop Entry", "X-Flatpak");
    const bool terminal = keyFileBool(keyFile, "Desktop Entry", "Terminal", false);
    const bool dbusActivatable = keyFileBool(keyFile, "Desktop Entry", "DBusActivatable", false);
    g_key_file_free(keyFile);

    if (result) {
        result->displayName = displayName;
        result->executable = execTemplate;
        result->iconName = iconName;
    }

    if (!hasExec || execTemplate.trimmed().isEmpty()) {
        if (result) {
            result->error = QStringLiteral("Desktop entry has no Exec key");
        }
        return false;
    }

    const QString tryExecError = tryExecFailure(tryExec);
    const bool isFlatpakDesktop = !xFlatpak.trimmed().isEmpty()
        || execTemplate.contains(QRegularExpression(QStringLiteral("(^|\\s)flatpak\\s+run(\\s|$)")));
    if (!tryExecError.isEmpty() && !isFlatpakDesktop) {
        if (result) {
            result->error = tryExecError;
        }
        return false;
    }

    const QString expandedExec = expandDesktopExecTemplate(execTemplate, path, displayName, iconName);
    if (expandedExec.isEmpty()) {
        if (result) {
            result->error = QStringLiteral("Expanded Exec command is empty");
        }
        return false;
    }

    const QProcessEnvironment launchEnv = env;

    qint64 pid = -1;
    QString error;
    QString resolvedCommand = expandedExec;
    const bool launched = terminal
        ? tryLaunchTerminalCommand(expandedExec, launchEnv, &pid, &error)
        : startExpandedCommand(expandedExec,
                               launchEnv,
                               &pid,
                               &resolvedCommand,
                               dbusActivatable);
    if (launched) {
        if (result) {
            result->ok = true;
            result->pid = pid;
            result->launchCommand = terminal ? expandedExec : resolvedCommand;
            result->privateSessionBus = false;
        }
        return true;
    }

    if (result) {
        result->error = error.isEmpty()
            ? QStringLiteral("Failed to start Exec command: %1").arg(expandedExec)
            : error;
    }
    return false;
}

} // namespace

QProcessEnvironment AppLauncher::prepareLaunchEnvironment(const QProcessEnvironment &env)
{
    QProcessEnvironment prepared = env;

    if (!isSlmSession(prepared)) {
        return prepared;
    }

    const QString waylandDisplay = prepared.value(QStringLiteral("WAYLAND_DISPLAY")).trimmed();
    if (waylandDisplay.startsWith(QStringLiteral("slm-wayland-"))
            && hasWaylandCompatibilitySocket(prepared)
            && !envFlagEnabled(prepared, QStringLiteral("SLM_FORCE_INTERNAL_WAYLAND_DISPLAY"))) {
        prepared.insert(QStringLiteral("WAYLAND_DISPLAY"), QStringLiteral("wayland-0"));
    }

    return prepared;
}

QString AppLauncher::expandDesktopExecCommand(const QString &desktopFilePath)
{
    const QString path = QFileInfo(desktopFilePath).absoluteFilePath();
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        return {};
    }

    GKeyFile *keyFile = g_key_file_new();
    GError *keyError = nullptr;
    if (!g_key_file_load_from_file(keyFile, path.toUtf8().constData(), G_KEY_FILE_NONE, &keyError)) {
        if (keyError) {
            g_error_free(keyError);
        }
        g_key_file_free(keyFile);
        return {};
    }

    bool hasExec = false;
    const QString execTemplate = takeKeyFileString(keyFile, "Desktop Entry", "Exec", &hasExec);
    if (!hasExec) {
        g_key_file_free(keyFile);
        return {};
    }
    const QString displayName = takeKeyFileString(keyFile, "Desktop Entry", "Name");
    const QString iconName = takeKeyFileString(keyFile, "Desktop Entry", "Icon");
    const QString expanded = expandDesktopExecTemplate(execTemplate, path, displayName, iconName);
    g_key_file_free(keyFile);
    return expanded;
}

AppLaunchResult AppLauncher::launchDesktopFile(const QString &desktopFilePath,
                                                const QProcessEnvironment &env)
{
    AppLaunchResult result;
    result.desktopFilePath = desktopFilePath;
    const QProcessEnvironment launchEnv = prepareLaunchEnvironment(env);

    const QString path = QFileInfo(desktopFilePath).absoluteFilePath();
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        qWarning() << "AppLauncher: desktop file not found:" << path;
        return result;
    }

    if (tryLaunchDesktopFileDirect(path, launchEnv, &result)) {
        qInfo().noquote() << "[app-launch] launcher direct"
                          << "desktop=" << path
                          << "name=" << result.displayName
                          << "exec=" << result.executable
                          << "launchCommand=" << result.launchCommand
                          << "pid=" << result.pid
                          << "privateSessionBus=" << (result.privateSessionBus ? QStringLiteral("true") : QStringLiteral("false"))
                          << "gioVfs=" << launchEnv.value(QStringLiteral("GIO_USE_VFS"))
                          << "gioVolumeMonitor=" << launchEnv.value(QStringLiteral("GIO_USE_VOLUME_MONITOR"))
                          << "gioFileMonitor=" << launchEnv.value(QStringLiteral("GIO_USE_FILE_MONITOR"))
                          << "noAtBridge=" << launchEnv.value(QStringLiteral("NO_AT_BRIDGE"))
                          << "gsettingsBackend=" << launchEnv.value(QStringLiteral("GSETTINGS_BACKEND"))
                          << "gtkImModule=" << launchEnv.value(QStringLiteral("GTK_IM_MODULE"))
                          << "gtkModules=" << launchEnv.value(QStringLiteral("GTK_MODULES"))
                          << "xModifiers=" << launchEnv.value(QStringLiteral("XMODIFIERS"))
                          << "gdkBackend=" << launchEnv.value(QStringLiteral("GDK_BACKEND"))
                          << "qtQpaPlatform=" << launchEnv.value(QStringLiteral("QT_QPA_PLATFORM"))
                          << "display=" << launchEnv.value(QStringLiteral("DISPLAY"))
                          << "waylandDisplay=" << launchEnv.value(QStringLiteral("WAYLAND_DISPLAY"))
                          << "ok=true";
        return result;
    }

    GDesktopAppInfo *info = g_desktop_app_info_new_from_filename(path.toUtf8().constData());
    if (!info) {
        qWarning().noquote() << "AppLauncher: direct launch failed and desktop file cannot be parsed:"
                             << path << "error=" << result.error;
        return result;
    }
    if (result.displayName.isEmpty()) {
        result.displayName = fromUtf8(g_app_info_get_display_name(G_APP_INFO(info)));
    }
    if (result.executable.isEmpty()) {
        result.executable = fromUtf8(g_app_info_get_executable(G_APP_INFO(info)));
    }
    if (result.iconName.isEmpty()) {
        result.iconName = fromUtf8(g_desktop_app_info_get_string(info, "Icon"));
    }

    // Compatibility fallback for unusual entries such as DBusActivatable apps.
    // It is deliberately detached as an external process so a GLib/DBus timeout
    // cannot block the shell launch path.
    QString fallbackError;
    qint64 fallbackPid = -1;
    result.ok = launchGioDetached(path, launchEnv, &fallbackPid, &fallbackError);
    result.pid = fallbackPid;
    qInfo().noquote() << "[app-launch] launcher gio-detached"
                      << "desktop=" << path
                      << "name=" << result.displayName
                      << "exec=" << result.executable
                      << "pid=" << result.pid
                      << "ok=" << result.ok
                      << "error=" << (result.ok ? QString() : fallbackError);
    if (!result.ok) {
        result.error = fallbackError.isEmpty() ? result.error : fallbackError;
        qWarning().noquote() << "AppLauncher: detached gio fallback failed:"
                             << path << "error=" << result.error;
    }

    g_object_unref(info);
    return result;
}

bool AppLauncher::launchCommand(const QString &command, const QString &workingDirectory,
                                 const QProcessEnvironment &env)
{
    if (command.trimmed().isEmpty()) {
        qWarning() << "AppLauncher: empty command";
        return false;
    }

    QProcess proc;
    proc.setProcessEnvironment(prepareLaunchEnvironment(env));
    proc.setProgram(QStringLiteral("/bin/bash"));
    proc.setArguments({QStringLiteral("-lc"), command.trimmed()});
    if (!workingDirectory.trimmed().isEmpty())
        proc.setWorkingDirectory(workingDirectory.trimmed());

    return proc.startDetached();
}

QString AppLauncher::appIdFromDesktopFile(const QString &desktopFilePath)
{
    // Strip directory and extension to produce a stable short ID.
    return QFileInfo(desktopFilePath).completeBaseName();
}

QString AppLauncher::appIdFromDesktopId(const QString &desktopId)
{
    QString id = desktopId.trimmed();
    if (id.endsWith(QStringLiteral(".desktop"), Qt::CaseInsensitive))
        id.chop(8);
    return id;
}
