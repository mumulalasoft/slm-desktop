#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "recoveryapp.h"
#include "src/core/system/missingcomponentcontroller.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("slm-recovery-app"));
    app.setOrganizationName(QStringLiteral("SLM Desktop"));

    Slm::Login::RecoveryApp recovery;
    Slm::System::MissingComponentController missingComponents;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("RecoveryApp"), &recovery);
    engine.rootContext()->setContextProperty(QStringLiteral("MissingComponents"), &missingComponents);

    const QUrl url(QStringLiteral("qrc:/qt/qml/SlmRecovery/Main.qml"));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated,
        &app, [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(1);
        },
        Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
