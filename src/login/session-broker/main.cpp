#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <cstdio>
#include <unistd.h>
#include "sessionbroker.h"

using namespace Slm::Login;

static FILE *g_logFile = nullptr;

static void brokerMessageHandler(QtMsgType type,
                                 const QMessageLogContext &,
                                 const QString &msg)
{
    const char *level = "DBG";
    switch (type) {
    case QtInfoMsg:     level = "INF"; break;
    case QtWarningMsg:  level = "WRN"; break;
    case QtCriticalMsg:
    case QtFatalMsg:    level = "ERR"; break;
    default: break;
    }
    const QByteArray ts = QDateTime::currentDateTime()
                              .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
                              .toLocal8Bit();
    const QByteArray line = msg.toLocal8Bit();

    fprintf(stderr, "[slm-session-broker][%s][%s] %s\n",
            ts.constData(), level, line.constData());
    fflush(stderr);

    if (g_logFile) {
        fprintf(g_logFile, "[%s][%s] %s\n", ts.constData(), level, line.constData());
        fflush(g_logFile);
    }
}

int main(int argc, char *argv[])
{
    g_logFile = fopen("/tmp/slm-session-broker.log", "a");
    qInstallMessageHandler(brokerMessageHandler);

    QCoreApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("SLM"));
    app.setApplicationName(QStringLiteral("slm-session-broker"));
    app.setApplicationVersion(QStringLiteral("1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral(
        "SLM Desktop session broker — validates state, selects startup mode, "
        "launches compositor and shell."));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption modeOpt(
        QStringList{QStringLiteral("m"), QStringLiteral("mode")},
        QStringLiteral("Startup mode: normal|safe|recovery"),
        QStringLiteral("mode"),
        QStringLiteral("normal"));
    parser.addOption(modeOpt);
    parser.process(app);

    const StartupMode mode = startupModeFromString(parser.value(modeOpt));
    qInfo("=== slm-session-broker start (pid=%d) ===", static_cast<int>(getpid()));

    SessionBroker broker(mode);
    // run() is synchronous — it blocks until the compositor exits.
    // We do not call app.exec(); the event loop is not needed here.
    const int ret = broker.run();

    qInfo("=== slm-session-broker exit (code=%d) ===", ret);
    if (g_logFile) {
        fclose(g_logFile);
        g_logFile = nullptr;
    }
    return ret;
}
