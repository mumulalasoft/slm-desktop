#include "dbusbinding.h"

#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QDBusVariant>

namespace {
constexpr const char kDbusPropertiesIface[] = "org.freedesktop.DBus.Properties";
}

DBusBinding::DBusBinding(const QString &service,
                         const QString &path,
                         const QString &interfaceName,
                         const QString &member,
                         Mode mode,
                         const QDBusConnection &bus,
                         QObject *parent)
    : SettingBinding(parent)
    , m_service(service.trimmed())
    , m_path(path.trimmed())
    , m_interfaceName(interfaceName.trimmed())
    , m_member(member.trimmed())
    , m_mode(mode)
    , m_bus(bus)
{
}

QVariant DBusBinding::value() const
{
    if (m_service.isEmpty() || m_path.isEmpty() || m_interfaceName.isEmpty() || m_member.isEmpty()) {
        return {};
    }

    if (m_mode == Mode::Method) {
        QDBusInterface iface(m_service, m_path, m_interfaceName, m_bus);
        if (!iface.isValid()) {
            return {};
        }
        const QDBusMessage reply = iface.call(m_member);
        if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
            return {};
        }
        return reply.arguments().constFirst();
    }

    QDBusInterface props(m_service, m_path, QString::fromLatin1(kDbusPropertiesIface), m_bus);
    if (!props.isValid()) {
        return {};
    }
    QDBusReply<QDBusVariant> reply =
        props.call(QStringLiteral("Get"), m_interfaceName, m_member);
    if (!reply.isValid()) {
        return {};
    }
    return reply.value().variant();
}

void DBusBinding::setValue(const QVariant &newValue)
{
    if (m_service.isEmpty() || m_path.isEmpty() || m_interfaceName.isEmpty() || m_member.isEmpty()) {
        endOperation(QStringLiteral("write"), false, QStringLiteral("invalid-dbus-binding"));
        return;
    }
    beginOperation();

    if (m_mode == Mode::Method) {
        QDBusInterface iface(m_service, m_path, m_interfaceName, m_bus);
        if (!iface.isValid()) {
            endOperation(QStringLiteral("write"), false, QStringLiteral("dbus-interface-unavailable"));
            return;
        }
        QDBusPendingCall pending = newValue.isValid()
                                       ? iface.asyncCall(m_member, newValue)
                                       : iface.asyncCall(m_member);
        auto *watcher = new QDBusPendingCallWatcher(pending, this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this,
                [this](QDBusPendingCallWatcher *w) {
                    const QDBusPendingReply<QVariant> reply = *w;
                    if (reply.isError()) {
                        endOperation(QStringLiteral("write"), false, reply.error().message());
                        w->deleteLater();
                        return;
                    }
                    emit valueChanged();
                    endOperation(QStringLiteral("write"), true);
                    w->deleteLater();
                });
        return;
    }

    QDBusInterface props(m_service, m_path, QString::fromLatin1(kDbusPropertiesIface), m_bus);
    if (!props.isValid()) {
        endOperation(QStringLiteral("write"), false, QStringLiteral("dbus-properties-unavailable"));
        return;
    }
    QVariantList args;
    args << m_interfaceName
         << m_member
         << QVariant::fromValue(QDBusVariant(newValue));
    QDBusPendingCall pending = props.asyncCallWithArgumentList(QStringLiteral("Set"), args);
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *w) {
                const QDBusPendingReply<> reply = *w;
                if (reply.isError()) {
                    endOperation(QStringLiteral("write"), false, reply.error().message());
                    w->deleteLater();
                    return;
                }
                emit valueChanged();
                endOperation(QStringLiteral("write"), true);
                w->deleteLater();
            });
}
