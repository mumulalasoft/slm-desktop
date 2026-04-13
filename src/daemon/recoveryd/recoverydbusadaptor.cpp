#include "recoverydbusadaptor.h"

#include <QDBusConnectionInterface>

namespace Slm::Recovery::Dbus {

namespace {
constexpr const char kService[] = "org.slm.Desktop.Recovery";
constexpr const char kPath[] = "/org/slm/Desktop/Recovery";
constexpr const char kIface[] = "org.slm.Desktop.Recovery";
}

QString serviceName()
{
    return QString::fromLatin1(kService);
}

QString objectPath()
{
    return QString::fromLatin1(kPath);
}

QString interfaceName()
{
    return QString::fromLatin1(kIface);
}

bool registerServiceObject(QDBusConnection bus, QObject *object)
{
    if (!bus.isConnected() || !object) {
        return false;
    }

    QDBusConnectionInterface *iface = bus.interface();
    if (!iface) {
        return false;
    }

    if (iface->isServiceRegistered(serviceName()).value()) {
        return false;
    }

    if (!bus.registerService(serviceName())) {
        return false;
    }

    const bool ok = bus.registerObject(objectPath(),
                                       object,
                                       QDBusConnection::ExportAllSlots
                                           | QDBusConnection::ExportAllSignals
                                           | QDBusConnection::ExportAllProperties);
    if (!ok) {
        bus.unregisterService(serviceName());
        return false;
    }

    return true;
}

void unregisterServiceObject(QDBusConnection bus)
{
    if (!bus.isConnected()) {
        return;
    }
    bus.unregisterObject(objectPath());
    bus.unregisterService(serviceName());
}

} // namespace Slm::Recovery::Dbus
