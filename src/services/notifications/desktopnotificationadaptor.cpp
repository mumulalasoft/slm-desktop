#include "desktopnotificationadaptor.h"

#include "notificationmanager.h"

DesktopNotificationAdaptor::DesktopNotificationAdaptor(NotificationManager *manager)
    : QDBusAbstractAdaptor(manager)
    , m_manager(manager)
{
    if (m_manager) {
        connect(m_manager, &NotificationManager::NotificationAdded,
                this, &DesktopNotificationAdaptor::NotificationAdded);
        connect(m_manager, &NotificationManager::NotificationRemoved,
                this, &DesktopNotificationAdaptor::NotificationRemoved);
    }
}

uint DesktopNotificationAdaptor::Notify(const QString &app,
                                        const QString &title,
                                        const QString &body,
                                        const QString &icon,
                                        const QStringList &actions,
                                        const QString &priority)
{
    return m_manager ? m_manager->NotifyModern(app, title, body, icon, actions, priority) : 0u;
}

bool DesktopNotificationAdaptor::Dismiss(uint notificationId)
{
    return m_manager ? m_manager->Dismiss(notificationId) : false;
}

QVariantList DesktopNotificationAdaptor::GetAll() const
{
    return m_manager ? m_manager->GetAll() : QVariantList{};
}

void DesktopNotificationAdaptor::ClearAll()
{
    if (m_manager) {
        m_manager->ClearAll();
    }
}

bool DesktopNotificationAdaptor::ToggleCenter()
{
    return m_manager ? m_manager->ToggleCenter() : false;
}

