#include "actiondaemon.h"
#include "actiondadaptors.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDebug>

namespace {
constexpr const char kService[] = "org.slm.Actiond";
constexpr const char kPath[] = "/org/slm/Actiond";
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SLM"));
    QCoreApplication::setApplicationName(QStringLiteral("SLM Action Broker"));

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qCritical() << "[slm-actiond] session bus unavailable";
        return 2;
    }

    Slm::Actiond::ActionDaemon daemon;
    Slm::Actiond::ActiondProviderAdaptor providerAdaptor(&daemon);
    Slm::Actiond::ActiondFrontendAdaptor frontendAdaptor(&daemon);

    if (!bus.registerObject(QString::fromLatin1(kPath),
                            &daemon,
                            QDBusConnection::ExportAdaptors)) {
        qCritical() << "[slm-actiond] failed register object" << bus.lastError().message();
        return 3;
    }

    if (!bus.registerService(QString::fromLatin1(kService))) {
        qCritical() << "[slm-actiond] failed register service" << bus.lastError().message();
        return 4;
    }

    qInfo() << "[slm-actiond] running as" << kService;
    return app.exec();
}
