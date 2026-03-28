#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include <QCommandLineParser>
#include <QString>
#include <QLibraryInfo>
#include <QDir>
#include <QFileInfo>
#include <QPalette>

using namespace Qt::StringLiterals;

#include "settingsapp.h"
#include "globalsearchprovider.h"
#include "wallpapermanager.h"
#include "mimeappsmanager.h"
#include "thememanager.h"
#include "fontmanager.h"
#include "modules/useraccounts/useraccountscontroller.h"
#include "../../core/prefs/uipreferences.h"
#include "../../core/icons/themeiconprovider.h"
#include "../../core/icons/themeiconcontroller.h"
#include "../../printing/core/PrinterManager.h"
#include "../../printing/core/PrinterAdminService.h"
#include "../../core/system/missingcomponentcontroller.h"
#include "modules/developer/envvariablecontroller.h"
#include "modules/developer/envserviceclient.h"
#include "modules/developer/effectiveenvpreviewcontroller.h"
#include "modules/developer/perappoverridescontroller.h"
#include "modules/developer/developermode.h"
#include "modules/developer/buildinfocontroller.h"
#include "modules/developer/logserviceclient.h"
#include "modules/developer/logscontroller.h"
#include "modules/developer/svcmanagerclient.h"
#include "modules/developer/processservicescontroller.h"
#include "modules/developer/featureflagscontroller.h"
#include "modules/developer/dbusinspectorcontroller.h"
#include "modules/developer/appsandboxcontroller.h"
#include "modules/developer/systemenvcontroller.h"
#include "modules/developer/xdgportalscontroller.h"
#include "modules/applications/startupappscontroller.h"
#include "modules/developer/componenthealthcontroller.h"
#include "modules/developer/daemonhealthclient.h"

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
    QCommandLineOption styleGalleryOption(QStringLiteral("style-gallery"),
                                          QStringLiteral("Open style preview gallery window for visual QA."));
    parser.addOption(moduleOption);
    parser.addOption(deepLinkOption);
    parser.addOption(styleGalleryOption);
    parser.process(app);

    QString initialModuleId = parser.value(moduleOption);
    const QString initialDeepLink = parser.value(deepLinkOption).trimmed();
    const bool openStyleGallery = parser.isSet(styleGalleryOption);

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
    const QUrl url = openStyleGallery
        ? QUrl(u"qrc:/qt/qml/SlmSettings/Qml/apps/settings/StylePreviewGallery.qml"_s)
        : QUrl(u"qrc:/qt/qml/SlmSettings/Qml/apps/settings/Main.qml"_s);
    UIPreferences uiPreferences;
    ThemeIconController themeIconController;
    Slm::Print::PrinterManager printManager;
    Slm::Print::PrinterAdminService printerAdmin;
    EnvVariableController envVarController;
    EnvServiceClient envServiceClient;
    EffectiveEnvPreviewController effectiveEnvPreview(&envServiceClient);
    PerAppOverridesController perAppOverrides(&envServiceClient);
    DeveloperModeController developerMode;
    BuildInfoController buildInfo;
    LogServiceClient logServiceClient;
    LogsController logsController(&logServiceClient);
    SvcManagerClient svcManagerClient;
    ProcessServicesController processServicesController(&svcManagerClient);
    FeatureFlagsController featureFlags;
    DbusInspectorController dbusInspector;
    AppSandboxController    appSandbox;
    SystemEnvController     systemEnv(&envServiceClient);
    XdgPortalsController    xdgPortals;
    StartupAppsController   startupApps;
    ComponentHealthController componentHealth;
    DaemonHealthClient daemonHealthClient;
    Slm::System::MissingComponentController missingComponents;
    WallpaperManager wallpaperManager(&uiPreferences);
    MimeAppsManager mimeAppsManager;
    ThemeManager themeManager(&uiPreferences);
    FontManager fontManager(&uiPreferences);
    UserAccountsController userAccounts;
    const auto applyIconThemePref = [&]() {
        const QString light = uiPreferences.iconThemeLight().trimmed();
        const QString dark = uiPreferences.iconThemeDark().trimmed();
        if (!light.isEmpty() && !dark.isEmpty()) {
            themeIconController.setThemeMapping(light, dark);
        } else {
            themeIconController.useAutoDetectedMapping();
        }
    };
    const auto applyIconThemeMode = [&]() {
        const QString mode = uiPreferences.themeMode().trimmed().toLower();
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
    engine.rootContext()->setContextProperty(QStringLiteral("UIPreferences"), &uiPreferences);
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeIconController"), &themeIconController);
    engine.rootContext()->setContextProperty(QStringLiteral("PrintManager"), &printManager);
    engine.rootContext()->setContextProperty(QStringLiteral("PrinterAdmin"), &printerAdmin);
    engine.rootContext()->setContextProperty(QStringLiteral("WallpaperManager"), &wallpaperManager);
    engine.rootContext()->setContextProperty(QStringLiteral("MimeAppsManager"), &mimeAppsManager);
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeManager"), &themeManager);
    engine.rootContext()->setContextProperty(QStringLiteral("FontManager"), &fontManager);
    engine.rootContext()->setContextProperty(QStringLiteral("UserAccounts"), &userAccounts);
    engine.rootContext()->setContextProperty(QStringLiteral("EnvVarController"), &envVarController);
    engine.rootContext()->setContextProperty(QStringLiteral("EnvServiceClient"), &envServiceClient);
    engine.rootContext()->setContextProperty(QStringLiteral("EffectiveEnvPreview"), &effectiveEnvPreview);
    engine.rootContext()->setContextProperty(QStringLiteral("PerAppOverrides"), &perAppOverrides);
    engine.rootContext()->setContextProperty(QStringLiteral("DeveloperMode"), &developerMode);
    engine.rootContext()->setContextProperty(QStringLiteral("BuildInfo"), &buildInfo);
    engine.rootContext()->setContextProperty(QStringLiteral("LogServiceClient"), &logServiceClient);
    engine.rootContext()->setContextProperty(QStringLiteral("LogsController"), &logsController);
    engine.rootContext()->setContextProperty(QStringLiteral("SvcManagerClient"), &svcManagerClient);
    engine.rootContext()->setContextProperty(QStringLiteral("ProcessServicesController"), &processServicesController);
    engine.rootContext()->setContextProperty(QStringLiteral("FeatureFlags"), &featureFlags);
    engine.rootContext()->setContextProperty(QStringLiteral("DbusInspector"), &dbusInspector);
    engine.rootContext()->setContextProperty(QStringLiteral("AppSandbox"),           &appSandbox);
    engine.rootContext()->setContextProperty(QStringLiteral("SystemEnvController"), &systemEnv);
    engine.rootContext()->setContextProperty(QStringLiteral("XdgPortals"),          &xdgPortals);
    engine.rootContext()->setContextProperty(QStringLiteral("StartupAppsController"), &startupApps);
    engine.rootContext()->setContextProperty(QStringLiteral("ComponentHealth"), &componentHealth);
    engine.rootContext()->setContextProperty(QStringLiteral("DaemonHealthClient"), &daemonHealthClient);
    engine.rootContext()->setContextProperty(QStringLiteral("MissingComponents"), &missingComponents);
    QObject::connect(&uiPreferences, &UIPreferences::iconThemeLightChanged, &app, [&]() {
        applyIconThemePref();
        applyIconThemeMode();
    });
    QObject::connect(&uiPreferences, &UIPreferences::iconThemeDarkChanged, &app, [&]() {
        applyIconThemePref();
        applyIconThemeMode();
    });
    QObject::connect(&uiPreferences, &UIPreferences::themeModeChanged, &app, [&]() {
        applyIconThemeMode();
    });
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

    if (!openStyleGallery) {
        if (!initialDeepLink.isEmpty()) {
            settingsApp.openDeepLink(initialDeepLink);
        } else if (!initialModuleId.isEmpty()) {
            settingsApp.openModule(initialModuleId);
        }
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
