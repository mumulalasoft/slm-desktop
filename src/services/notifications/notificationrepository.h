#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QVariantMap>
#include <QVector>

#include "notificationtypes.h"

class NotificationListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        AppNameRole,
        AppIdRole,
        AppIconRole,
        SummaryRole,
        BodyRole,
        ActionsRole,
        PriorityRole,
        GroupIdRole,
        ReadRole,
        BannerRole,
        LifecycleStateRole,
        UrgencyRole,
        TimestampRole
    };

    explicit NotificationListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void upsert(const NotificationEntry &entry);
    bool removeById(uint id);
    bool markReadById(uint id, bool read = true);
    int markAllRead(bool read = true);
    void clear();
    int count() const;
    int unreadCount() const;
    int unreadCountForAppId(const QString &appId) const;
    int unreadCountForGroup(const QString &groupId) const;
    QString displayNameForGroup(const QString &groupId) const;
    int countForAppId(const QString &appId, uint excludeId = 0) const;
    bool setLifecycleById(uint id, const QString &state);

private:
    QVector<NotificationEntry> m_items;
};

class NotificationRepository : public QObject {
    Q_OBJECT
public:
    explicit NotificationRepository(QObject *parent = nullptr);

    NotificationListModel* notificationsModel() const;
    NotificationListModel* bannerModel() const;

    int count() const;
    int unreadCount() const;

    uint nextId();
    uint currentNextId() const;
    void ensureNextIdAtLeast(uint candidate);

    QVariantMap latestNotification() const;
    void updateLatestNotification(const NotificationEntry &entry);

    QVector<NotificationEntry> entries() const;

private:
    uint m_nextId = 1;
    QVariantMap m_latestNotification;
    NotificationListModel *m_notificationsModel = nullptr;
    NotificationListModel *m_bannerModel = nullptr;
};
