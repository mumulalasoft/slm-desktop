#include "devicescompatservice.h"

#include "devicesmanager.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>

namespace {
constexpr const char kService[] = "org.desktop_shell.Desktop.Devices";
constexpr const char kPath[] = "/org/desktop_shell/Desktop/Devices";
}

DevicesCompatService::DevicesCompatService(DevicesManager *manager, QObject *parent)
    : QObject(parent)
    , m_manager(manager)
{
    registerDbusService();
}

DevicesCompatService::~DevicesCompatService()
{
    if (!m_serviceRegistered) {
        return;
    }
    QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(kPath));
    QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(kService));
}

bool DevicesCompatService::serviceRegistered() const
{
    return m_serviceRegistered;
}

QVariantMap DevicesCompatService::Mount(const QString &target)
{
    return m_manager ? m_manager->Mount(target) : QVariantMap{{QStringLiteral("ok"), false}};
}

QVariantMap DevicesCompatService::Eject(const QString &target)
{
    return m_manager ? m_manager->Eject(target) : QVariantMap{{QStringLiteral("ok"), false}};
}

QVariantMap DevicesCompatService::Unlock(const QString &target, const QString &passphrase)
{
    return m_manager ? m_manager->Unlock(target, passphrase) : QVariantMap{{QStringLiteral("ok"), false}};
}

QVariantMap DevicesCompatService::Format(const QString &target, const QString &filesystem)
{
    return m_manager ? m_manager->Format(target, filesystem) : QVariantMap{{QStringLiteral("ok"), false}};
}

void DevicesCompatService::registerDbusService()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }
    QDBusConnectionInterface *iface = bus.interface();
    if (!iface) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }
    if (iface->isServiceRegistered(QString::fromLatin1(kService)).value()) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }
    if (!bus.registerService(QString::fromLatin1(kService))) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }
    const bool ok = bus.registerObject(QString::fromLatin1(kPath),
                                       this,
                                       QDBusConnection::ExportAllSlots |
                                           QDBusConnection::ExportAllProperties);
    if (!ok) {
        bus.unregisterService(QString::fromLatin1(kService));
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }
    m_serviceRegistered = true;
    emit serviceRegisteredChanged();
}

