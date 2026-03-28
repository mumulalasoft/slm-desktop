#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QString>

#include "authdialogcontroller.h"
#include "polkitagentapp.h"

using namespace Qt::StringLiterals;

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName(u"SLM"_s);
    app.setApplicationName(u"slm-polkit-agent"_s);

    Slm::Login::PolkitAgentApp agent;
    QString error;
    if (!agent.start(&error)) {
        qWarning().noquote() << "[slm-polkit-agent] failed to start:" << error;
        return 1;
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(u"authDialogController"_s, agent.dialogController());
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
