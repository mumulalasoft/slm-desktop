#pragma once

#include <QDBusConnection>
#include <QObject>

namespace Slm::Firewall::Dbus {

QString serviceName();
QString objectPath();
QString interfaceName();

bool registerServiceObject(QDBusConnection bus, QObject *object);
void unregisterServiceObject(QDBusConnection bus);

} // namespace Slm::Firewall::Dbus
