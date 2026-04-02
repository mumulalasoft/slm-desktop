#include <QDebug>
#include <QGuiApplication>
#include <QLibraryInfo>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QString>

#include "../../core/prefs/uipreferences.h"
#include "authdialogcontroller.h"
#include "polkitagentapp.h"

using namespace Qt::StringLiterals;

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName(u"SLM"_s);
    // Share the same UI preference scope as shell/settings so theme updates
    // (mode, accent, font scale) are reflected in auth dialog runtime.
    app.setApplicationName(u"SLM Desktop"_s);

    Slm::Login::PolkitAgentApp agent;
    QString error;
    if (!agent.start(&error)) {
        qWarning().noquote() << "[slm-polkit-agent] failed to start:" << error;
        return 1;
    }

    QQmlApplicationEngine engine;
    const QString qtQmlImportPath = QLibraryInfo::path(QLibraryInfo::QmlImportsPath);
    if (!qtQmlImportPath.trimmed().isEmpty()) {
        engine.addImportPath(qtQmlImportPath);
    }
    engine.addImportPath(u"qrc:/qt/qml"_s);
    UIPreferences uiPreferences;
    engine.rootContext()->setContextProperty(u"authDialogController"_s, agent.dialogController());
    engine.rootContext()->setContextProperty(u"UIPreferences"_s, &uiPreferences);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() {
            qWarning().noquote() << "[slm-polkit-agent] failed to load AuthDialog QML";
            QCoreApplication::exit(2);
        },
        Qt::QueuedConnection);
    engine.loadFromModule(u"SlmPolkitAgent"_s, u"AuthDialog"_s);

    return app.exec();
}
