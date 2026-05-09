#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <signal.h>
#include <unistd.h>
#include "sessionbroker.h"

using namespace Slm::Login;

static FILE *g_logFile = nullptr;
static int   g_logFd   = -1;   // file descriptor for async-signal-safe writes

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

// --- async-signal-safe signal handler ---

static const char *sigName(int sig)
{
    switch (sig) {
    case SIGTERM: return "SIGTERM";
    case SIGINT:  return "SIGINT";
    case SIGHUP:  return "SIGHUP";
    case SIGPIPE: return "SIGPIPE";
    case SIGABRT: return "SIGABRT";
    default:      return "SIG?";
    }
}

// SA_RESETHAND resets disposition to SIG_DFL before the handler runs,
// so raise(sig) terminates with the correct signal after we log.
static void sigHandler(int sig, siginfo_t *info, void *)
{
    char buf[128];
    int len = 0;

    // "[broker] caught signal=SIGTERM(15) sender_pid=1234\n"
    for (const char *p = "[broker] caught signal="; *p; ++p) buf[len++] = *p;
    for (const char *p = sigName(sig); *p; ++p)             buf[len++] = *p;
    buf[len++] = '(';
    {
        char tmp[12]; int n = 0, v = sig > 0 ? sig : 0;
        if (v == 0) { tmp[n++] = '0'; }
        while (v > 0) { tmp[n++] = '0' + v % 10; v /= 10; }
        for (int i = n - 1; i >= 0; --i) buf[len++] = tmp[i];
    }
    buf[len++] = ')';
    for (const char *p = " sender_pid="; *p; ++p) buf[len++] = *p;
    {
        long long pid = info ? (long long)info->si_pid : -1LL;
        if (pid < 0) { buf[len++] = '-'; pid = -pid; }
        char tmp[20]; int n = 0;
        if (pid == 0) { tmp[n++] = '0'; }
        while (pid > 0) { tmp[n++] = '0' + (int)(pid % 10); pid /= 10; }
        for (int i = n - 1; i >= 0; --i) buf[len++] = tmp[i];
    }
    buf[len++] = '\n';

    const int fds[2] = { STDERR_FILENO, g_logFd };
    for (int fd : fds) {
        if (fd >= 0) write(fd, buf, (size_t)len);
    }

    if (sig == SIGTERM || sig == SIGINT) {
        // Request graceful shutdown: monitorSession() will see the flag,
        // call terminateCompositor(), and let broker exit cleanly.
        // SA_RESETHAND means a second SIGTERM uses SIG_DFL as a kill-of-last-resort.
        SessionBroker::requestTermination();
        return;
    }
    raise(sig);
}

int main(int argc, char *argv[])
{
    g_logFile = fopen("/tmp/slm-session-broker.log", "a");
    if (g_logFile) g_logFd = fileno(g_logFile);
    qInstallMessageHandler(brokerMessageHandler);

    // Detach from greetd's POSIX session so that when greetd's worker exits
    // the kernel does not send SIGHUP to us (session-leader-exit semantics).
    // logind tracks sessions via cgroup, not POSIX session ID, so this is safe.
    if (setsid() < 0)
        qWarning("setsid() failed (already session leader?): %s", strerror(errno));

    // Broker has no app.exec() so Qt's default signal trampolines are inactive.
    // These raw handlers log the signal + sender PID before re-raising.
    {
        const int sigs[] = { SIGTERM, SIGINT, SIGHUP, SIGPIPE, SIGABRT };
        struct sigaction sa = {};
        sa.sa_sigaction = sigHandler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
        for (int s : sigs)
            sigaction(s, &sa, nullptr);
    }

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
