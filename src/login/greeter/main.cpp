#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QDateTime>
#include <cstdio>
#include <cstring>
#include <csignal>
#include <unistd.h>
#ifdef SLM_HAVE_EXECINFO_H
#  include <execinfo.h>
#endif
#include "greeterapp.h"
#include "../../core/system/missingcomponentcontroller.h"

using namespace Qt::StringLiterals;
using namespace Slm::Login;

static FILE *g_logFile = nullptr;

static FILE *openLogFile()
{
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

// Async-signal-safe: write crash info to log file and stderr then re-raise.
static void crashSignalHandler(int sig)
{
    const char *signame = (sig == SIGSEGV) ? "SIGSEGV"
                        : (sig == SIGABRT) ? "SIGABRT"
                        : (sig == SIGBUS)  ? "SIGBUS"
                        :                    "SIGNAL";
    char buf[128];
    int n = snprintf(buf, sizeof(buf),
                     "[slm-greeter] FATAL: %s (signal %d) — greeter crashed\n",
                     signame, sig);
    (void)write(STDERR_FILENO, buf, (size_t)n);
    if (g_logFile) {
        (void)fwrite(buf, 1, (size_t)n, g_logFile);
        (void)fflush(g_logFile);
    }

#ifdef SLM_HAVE_EXECINFO_H
    void *frames[32];
    int count = backtrace(frames, 32);
    backtrace_symbols_fd(frames, count, STDERR_FILENO);
#endif

    // Reset to default and re-raise so the process gets the correct exit status.
    struct sigaction sa{};
    sa.sa_handler = SIG_DFL;
    sigaction(sig, &sa, nullptr);
    raise(sig);
}

static void termSignalHandler(int /*sig*/)
{
    const char msg[] = "[slm-greeter] received SIGTERM — exiting cleanly\n";
    (void)write(STDERR_FILENO, msg, sizeof(msg) - 1);
    if (g_logFile) {
        (void)fwrite(msg, 1, sizeof(msg) - 1, g_logFile);
        (void)fflush(g_logFile);
    }
    QCoreApplication::exit(0);
}

static void installSignalHandlers()
{
    struct sigaction sa{};
    sigemptyset(&sa.sa_mask);

    sa.sa_handler = crashSignalHandler;
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGBUS,  &sa, nullptr);

    sa.sa_handler = termSignalHandler;
    sigaction(SIGTERM, &sa, nullptr);
}

static void dumpEnv()
{
    const auto e = [](const char *k) -> QByteArray {
        const QByteArray v = qgetenv(k);
        return v.isEmpty() ? QByteArrayLiteral("<unset>") : v;
    };
    qInfo("env: GREETD_SOCK=%s",        e("GREETD_SOCK").constData());
    qInfo("env: XDG_RUNTIME_DIR=%s",    e("XDG_RUNTIME_DIR").constData());
    qInfo("env: WAYLAND_DISPLAY=%s",    e("WAYLAND_DISPLAY").constData());
    qInfo("env: DISPLAY=%s",            e("DISPLAY").constData());
    qInfo("env: QT_QPA_PLATFORM=%s",    e("QT_QPA_PLATFORM").constData());
    qInfo("env: WLR_RENDERER=%s",       e("WLR_RENDERER").constData());
    qInfo("env: QT_QUICK_BACKEND=%s",   e("QT_QUICK_BACKEND").constData());
    qInfo("env: SLM_OFFICIAL_SESSION=%s", e("SLM_OFFICIAL_SESSION").constData());
}

int main(int argc, char *argv[])
{
    g_logFile = openLogFile();
    qInstallMessageHandler(greeterMessageHandler);
    installSignalHandlers();

    qputenv("QT_QUICK_CONTROLS_FALLBACK_STYLE", "Basic");
    qputenv("QT_QUICK_CONTROLS_STYLE", "SlmStyle");
    // Always allow xcb fallback — greetd sets QT_QPA_PLATFORM=wayland but if
    // cage's socket is not yet visible Qt will abort without a fallback.
    const QByteArray qpa = qgetenv("QT_QPA_PLATFORM");
    if (qpa.isEmpty() || qpa == "wayland") {
        qputenv("QT_QPA_PLATFORM", "wayland;xcb");
    }

    qInfo("=== slm-greeter start (pid=%d) ===", static_cast<int>(getpid()));
    if (!g_logFile)
        fprintf(stderr, "[slm-greeter] WARNING: could not open /tmp/slm-greeter.log\n");

    dumpEnv();

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
        &app, [&url](const QUrl &failedUrl) {
            qCritical("QML load failed: %s", qPrintable(failedUrl.toString()));
            if (failedUrl == url)
                QCoreApplication::exit(-1);
        },
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
