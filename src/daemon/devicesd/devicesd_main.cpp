#include <QCoreApplication>
#include <QDebug>

#include "devicesmanager.h"
#include "devicescompatservice.h"
#include "devicesservice.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SLM"));
    QCoreApplication::setApplicationName(QStringLiteral("SLM Devices Daemon"));

    DevicesManager manager;
    DevicesService service(&manager);
    DevicesCompatService legacyService(&manager);
    qInfo().noquote() << "[slm-devicesd] serviceRegistered=" << service.serviceRegistered();
    qInfo().noquote() << "[slm-devicesd] legacyServiceRegistered=" << legacyService.serviceRegistered();

    return app.exec();
}
