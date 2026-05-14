#pragma once

#include <QDBusArgument>
#include <QMap>
#include <QMetaType>
#include <QString>
#include <QVariantMap>

using PortalSettingsMap = QMap<QString, QVariantMap>;

Q_DECLARE_METATYPE(PortalSettingsMap)
QDBusArgument &operator<<(QDBusArgument &argument, const PortalSettingsMap &settings);
const QDBusArgument &operator>>(const QDBusArgument &argument, PortalSettingsMap &settings);
