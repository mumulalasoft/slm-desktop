#include "../../services/envd/dbusinterface.h"
#include "../../services/envd/envservice.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("slm-envd"));
    app.setOrganizationName(QStringLiteral("SLM"));

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qCritical() << "slm-envd: cannot connect to D-Bus session bus";
        return 1;
    }

    EnvService service;
    if (!service.start()) {
        qCritical() << "slm-envd: failed to load user environment store";
        return 1;
    }

    DbusInterface iface(&service);
    if (!iface.registerOn(bus)) {
        qCritical() << "slm-envd: failed to register D-Bus service org.slm.Environment1:"
                    << bus.lastError().message();
        return 1;
    }

    qInfo() << "slm-envd: running on org.slm.Environment1";
    return app.exec();
}
