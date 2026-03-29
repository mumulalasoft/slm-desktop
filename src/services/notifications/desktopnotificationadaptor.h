#pragma once

#include <QDBusAbstractAdaptor>
#include <QVariantList>

class NotificationManager;

class DesktopNotificationAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.example.Desktop.Notification")

public:
    explicit DesktopNotificationAdaptor(NotificationManager *manager);

public slots:
    uint Notify(const QString &app,
                const QString &title,
                const QString &body,
                const QString &icon,
                const QStringList &actions,
                const QString &priority);
    bool Dismiss(uint notificationId);
    QVariantList GetAll() const;
    void ClearAll();
    bool ToggleCenter();

signals:
    void NotificationAdded(uint id);
    void NotificationRemoved(uint id);

private:
    NotificationManager *m_manager = nullptr;
};

