#include "../../services/envd-helper/helperdbusinterface.h"
#include "../../services/envd-helper/helperservice.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("slm-envd-helper"));
    app.setOrganizationName(QStringLiteral("SLM"));

    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        qCritical() << "slm-envd-helper: cannot connect to D-Bus system bus";
        return 1;
    }

    HelperService service;
    HelperDbusInterface iface(&service);
    if (!iface.registerOn(bus)) {
        qCritical() << "slm-envd-helper: failed to register D-Bus service"
                    << "org.slm.EnvironmentHelper1:"
                    << bus.lastError().message();
        return 1;
    }

    qInfo() << "slm-envd-helper: running on org.slm.EnvironmentHelper1 (system bus)";
    return app.exec();
}
