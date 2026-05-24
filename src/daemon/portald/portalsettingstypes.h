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

// D-Bus struct (ddd) for org.freedesktop.impl.portal.Screenshot.PickColor results
struct PortalColorValue {
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
};

Q_DECLARE_METATYPE(PortalColorValue)

inline QDBusArgument &operator<<(QDBusArgument &arg, const PortalColorValue &color)
{
    arg.beginStructure();
    arg << color.r << color.g << color.b;
    arg.endStructure();
    return arg;
}

inline const QDBusArgument &operator>>(const QDBusArgument &arg, PortalColorValue &color)
{
    arg.beginStructure();
    arg >> color.r >> color.g >> color.b;
    arg.endStructure();
    return arg;
}
