#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QLibraryInfo>
#include <QPalette>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStringList>
#include <QUrl>

#include "src/apps/filemanager/include/filemanagerapi.h"
#include "src/apps/filemanager/filemanagerdbusservice.h"
#include "src/apps/filemanager/include/filemanagermodel.h"
#include "src/apps/filemanager/include/filemanagermodelfactory.h"
#include "src/apps/settings/desktopsettingsclient.h"
#include "src/core/icons/themeiconcontroller.h"
#include "src/core/icons/themeiconprovider.h"
#include "src/filemanager/FileManagerShellBridge.h"
#include "src/filemanager/ThumbnailImageProvider.h"
#include "src/filemanager/ops/globalprogresscenter.h"
#include "src/services/fileindex/metadataindexserver.h"

namespace {

void ensureDaemonAvailable(const QString &serviceName, const QString &execName)
{
    QDBusConnectionInterface *busIface = QDBusConnection::sessionBus().interface();
    if (!busIface) {
        return;
    }
    if (busIface->isServiceRegistered(serviceName).value()) {
        return;
    }

    const QString appDir = QFileInfo(QCoreApplication::applicationFilePath()).absolutePath();
    const QString localPath = QDir(appDir).filePath(execName);
    bool started = false;
    if (QFileInfo::exists(localPath)) {
        started = QProcess::startDetached(localPath, {});
    }
    if (!started) {
        QProcess::startDetached(execName, {});
    }
}

QString resolveInitialPath(const QString &raw)
{
    QString p = raw.trimmed();
    if (p.isEmpty()) {
        return QDir::homePath();
    }
    if (p.startsWith(QLatin1Char('~'))) {
        if (p.size() == 1) {
            return QDir::homePath();
        }
        if (p[1] == QLatin1Char('/')) {
            return QDir::homePath() + p.mid(1);
        }
    }
    if (QFileInfo(p).isRelative()) {
        return QDir(QDir::currentPath()).absoluteFilePath(p);
    }
    return p;
}

} // namespace

int main(int argc, char *argv[])
{
    qputenv("QT_QUICK_CONTROLS_FALLBACK_STYLE", "Basic");
    qputenv("QT_QUICK_CONTROLS_STYLE", "SlmStyle");

    QGuiApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("SLM"));
    app.setApplicationName(QStringLiteral("SLM File Manager"));
    app.setApplicationDisplayName(QObject::tr("Files"));
    app.setDesktopFileName(QStringLiteral("slm-filemanager"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("system-file-manager")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("SLM File Manager"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("path"),
                                 QStringLiteral("Folder to open (defaults to $HOME)"),
                                 QStringLiteral("[path]"));
    parser.process(app);

    const QStringList positional = parser.positionalArguments();
    const QString initialPath = resolveInitialPath(positional.isEmpty() ? QString()
                                                                        : positional.first());

    // Bring up daemons we rely on. fileopsd handles copy/move/trash; sharingd
    // backs the folder share dialog. devicesd is autostarted via D-Bus when
    // the sidebar enumerates removable volumes.
    ensureDaemonAvailable(QStringLiteral("org.slm.Desktop.FileOperations"),
                          QStringLiteral("slm-fileopsd"));
    ensureDaemonAvailable(QStringLiteral("org.slm.Sharing"),
                          QStringLiteral("slm-sharingd"));

    QQmlApplicationEngine engine;
    const QString qtQmlImportPath = QLibraryInfo::path(QLibraryInfo::QmlImportsPath);
    if (!qtQmlImportPath.trimmed().isEmpty()) {
        engine.addImportPath(qtQmlImportPath);
    }
    // Ensure embedded resources (qrc:/qt/qml) take precedence over the build
    // tree, so the Slm_Desktop shim shipped with slm-filemanager wins over the
    // full module written by slm-desktop when both are in the import path.
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));

    FileManagerApi fileManagerApi;
    FileManagerShellBridge fileManagerShellBridge(&fileManagerApi);
    MetadataIndexServer metadataIndexServer(&fileManagerApi);
    FileManagerModel fileManagerModel(&fileManagerApi, &metadataIndexServer);
    FileManagerModelFactory fileManagerModelFactory(&fileManagerApi, &metadataIndexServer);
    GlobalProgressCenter globalProgressCenter;
    ThemeIconController themeIconController;
    DesktopSettingsClient desktopSettings;

    const auto applyIconThemePref = [&]() {
        const QString gtkLight = desktopSettings.gtkIconThemeLight().trimmed();
        const QString gtkDark = desktopSettings.gtkIconThemeDark().trimmed();
        const QString kdeLight = desktopSettings.kdeIconThemeLight().trimmed();
        const QString kdeDark = desktopSettings.kdeIconThemeDark().trimmed();
        const QString light = !gtkLight.isEmpty() ? gtkLight : kdeLight;
        const QString dark = !gtkDark.isEmpty() ? gtkDark : kdeDark;
        if (!light.isEmpty() && !dark.isEmpty()) {
            themeIconController.setThemeMapping(light, dark);
        } else {
            themeIconController.useAutoDetectedMapping();
        }
    };
    const auto applyIconThemeMode = [&]() {
        const QString mode = desktopSettings.themeMode().trimmed().toLower();
        bool darkMode = false;
        if (mode == QStringLiteral("dark")) {
            darkMode = true;
        } else if (mode == QStringLiteral("light")) {
            darkMode = false;
        } else {
            darkMode = app.palette().color(QPalette::Window).lightnessF() < 0.5;
        }
        themeIconController.applyForDarkMode(darkMode);
    };
    applyIconThemePref();
    applyIconThemeMode();

    engine.addImageProvider(QStringLiteral("themeicon"), new ThemeIconProvider);
    engine.addImageProvider(QStringLiteral("thumbnail"),
                            new ThumbnailImageProvider(&fileManagerApi));

    QQmlContext *ctx = engine.rootContext();
    ctx->setContextProperty(QStringLiteral("FileManagerApi"), &fileManagerApi);
    ctx->setContextProperty(QStringLiteral("FileManagerShellBridge"), &fileManagerShellBridge);
    ctx->setContextProperty(QStringLiteral("FileManagerModel"), &fileManagerModel);
    ctx->setContextProperty(QStringLiteral("FileManagerModelFactory"), &fileManagerModelFactory);
    ctx->setContextProperty(QStringLiteral("MetadataIndexServer"), &metadataIndexServer);
    ctx->setContextProperty(QStringLiteral("GlobalProgressCenter"), &globalProgressCenter);
    ctx->setContextProperty(QStringLiteral("ThemeIconController"), &themeIconController);
    ctx->setContextProperty(QStringLiteral("DesktopSettings"), &desktopSettings);
    ctx->setContextProperty(QStringLiteral("slmFileManagerInitialPath"), initialPath);
    ctx->setContextProperty(QStringLiteral("slmActionTreeDebug"), false);

    QObject::connect(&desktopSettings, &DesktopSettingsClient::gtkIconThemeLightChanged, &app, [&]() {
        applyIconThemePref();
        applyIconThemeMode();
    });
    QObject::connect(&desktopSettings, &DesktopSettingsClient::gtkIconThemeDarkChanged, &app, [&]() {
        applyIconThemePref();
        applyIconThemeMode();
    });
    QObject::connect(&desktopSettings, &DesktopSettingsClient::kdeIconThemeLightChanged, &app, [&]() {
        applyIconThemePref();
        applyIconThemeMode();
    });
    QObject::connect(&desktopSettings, &DesktopSettingsClient::kdeIconThemeDarkChanged, &app, [&]() {
        applyIconThemePref();
        applyIconThemeMode();
    });
    QObject::connect(&desktopSettings, &DesktopSettingsClient::themeModeChanged, &app, [&]() {
        applyIconThemeMode();
    });

    engine.load(QUrl(QStringLiteral(
        "qrc:/qt/qml/Slm_Desktop/Qml/apps/filemanager/StandaloneMain.qml")));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    FileManagerDbusService dbusService(&engine);

    return app.exec();
}
