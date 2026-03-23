#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include <QCommandLineParser>
#include <QString>
#include <QLibraryInfo>

using namespace Qt::StringLiterals;

#include "settingsapp.h"
#include "globalsearchprovider.h"
#include "../../core/prefs/uipreferences.h"
#include "../../printing/core/PrinterManager.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("Antigravity");
    app.setApplicationName("SlmSettings");
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
    if (!qtQmlImportPath.trimmed().isEmpty()) {
        engine.addImportPath(qtQmlImportPath);
    }
    const QUrl url(u"qrc:/qt/qml/SlmSettings/Qml/apps/settings/Main.qml"_s);
    UIPreferences uiPreferences;
    Slm::Print::PrinterManager printManager;
    engine.rootContext()->setContextProperty(QStringLiteral("UIPreferences"), &uiPreferences);
    engine.rootContext()->setContextProperty(QStringLiteral("PrintManager"), &printManager);
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
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    
    engine.load(url);

    return app.exec();
}
