#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QDateTime>
#include <cstdio>
#include <unistd.h>
#include "greeterapp.h"
#include "../../core/system/missingcomponentcontroller.h"

using namespace Qt::StringLiterals;
using namespace Slm::Login;

static FILE *g_logFile = nullptr;

static FILE *openLogFile()
{
    // Prefer a stable path readable after greeter exits.
    // /tmp is writable by any user the greeter runs as.
    FILE *f = fopen("/tmp/slm-greeter.log", "a");
    if (!f) f = fopen("/tmp/slm-greeter-fallback.log", "a");
    return f;
}

static void greeterMessageHandler(QtMsgType type,
                                  const QMessageLogContext &,
                                  const QString &msg)
{
    const char *level = "DBG";
    switch (type) {
    case QtInfoMsg:    level = "INF"; break;
    case QtWarningMsg: level = "WRN"; break;
    case QtCriticalMsg:
    case QtFatalMsg:   level = "ERR"; break;
    default: break;
    }
    const QByteArray ts = QDateTime::currentDateTime()
                              .toString(u"yyyy-MM-dd HH:mm:ss.zzz"_s)
                              .toLocal8Bit();
    const QByteArray line = msg.toLocal8Bit();

    fprintf(stderr, "[slm-greeter][%s][%s] %s\n", ts.constData(), level, line.constData());
    fflush(stderr);

    if (g_logFile) {
        fprintf(g_logFile, "[%s][%s] %s\n", ts.constData(), level, line.constData());
        fflush(g_logFile);
    }
}

static void dumpEnv()
{
    const auto e = [](const char *k) -> QByteArray {
        const QByteArray v = qgetenv(k);
        return v.isEmpty() ? QByteArrayLiteral("<unset>") : v;
    };
    qInfo("env: GREETD_SOCK=%s", e("GREETD_SOCK").constData());
    qInfo("env: XDG_RUNTIME_DIR=%s", e("XDG_RUNTIME_DIR").constData());
    qInfo("env: WAYLAND_DISPLAY=%s", e("WAYLAND_DISPLAY").constData());
    qInfo("env: DISPLAY=%s", e("DISPLAY").constData());
    qInfo("env: QT_QPA_PLATFORM=%s", e("QT_QPA_PLATFORM").constData());
    qInfo("env: SLM_OFFICIAL_SESSION=%s", e("SLM_OFFICIAL_SESSION").constData());
}

int main(int argc, char *argv[])
{
    g_logFile = openLogFile();
    qInstallMessageHandler(greeterMessageHandler);

    qputenv("QT_QUICK_CONTROLS_FALLBACK_STYLE", "Basic");
    qputenv("QT_QUICK_CONTROLS_STYLE", "SlmStyle");
    // Greeter runs on the real framebuffer; disable GPU compositor if needed.
    qputenv("QT_QPA_PLATFORM", qgetenv("QT_QPA_PLATFORM").isEmpty()
                                    ? "wayland;xcb"
                                    : qgetenv("QT_QPA_PLATFORM"));

    qInfo("=== slm-greeter start (pid=%d) ===", static_cast<int>(getpid()));
    if (!g_logFile)
        fprintf(stderr, "[slm-greeter] WARNING: could not open /tmp/slm-greeter.log\n");

    dumpEnv(); // after qputenv so effective QT_QPA_PLATFORM is shown

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
    const int ret = app.exec();

    qInfo("=== slm-greeter exit (code=%d) ===", ret);
    if (g_logFile) {
        fclose(g_logFile);
        g_logFile = nullptr;
    }
    return ret;
}
