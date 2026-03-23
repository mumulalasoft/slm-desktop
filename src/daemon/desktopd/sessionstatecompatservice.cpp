#include "sessionstatecompatservice.h"

#include "sessionstatemanager.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>

namespace {
constexpr const char kLegacyService[] = "org.desktop_shell.SessionState";
constexpr const char kLegacyPath[] = "/org/desktop_shell/SessionState";
}

SessionStateCompatService::SessionStateCompatService(SessionStateManager *manager, QObject *parent)
    : QObject(parent)
    , m_manager(manager)
{
    registerDbusService();

    if (m_manager) {
        connect(m_manager, &SessionStateManager::SessionLocked,
                this, &SessionStateCompatService::SessionLocked);
        connect(m_manager, &SessionStateManager::SessionUnlocked,
                this, &SessionStateCompatService::SessionUnlocked);
        connect(m_manager, &SessionStateManager::IdleChanged,
                this, &SessionStateCompatService::IdleChanged);
        connect(m_manager, &SessionStateManager::ActiveAppChanged,
                this, &SessionStateCompatService::ActiveAppChanged);
    }
}

SessionStateCompatService::~SessionStateCompatService()
{
    if (!m_serviceRegistered) {
        return;
    }
    QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(kLegacyPath));
    QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(kLegacyService));
}

bool SessionStateCompatService::serviceRegistered() const
{
    return m_serviceRegistered;
}

uint SessionStateCompatService::Inhibit(const QString &reason, uint flags)
{
    return m_manager ? m_manager->Inhibit(reason, flags) : 0u;
}

void SessionStateCompatService::Logout()
{
    if (m_manager) {
        m_manager->Logout();
    }
}

void SessionStateCompatService::Shutdown()
{
    if (m_manager) {
        m_manager->Shutdown();
    }
}

void SessionStateCompatService::Reboot()
{
    if (m_manager) {
        m_manager->Reboot();
    }
}

void SessionStateCompatService::Lock()
{
    if (m_manager) {
        m_manager->Lock();
    }
}

QVariantMap SessionStateCompatService::Unlock(const QString &password)
{
    Q_UNUSED(password)
    // Legacy interface does not implement authenticated unlock.
    return {
        {QStringLiteral("ok"), false},
        {QStringLiteral("error"), QStringLiteral("unsupported")},
    };
}

void SessionStateCompatService::registerDbusService()
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

    if (iface->isServiceRegistered(QString::fromLatin1(kLegacyService)).value()) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    if (!bus.registerService(QString::fromLatin1(kLegacyService))) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    const bool ok = bus.registerObject(QString::fromLatin1(kLegacyPath),
                                       this,
                                       QDBusConnection::ExportAllSlots |
                                           QDBusConnection::ExportAllSignals |
                                           QDBusConnection::ExportAllProperties);
    if (!ok) {
        bus.unregisterService(QString::fromLatin1(kLegacyService));
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    m_serviceRegistered = true;
    emit serviceRegisteredChanged();
}
