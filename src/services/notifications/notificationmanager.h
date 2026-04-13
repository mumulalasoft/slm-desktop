#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include <QVector>

struct NotificationEntry {
    uint id = 0;
    QString appId;
    QString appName;
    QString appIcon;
    QString summary;
    QString body;
    QStringList actions;
    QString priority = QStringLiteral("normal");
    QString groupId;
    bool read = false;
    bool banner = true;
    int urgency = 1;
    int expireTimeoutMs = -1;
    QDateTime timestamp;
};

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
    int unreadCountForApp(const QString &appName) const;
    int countForAppId(const QString &appId, uint excludeId = 0) const;

private:
    QVector<NotificationEntry> m_items;
};

class NotificationManager : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")
    Q_PROPERTY(bool serviceRegistered READ serviceRegistered NOTIFY serviceRegisteredChanged)
    Q_PROPERTY(bool desktopServiceRegistered READ desktopServiceRegistered NOTIFY desktopServiceRegisteredChanged)
    Q_PROPERTY(QAbstractListModel* notifications READ notifications CONSTANT)
    Q_PROPERTY(QAbstractListModel* bannerNotifications READ bannerNotifications CONSTANT)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int unreadCount READ unreadCount NOTIFY unreadCountChanged)
    Q_PROPERTY(bool doNotDisturb READ doNotDisturb WRITE setDoNotDisturb NOTIFY doNotDisturbChanged)
    Q_PROPERTY(QVariantMap latestNotification READ latestNotification NOTIFY latestNotificationChanged)
    Q_PROPERTY(int bubbleDurationMs READ bubbleDurationMs WRITE setBubbleDurationMs NOTIFY bubbleDurationMsChanged)
    Q_PROPERTY(bool centerVisible READ centerVisible NOTIFY centerVisibleChanged)

public:
    explicit NotificationManager(QObject *parent = nullptr);
    ~NotificationManager() override;

    bool serviceRegistered() const;
    bool desktopServiceRegistered() const;
    QAbstractListModel* notifications() const;
    QAbstractListModel* bannerNotifications() const;
    int count() const;
    int unreadCount() const;
    bool doNotDisturb() const;
    void setDoNotDisturb(bool enabled);
    QVariantMap latestNotification() const;
    int bubbleDurationMs() const;
    void setBubbleDurationMs(int durationMs);
    bool centerVisible() const;

    Q_INVOKABLE void clearAll();
    Q_INVOKABLE bool closeById(uint id);
    Q_INVOKABLE void toggleCenter();
    Q_INVOKABLE QVariantList getAll() const;
    Q_INVOKABLE uint notifySimple(const QString &appId,
                                  const QString &title,
                                  const QString &body,
                                  const QString &icon,
                                  const QStringList &actions,
                                  const QString &priority = QStringLiteral("normal"));
    Q_INVOKABLE bool dismiss(uint id);
    Q_INVOKABLE bool dismissBanner(uint id);
    Q_INVOKABLE void markAllRead();
    Q_INVOKABLE bool markRead(uint id, bool read = true);
    Q_INVOKABLE void invokeAction(uint id, const QString &actionKey = QStringLiteral("default"));
    Q_INVOKABLE int unreadCountForApp(const QString &appName) const;

public slots:
    QStringList GetCapabilities() const;
    void GetServerInformation(QString &name, QString &vendor, QString &version, QString &specVersion) const;
    uint Notify(const QString &appName,
                uint replacesId,
                const QString &appIcon,
                const QString &summary,
                const QString &body,
                const QStringList &actions,
                const QVariantMap &hints,
                int expireTimeout);
    void CloseNotification(uint id);
    uint NotifyModern(const QString &appId,
                      const QString &title,
                      const QString &body,
                      const QString &icon,
                      const QStringList &actions,
                      const QString &priority);
    bool Dismiss(uint id);
    QVariantList GetAll() const;
    void ClearAll();
    bool ToggleCenter();

signals:
    void serviceRegisteredChanged();
    void desktopServiceRegisteredChanged();
    void countChanged();
    void unreadCountChanged();
    void doNotDisturbChanged();
    void latestNotificationChanged();
    void bubbleDurationMsChanged();
    void centerVisibleChanged();
    void NotificationClosed(uint id, uint reason);
    void ActionInvoked(uint id, const QString &actionKey);
    void NotificationAdded(uint id);
    void NotificationRemoved(uint id);

private:
    void registerDbusService();
    int urgencyFromHints(const QVariantMap &hints) const;
    void emitCountIfChanged(int previousCount);
    void emitUnreadCountIfChanged(int previousUnreadCount);
    QString normalizePriority(const QString &priority) const;
    uint upsertNotification(const NotificationEntry &entry, bool suppressBanner = false);
    QVariantMap toVariantMap(const NotificationEntry &entry) const;
    void updateLatestNotification(const NotificationEntry &entry);
    bool shouldSuppressBannerForSpam(const NotificationEntry &entry);

    // History persistence: save/load notification history to disk so the
    // notification center survives daemon restarts.
    void saveHistory();
    void loadHistory();

    static constexpr int kMaxHistoryEntries = 200;

    bool m_serviceRegistered = false;
    bool m_desktopServiceRegistered = false;
    uint m_nextId = 1;
    bool m_doNotDisturb = false;
    bool m_centerVisible = false;
    QVariantMap m_latestNotification;
    int m_bubbleDurationMs = 5000;
    int m_maxActiveBannersPerApp = 3;
    int m_bannerRateLimitWindowMs = 10000;
    int m_maxBannerAttemptsPerWindow = 6;
    QHash<QString, QVector<qint64>> m_bannerAttemptHistoryByApp;
    NotificationListModel *m_model = nullptr;
    NotificationListModel *m_bannerModel = nullptr;
    QString m_historyFilePath;
    QTimer m_saveTimer;
};
