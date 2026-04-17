#include "../../services/svcmgr/svcmanagerdbusinterface.h"
#include "../../services/svcmgr/svcmanagerservice.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("slm-svcmgrd"));
    app.setOrganizationName(QStringLiteral("SLM"));

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qCritical() << "slm-svcmgrd: cannot connect to D-Bus session bus";
        return 1;
    }

    SvcManagerService service;
    service.start();

    SvcManagerDbusInterface iface(&service);
    if (!iface.registerOn(bus)) {
        qCritical() << "slm-svcmgrd: failed to register D-Bus service org.slm.ServiceManager1:"
                    << bus.lastError().message();
        return 1;
    }

    qInfo() << "slm-svcmgrd: running on org.slm.ServiceManager1";
    return app.exec();
}
