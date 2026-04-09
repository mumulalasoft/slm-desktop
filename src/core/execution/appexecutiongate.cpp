#include "appexecutiongate.h"

#include "../../../dockmodel.h"
#include "../../../shortcutmodel.h"
#include "../prefs/uipreferences.h"
#include "../../core/launcher/applauncher.h"
#include "../../core/launcher/launchenvresolver.h"

#include <QFileInfo>
#include <QFile>
#include <QProcess>
#include <QDebug>
#include <QUrl>

#ifdef signals
#undef signals
#define DESKTOP_SHELL_RESTORE_QT_SIGNALS_MACRO
#endif
extern "C" {
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
}
#ifdef DESKTOP_SHELL_RESTORE_QT_SIGNALS_MACRO
#define signals Q_SIGNALS
#undef DESKTOP_SHELL_RESTORE_QT_SIGNALS_MACRO
#endif

namespace {
QString fromUtf8(const char *value)
{
    return value ? QString::fromUtf8(value) : QString();
}

bool launchDesktopFileViaGio(const QString &desktopFilePath,
                             QString *outDisplayName,
                             QString *outExecutable,
                             QString *outIconName)
{
    const QString path = QFileInfo(desktopFilePath).absoluteFilePath();
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        return false;
    }

    GDesktopAppInfo *info = g_desktop_app_info_new_from_filename(path.toUtf8().constData());
    if (!info) {
        return false;
    }

    if (outDisplayName) {
        *outDisplayName = fromUtf8(g_app_info_get_display_name(G_APP_INFO(info)));
    }
    if (outExecutable) {
        *outExecutable = fromUtf8(g_app_info_get_executable(G_APP_INFO(info)));
    }
    if (outIconName) {
        *outIconName = fromUtf8(g_desktop_app_info_get_string(info, "Icon"));
    }

    GError *gerr = nullptr;
    const bool ok = g_app_info_launch(G_APP_INFO(info), nullptr, nullptr, &gerr);
    if (gerr != nullptr) {
        g_error_free(gerr);
    }
    g_object_unref(info);
    return ok;
}
}

AppExecutionGate::AppExecutionGate(DockModel *dockModel, ShortcutModel *shortcutModel,
                                   UIPreferences *uiPreferences,
                                   QObject *desktopSettings,
                                   QObject *parent)
    : QObject(parent)
    , m_dockModel(dockModel)
    , m_shortcutModel(shortcutModel)
    , m_uiPreferences(uiPreferences)
    , m_desktopSettings(desktopSettings)
{
}

void AppExecutionGate::setLaunchEnvResolver(LaunchEnvResolver *resolver)
{
    m_envResolver = resolver;
}

bool AppExecutionGate::launchDesktopEntry(const QString &desktopFilePath, const QString &executable,
                                          const QString &displayName, const QString &iconName,
                                          const QString &iconSource, const QString &source)
{
    bool ok = false;
    QString resolvedName = displayName;
    QString resolvedExec = executable;
    QString resolvedIcon = iconName;

    if (m_dockModel) {
        ok = m_dockModel->activateOrLaunch(desktopFilePath, executable, displayName);
    } else if (!desktopFilePath.trimmed().isEmpty()) {
        if (m_envResolver) {
            // Inject resolved environment via GAppLaunchContext.
            const QString appId = AppLauncher::appIdFromDesktopFile(desktopFilePath);
            const QProcessEnvironment env = m_envResolver->resolve(appId);
            const AppLaunchResult r = AppLauncher::launchDesktopFile(desktopFilePath, env);
            ok = r.ok;
            if (!r.displayName.isEmpty()) resolvedName = r.displayName;
            if (!r.executable.isEmpty())  resolvedExec = r.executable;
            if (!r.iconName.isEmpty())    resolvedIcon = r.iconName;
        } else {
            ok = launchDesktopFileViaGio(desktopFilePath, &resolvedName, &resolvedExec, &resolvedIcon);
        }
    }
    if (ok && m_dockModel) {
        m_dockModel->noteLaunchedEntry(desktopFilePath, executable, displayName, iconName, iconSource);
    }
    if (resolvedName.isEmpty()) {
        resolvedName = QFileInfo(desktopFilePath).completeBaseName();
    }

    if (verboseLoggingEnabled()) {
        qInfo().noquote() << "AppExecutionGate.launchDesktopEntry:"
                          << "source=" << source
                          << "name=" << resolvedName
                          << "desktop=" << desktopFilePath
                          << "exec=" << resolvedExec
                          << "ok=" << ok;
    }
    emit appExecutionRecorded(source, resolvedName, desktopFilePath, resolvedExec, ok);
    return ok;
}

bool AppExecutionGate::launchDesktopId(const QString &desktopId, const QString &source)
{
    QString id = desktopId.trimmed();
    if (id.isEmpty()) {
        emit appExecutionRecorded(source, QStringLiteral("desktopId"), QString(), QString(), false);
        return false;
    }

    GDesktopAppInfo *info = g_desktop_app_info_new(id.toUtf8().constData());
    if (!info && !id.endsWith(QStringLiteral(".desktop"), Qt::CaseInsensitive)) {
        id += QStringLiteral(".desktop");
        info = g_desktop_app_info_new(id.toUtf8().constData());
    }
    if (!info) {
        emit appExecutionRecorded(source, desktopId, QString(), QString(), false);
        return false;
    }

    const QString desktopFilePath = fromUtf8(g_desktop_app_info_get_filename(info));
    const QString executable = fromUtf8(g_app_info_get_executable(G_APP_INFO(info)));
    const QString displayName = fromUtf8(g_app_info_get_display_name(G_APP_INFO(info)));
    const QString iconName = fromUtf8(g_desktop_app_info_get_string(info, "Icon"));
    g_object_unref(info);

    return launchDesktopEntry(desktopFilePath, executable, displayName, iconName, QString(), source);
}

bool AppExecutionGate::launchShortcut(int shortcutIndex, const QVariantMap &entry, const QString &source)
{
    if (!m_shortcutModel || shortcutIndex < 0) {
        emit appExecutionRecorded(source, entry.value(QStringLiteral("name")).toString(),
                                  entry.value(QStringLiteral("desktopFile")).toString(),
                                  entry.value(QStringLiteral("executable")).toString(), false);
        return false;
    }

    const bool ok = m_shortcutModel->launchShortcut(shortcutIndex);
    if (ok && m_dockModel) {
        m_dockModel->noteLaunchedEntry(entry.value(QStringLiteral("desktopFile")).toString(),
                                       entry.value(QStringLiteral("executable")).toString(),
                                       entry.value(QStringLiteral("name")).toString(),
                                       entry.value(QStringLiteral("iconName")).toString(),
                                       entry.value(QStringLiteral("iconSource")).toString());
    }

    if (verboseLoggingEnabled()) {
        qInfo().noquote() << "AppExecutionGate.launchShortcut:"
                          << "source=" << source
                          << "name=" << entry.value(QStringLiteral("name")).toString()
                          << "desktop=" << entry.value(QStringLiteral("desktopFile")).toString()
                          << "exec=" << entry.value(QStringLiteral("executable")).toString()
                          << "index=" << shortcutIndex
                          << "ok=" << ok;
    }
    emit appExecutionRecorded(source,
                              entry.value(QStringLiteral("name")).toString(),
                              entry.value(QStringLiteral("desktopFile")).toString(),
                              entry.value(QStringLiteral("executable")).toString(), ok);
    return ok;
}

bool AppExecutionGate::launchEntryMap(const QVariantMap &entry, const QString &source)
{
    const QString type = entry.value(QStringLiteral("type")).toString().trimmed().toLower();
    const QString desktopFile = entry.value(QStringLiteral("desktopFile")).toString().trimmed();
    const QString desktopId = entry.value(QStringLiteral("desktopId")).toString().trimmed();
    const QString executable = entry.value(QStringLiteral("executable")).toString().trimmed();
    const QString name = entry.value(QStringLiteral("name")).toString().trimmed();
    const QString iconName = entry.value(QStringLiteral("iconName")).toString().trimmed();
    const QString iconSource = entry.value(QStringLiteral("iconSource")).toString().trimmed();
    const QString target = entry.value(QStringLiteral("target")).toString().trimmed();

    if (!desktopFile.isEmpty() || type == QStringLiteral("desktop")) {
        return launchDesktopEntry(desktopFile, executable, name, iconName, iconSource, source);
    }
    if (!desktopId.isEmpty()) {
        return launchDesktopId(desktopId, source);
    }

    if (type == QStringLiteral("web") || type == QStringLiteral("file")) {
        return openTarget(target, source);
    }

    if (!target.isEmpty()) {
        return openTarget(target, source);
    }

    if (!executable.isEmpty()) {
        // Generic command fallback for future terminal/filemanager integration.
        return launchCommand(executable, QString(), source);
    }

    emit appExecutionRecorded(source, name.isEmpty() ? QStringLiteral("entry") : name,
                              desktopFile, executable, false);
    return false;
}

void AppExecutionGate::noteLaunch(const QVariantMap &entry, const QString &source)
{
    if (m_dockModel) {
        m_dockModel->noteLaunchedEntry(entry.value(QStringLiteral("desktopFile")).toString(),
                                       entry.value(QStringLiteral("executable")).toString(),
                                       entry.value(QStringLiteral("name")).toString(),
                                       entry.value(QStringLiteral("iconName")).toString(),
                                       entry.value(QStringLiteral("iconSource")).toString());
    }
    if (verboseLoggingEnabled()) {
        qInfo().noquote() << "AppExecutionGate.noteLaunch:"
                          << "source=" << source
                          << "name=" << entry.value(QStringLiteral("name")).toString()
                          << "desktop=" << entry.value(QStringLiteral("desktopFile")).toString()
                          << "exec=" << entry.value(QStringLiteral("executable")).toString();
    }
    emit appExecutionRecorded(source,
                              entry.value(QStringLiteral("name")).toString(),
                              entry.value(QStringLiteral("desktopFile")).toString(),
                              entry.value(QStringLiteral("executable")).toString(), true);
}

bool AppExecutionGate::launchCommand(const QString &command, const QString &workingDirectory,
                                     const QString &source)
{
    const QString cmd = command.trimmed();
    if (cmd.isEmpty()) {
        emit appExecutionRecorded(source, QStringLiteral("command"), QString(), QString(), false);
        return false;
    }

    bool ok = false;
    qint64 pid = -1;
    if (m_envResolver) {
        const QProcessEnvironment env = m_envResolver->resolve(QString{});
        ok = AppLauncher::launchCommand(cmd, workingDirectory, env);
    } else {
        const QString shell = QStringLiteral("/bin/bash");
        const QStringList args{QStringLiteral("-lc"), cmd};
        ok = QProcess::startDetached(shell, args,
                                     workingDirectory.trimmed().isEmpty() ? QString()
                                                                          : workingDirectory.trimmed(),
                                     &pid);
    }
    if (verboseLoggingEnabled()) {
        qInfo().noquote() << "AppExecutionGate.launchCommand:"
                          << "source=" << source
                          << "command=" << cmd
                          << "cwd=" << workingDirectory
                          << "pid=" << pid
                          << "ok=" << ok;
    }
    emit appExecutionRecorded(source, QStringLiteral("command"), QString(), cmd, ok);
    return ok;
}

bool AppExecutionGate::openTarget(const QString &targetPathOrUrl, const QString &source)
{
    const QString raw = targetPathOrUrl.trimmed();
    if (raw.isEmpty()) {
        emit appExecutionRecorded(source, QStringLiteral("target"), QString(), QString(), false);
        return false;
    }

    QString desktopFilePath;
    QString executable;
    QString displayName;

    QUrl url(raw);
    if (url.isValid() && (url.scheme().startsWith(QStringLiteral("http")) || url.isLocalFile())) {
        if (url.isLocalFile()) {
            const QFileInfo info(url.toLocalFile());
            displayName = info.completeBaseName();
            if (info.suffix().compare(QStringLiteral("desktop"), Qt::CaseInsensitive) == 0) {
                desktopFilePath = info.absoluteFilePath();
            }
        } else {
            displayName = url.toDisplayString();
        }
    } else {
        const QFileInfo info(raw);
        if (info.exists()) {
            displayName = info.completeBaseName();
            if (info.suffix().compare(QStringLiteral("desktop"), Qt::CaseInsensitive) == 0) {
                desktopFilePath = info.absoluteFilePath();
            }
            url = QUrl::fromLocalFile(info.absoluteFilePath());
        } else {
            url = QUrl(raw);
            if (url.isValid()) {
                displayName = url.toDisplayString();
            }
        }
    }

    bool ok = false;
    if (!desktopFilePath.isEmpty()) {
        ok = launchDesktopEntry(desktopFilePath, executable, displayName, QString(), QString(), source);
    } else if (!raw.contains(QLatin1Char('/')) && !raw.contains(QLatin1Char(':'))) {
        ok = launchDesktopId(raw, source);
    } else {
        QString uri = url.toString(QUrl::FullyEncoded);
        if (uri.isEmpty() && QFileInfo(raw).exists()) {
            uri = QUrl::fromLocalFile(QFileInfo(raw).absoluteFilePath()).toString(QUrl::FullyEncoded);
        }
        GFile *file = nullptr;
        if (url.isLocalFile()) {
            const QByteArray localPath = QFile::encodeName(url.toLocalFile());
            file = g_file_new_for_path(localPath.constData());
        } else {
            file = g_file_new_for_uri(uri.toUtf8().constData());
        }

        QString handlerName;
        QString handlerExec;
        QString handlerDesktopFile;
        GError *queryErr = nullptr;
        GAppInfo *handler = file ? g_file_query_default_handler(file, nullptr, &queryErr) : nullptr;
        if (queryErr != nullptr) {
            g_error_free(queryErr);
        }

        if (handler) {
            handlerName = fromUtf8(g_app_info_get_display_name(handler));
            handlerExec = fromUtf8(g_app_info_get_executable(handler));
            if (G_IS_DESKTOP_APP_INFO(handler)) {
                handlerDesktopFile = fromUtf8(
                    g_desktop_app_info_get_filename(G_DESKTOP_APP_INFO(handler)));
            }

            GList *files = nullptr;
            files = g_list_append(files, file);
            GError *launchErr = nullptr;
            ok = g_app_info_launch(handler, files, nullptr, &launchErr);
            if (launchErr != nullptr) {
                g_error_free(launchErr);
            }
            g_list_free(files);
            g_object_unref(handler);
        } else {
            GError *launchErr = nullptr;
            ok = g_app_info_launch_default_for_uri(uri.toUtf8().constData(), nullptr, &launchErr);
            if (launchErr != nullptr) {
                g_error_free(launchErr);
            }
        }

        if (file) {
            g_object_unref(file);
        }

        const QString eventName = !handlerName.isEmpty()
                ? handlerName
                : (!displayName.isEmpty() ? displayName : QStringLiteral("target"));
        const QString eventDesktop = !handlerDesktopFile.isEmpty() ? handlerDesktopFile : desktopFilePath;
        const QString eventExec = !handlerExec.isEmpty() ? handlerExec : raw;

        if (verboseLoggingEnabled()) {
            qInfo().noquote() << "AppExecutionGate.openTarget:"
                              << "source=" << source
                              << "target=" << raw
                              << "uri=" << uri
                              << "handler=" << handlerName
                              << "ok=" << ok;
        }
        emit appExecutionRecorded(source, eventName, eventDesktop, eventExec, ok);
    }
    return ok;
}

bool AppExecutionGate::launchFromFileManager(const QString &targetPathOrUrl)
{
    return openTarget(targetPathOrUrl, QStringLiteral("filemanager"));
}

bool AppExecutionGate::launchFromTerminal(const QString &command, const QString &workingDirectory)
{
    return launchCommand(command, workingDirectory, QStringLiteral("terminal"));
}

bool AppExecutionGate::verboseLoggingEnabled() const
{
    if (m_desktopSettings) {
        QVariant v;
        const bool okInvoke = QMetaObject::invokeMethod(m_desktopSettings, "settingValue",
                                                         Q_RETURN_ARG(QVariant, v),
                                                         Q_ARG(QString, QStringLiteral("debug.verboseLogging")),
                                                         Q_ARG(QVariant, false));
        if (okInvoke) {
            return v.toBool();
        }
    }
    return m_uiPreferences ? m_uiPreferences->verboseLogging() : false;
}
