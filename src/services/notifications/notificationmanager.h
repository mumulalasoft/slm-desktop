#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QObject>
#include <QVariantMap>
#include <QVector>

struct NotificationEntry {
    uint id = 0;
    QString appName;
    QString appIcon;
    QString summary;
    QString body;
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
        AppIconRole,
        SummaryRole,
        BodyRole,
        UrgencyRole,
        TimestampRole
    };

    explicit NotificationListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void upsert(const NotificationEntry &entry);
    bool removeById(uint id);
    void clear();
    int count() const;

private:
    QVector<NotificationEntry> m_items;
};

class NotificationManager : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")
    Q_PROPERTY(bool serviceRegistered READ serviceRegistered NOTIFY serviceRegisteredChanged)
    Q_PROPERTY(QAbstractListModel* notifications READ notifications CONSTANT)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool doNotDisturb READ doNotDisturb WRITE setDoNotDisturb NOTIFY doNotDisturbChanged)
    Q_PROPERTY(QVariantMap latestNotification READ latestNotification NOTIFY latestNotificationChanged)
    Q_PROPERTY(int bubbleDurationMs READ bubbleDurationMs WRITE setBubbleDurationMs NOTIFY bubbleDurationMsChanged)

public:
    explicit NotificationManager(QObject *parent = nullptr);
    ~NotificationManager() override;

    bool serviceRegistered() const;
    QAbstractListModel* notifications() const;
    int count() const;
    bool doNotDisturb() const;
    void setDoNotDisturb(bool enabled);
    QVariantMap latestNotification() const;
    int bubbleDurationMs() const;
    void setBubbleDurationMs(int durationMs);

    Q_INVOKABLE void clearAll();
    Q_INVOKABLE bool closeById(uint id);

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

signals:
    void serviceRegisteredChanged();
    void countChanged();
    void doNotDisturbChanged();
    void latestNotificationChanged();
    void bubbleDurationMsChanged();
    void NotificationClosed(uint id, uint reason);
    void ActionInvoked(uint id, const QString &actionKey);

private:
    void registerDbusService();
    int urgencyFromHints(const QVariantMap &hints) const;
    void emitCountIfChanged(int previousCount);

    bool m_serviceRegistered = false;
    uint m_nextId = 1;
    bool m_doNotDisturb = false;
    QVariantMap m_latestNotification;
    int m_bubbleDurationMs = 5000;
    NotificationListModel *m_model = nullptr;
};
