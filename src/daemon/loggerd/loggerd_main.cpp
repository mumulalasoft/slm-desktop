#include "../../services/logger/logdbusinterface.h"
#include "../../services/logger/logservice.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("slm-loggerd"));
    app.setOrganizationName(QStringLiteral("SLM"));

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qCritical() << "slm-loggerd: cannot connect to D-Bus session bus";
        return 1;
    }

    LogService service;
    // Best-effort journalctl follow — not fatal if unavailable.
    service.start();

    LogDbusInterface iface(&service);
    if (!iface.registerOn(bus)) {
        qCritical() << "slm-loggerd: failed to register D-Bus service org.slm.Logger1:"
                    << bus.lastError().message();
        return 1;
    }

    qInfo() << "slm-loggerd: running on org.slm.Logger1";
    return app.exec();
}
