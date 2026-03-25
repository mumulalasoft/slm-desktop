#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include <QCommandLineParser>
#include <QString>
#include <QLibraryInfo>
#include <QDir>
#include <QFileInfo>

using namespace Qt::StringLiterals;

#include "settingsapp.h"
#include "globalsearchprovider.h"
#include "wallpapermanager.h"
#include "mimeappsmanager.h"
#include "thememanager.h"
#include "fontmanager.h"
#include "../../core/prefs/uipreferences.h"
#include "../../core/icons/themeiconprovider.h"
#include "../../printing/core/PrinterManager.h"
#include "../../printing/core/PrinterAdminService.h"
#include "modules/developer/envvariablecontroller.h"

int main(int argc, char *argv[])
{
    qputenv("QT_QUICK_CONTROLS_FALLBACK_STYLE", "Basic");
    const QString appDir = QFileInfo(QString::fromLocal8Bit(argv[0])).absolutePath();
    const QString currentStylePath = QDir::current().filePath("Style");
    const QString appStylePath     = QDir(appDir).filePath("Style");
    const QString selectedStylePath =
        QFileInfo::exists(currentStylePath) ? currentStylePath : appStylePath;

    QString styleImportRoot;
    if (QFileInfo::exists(selectedStylePath)) {
        qputenv("QT_QUICK_CONTROLS_STYLE", "Style");
        styleImportRoot = QFileInfo(selectedStylePath).absolutePath();
    } else {
        qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    }

    QGuiApplication app(argc, argv);
    app.setOrganizationName("SLM");
    app.setApplicationName("SLM Desktop");
    app.setWindowIcon(QIcon::fromTheme("preferences-system"));

    QCommandLineParser parser;
    parser.setApplicationDescription("System Settings");
    parser.addHelpOption();
    parser.addVersionOption();
    
    QCommandLineOption moduleOption("module", "Open a specific module by ID.", "id");
    QCommandLineOption deepLinkOption(QStringList{QStringLiteral("d"), QStringLiteral("deep-link")},
                                      QStringLiteral("Open a specific settings deep link (e.g. settings://print/pdf-fallback-printer)."),
                                      QStringLiteral("url"));
    parser.addOption(moduleOption);
    parser.addOption(deepLinkOption);
    parser.process(app);

    QString initialModuleId = parser.value(moduleOption);
    const QString initialDeepLink = parser.value(deepLinkOption).trimmed();

    QQmlApplicationEngine engine;
    const QString qtQmlImportPath = QLibraryInfo::path(QLibraryInfo::QmlImportsPath);
    if (!qtQmlImportPath.trimmed().isEmpty())
        engine.addImportPath(qtQmlImportPath);
    if (!styleImportRoot.isEmpty())
        engine.addImportPath(styleImportRoot);
    // Ensure embedded resources (qrc:/qt/qml) take precedence over the
    // filesystem build/ directory so the Theme-only Slm_Desktop shim is
    // found before the full Slm_Desktop module written by appSlm_Desktop.
    engine.addImportPath(u"qrc:/qt/qml"_s);
    engine.addImageProvider(QStringLiteral("icon"), new ThemeIconProvider);
    const QUrl url(u"qrc:/qt/qml/SlmSettings/Qml/apps/settings/Main.qml"_s);
    UIPreferences uiPreferences;
    Slm::Print::PrinterManager printManager;
    Slm::Print::PrinterAdminService printerAdmin;
    EnvVariableController envVarController;
    WallpaperManager wallpaperManager(&uiPreferences);
    MimeAppsManager mimeAppsManager;
    ThemeManager themeManager(&uiPreferences);
    FontManager fontManager(&uiPreferences);
    engine.rootContext()->setContextProperty(QStringLiteral("UIPreferences"), &uiPreferences);
    engine.rootContext()->setContextProperty(QStringLiteral("PrintManager"), &printManager);
    engine.rootContext()->setContextProperty(QStringLiteral("PrinterAdmin"), &printerAdmin);
    engine.rootContext()->setContextProperty(QStringLiteral("WallpaperManager"), &wallpaperManager);
    engine.rootContext()->setContextProperty(QStringLiteral("MimeAppsManager"), &mimeAppsManager);
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeManager"), &themeManager);
    engine.rootContext()->setContextProperty(QStringLiteral("FontManager"), &fontManager);
    engine.rootContext()->setContextProperty(QStringLiteral("EnvVarController"), &envVarController);
    printManager.reload();

    // Create the main application controller
    SettingsApp settingsApp(&engine);
    
    // Setup Global Search D-Bus provider
    GlobalSearchProvider searchProvider(settingsApp.moduleLoader(), settingsApp.searchEngine(), &app);

    QObject::connect(&searchProvider, &GlobalSearchProvider::activationRequested,
                     &settingsApp, [&settingsApp](const QString &id) {
        if (id.startsWith("module:")) {
            settingsApp.openModule(id.mid(QString("module:").size()));
            return;
        }
        if (id.startsWith("setting:")) {
            const QString payload = id.mid(QString("setting:").size());
            const QStringList parts = payload.split('/', Qt::KeepEmptyParts);
            settingsApp.openModuleSetting(parts.value(0), parts.value(1));
            return;
        }
        if (!settingsApp.openDeepLink(id)) {
            settingsApp.openModule(id);
        }
    });

    if (!initialDeepLink.isEmpty()) {
        settingsApp.openDeepLink(initialDeepLink);
    } else if (!initialModuleId.isEmpty()) {
        settingsApp.openModule(initialModuleId);
    }

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url, &settingsApp](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl) {
            QCoreApplication::exit(-1);
            return;
        }
        if (obj && url == objUrl) {
            settingsApp.raiseWindow();
        }
    }, Qt::QueuedConnection);

    engine.load(url);

    return app.exec();
}
