#include "FsdService.h"

#include <QCoreApplication>
#include <QDebug>
#include <csignal>

// Signal handler: let the event loop exit cleanly on SIGTERM/SIGINT.
// We use the classic "write to a pipe" trick via QSocketNotifier in production;
// for Phase 0 the simple approach is sufficient.
static void handleSignal(int /*sig*/)
{
    QCoreApplication::quit();
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("SLM"));
    app.setApplicationName(QStringLiteral("slm-fsd"));

    std::signal(SIGTERM, handleSignal);
    std::signal(SIGINT,  handleSignal);

    FsdService service;
    QString error;
    if (!service.start(&error)) {
        qCritical().noquote() << "[slm-fsd] failed to start:" << error;
        return 1;
    }

    qInfo().noquote() << "[slm-fsd] serviceRegistered=" << service.serviceRegistered()
                      << "apiVersion=" << service.apiVersion();

    return app.exec();
}
