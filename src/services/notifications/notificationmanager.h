#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

#include "notificationrepository.h"
#include "notificationtypes.h"

class NotificationPolicyEngine;
class NotificationRepository;
class NotificationLifecycleEngine;

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
    Q_INVOKABLE int unreadCountForAppId(const QString &appId) const;
    Q_INVOKABLE int unreadCountForGroup(const QString &groupId) const;
    Q_INVOKABLE QString groupDisplayName(const QString &groupId) const;
    Q_INVOKABLE void setGroupExpanded(const QString &groupId, bool expanded);
    Q_INVOKABLE void setRuntimeContext(bool fullscreen, bool screenShare, bool focusMode);

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
    void reloadPolicySnapshot();
    bool shouldDropNotification(const NotificationEntry &entry) const;
    bool shouldShowBanner(const NotificationEntry &entry) const;

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
    NotificationRuntimeContext m_runtimeContext;
    bool m_notificationsEnabled = true;
    bool m_policyAllowCriticalAlerts = true;
    bool m_policySilenceFullscreen = true;
    bool m_policySilenceScreenShare = true;
    bool m_policyFocusModeIntegration = true;
    bool m_policyDeliverQuietly = false;
    NotificationRepository *m_repository = nullptr;
    NotificationLifecycleEngine *m_lifecycle = nullptr;
    QString m_historyFilePath;
    QTimer m_saveTimer;
};
