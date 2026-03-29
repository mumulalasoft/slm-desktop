#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include "greeterapp.h"
#include "src/core/system/missingcomponentcontroller.h"

using namespace Qt::StringLiterals;
using namespace Slm::Login;

int main(int argc, char *argv[])
{
    qputenv("QT_QUICK_CONTROLS_FALLBACK_STYLE", "Basic");
    qputenv("QT_QUICK_CONTROLS_STYLE", "SlmStyle");
    // Greeter runs on the real framebuffer; disable GPU compositor if needed.
    qputenv("QT_QPA_PLATFORM", qgetenv("QT_QPA_PLATFORM").isEmpty()
                                    ? "wayland;xcb"
                                    : qgetenv("QT_QPA_PLATFORM"));

    QGuiApplication app(argc, argv);
    app.setOrganizationName(u"SLM"_s);
    app.setApplicationName(u"slm-greeter"_s);

    QQmlApplicationEngine engine;

    GreeterApp greeterApp;
    Slm::System::MissingComponentController missingComponents;
    engine.rootContext()->setContextProperty(u"GreeterApp"_s, &greeterApp);
    engine.rootContext()->setContextProperty(u"MissingComponents"_s, &missingComponents);

    const QUrl url(u"qrc:/qt/qml/SlmGreeter/Qml/greeter/Main.qml"_s);

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, [](){ QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.load(url);
    return app.exec();
}
