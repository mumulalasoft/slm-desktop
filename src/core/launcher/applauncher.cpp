#include "applauncher.h"

#include <QDebug>
#include <QFileInfo>
#include <QProcess>

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

AppLaunchResult AppLauncher::launchDesktopFile(const QString &desktopFilePath,
                                                const QProcessEnvironment &env)
{
    AppLaunchResult result;
    result.desktopFilePath = desktopFilePath;

    const QString path = QFileInfo(desktopFilePath).absoluteFilePath();
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        qWarning() << "AppLauncher: desktop file not found:" << path;
        return result;
    }

    GDesktopAppInfo *info = g_desktop_app_info_new_from_filename(path.toUtf8().constData());
    if (!info) {
        qWarning() << "AppLauncher: cannot parse desktop file:" << path;
        return result;
    }

    result.displayName  = fromUtf8(g_app_info_get_display_name(G_APP_INFO(info)));
    result.executable   = fromUtf8(g_app_info_get_executable(G_APP_INFO(info)));
    result.iconName     = fromUtf8(g_desktop_app_info_get_string(info, "Icon"));

    // Build a GAppLaunchContext and apply the resolved environment.
    GAppLaunchContext *ctx = g_app_launch_context_new();
    const QStringList keys = env.keys();
    for (const QString &key : keys) {
        const QByteArray k = key.toUtf8();
        const QByteArray v = env.value(key).toUtf8();
        g_app_launch_context_setenv(ctx, k.constData(), v.constData());
    }

    GError *gerr = nullptr;
    result.ok = g_app_info_launch(G_APP_INFO(info), nullptr, ctx, &gerr);
    if (gerr != nullptr) {
        qWarning() << "AppLauncher: GIO launch error:" << gerr->message;
        g_error_free(gerr);
    }

    g_object_unref(ctx);
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
    proc.setProcessEnvironment(env);
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
