#pragma once

#include <QDBusConnection>
#include <QObject>

namespace Slm::Recovery::Dbus {

QString serviceName();
QString objectPath();
QString interfaceName();

bool registerServiceObject(QDBusConnection bus, QObject *object);
void unregisterServiceObject(QDBusConnection bus);

} // namespace Slm::Recovery::Dbus
