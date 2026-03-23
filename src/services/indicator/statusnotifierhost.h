#pragma once

#include <QDBusContext>
#include <QDBusObjectPath>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class QDBusServiceWatcher;

class StatusNotifierHost : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.StatusNotifierWatcher")
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
    Q_PROPERTY(QStringList RegisteredStatusNotifierItems READ registeredStatusNotifierItems NOTIFY registeredStatusNotifierItemsChanged)
    Q_PROPERTY(bool IsStatusNotifierHostRegistered READ isStatusNotifierHostRegistered CONSTANT)
    Q_PROPERTY(int ProtocolVersion READ protocolVersion CONSTANT)

public:
    explicit StatusNotifierHost(QObject *parent = nullptr);
    ~StatusNotifierHost() override;

    QVariantList entries() const;
    QStringList registeredStatusNotifierItems() const;
    bool isStatusNotifierHostRegistered() const;
    int protocolVersion() const;

    Q_INVOKABLE void activate(const QString &itemId, int x, int y);
    Q_INVOKABLE void secondaryActivate(const QString &itemId, int x, int y);
    Q_INVOKABLE void contextMenu(const QString &itemId, int x, int y);
    Q_INVOKABLE void scroll(const QString &itemId, int delta, const QString &orientation);
    Q_INVOKABLE QVariantMap metricsSnapshot() const;
    Q_INVOKABLE void resetMetrics();

public slots:
    void RegisterStatusNotifierItem(const QString &serviceOrPath);
    void RegisterStatusNotifierHost(const QString &service);

signals:
    void entriesChanged();
    void registeredStatusNotifierItemsChanged();
    void StatusNotifierItemRegistered(const QString &service);
    void StatusNotifierItemUnregistered(const QString &service);
    void StatusNotifierHostRegistered();

private slots:
    void onWatchedServiceUnregistered(const QString &serviceName);
    void onItemSignalPing();

private:
    struct ItemEntry {
        QString id;
        QString service;
        QString path;
        QString title;
        QString iconName;
        QString iconSource;
        QString iconThemePath;
        QString status;
        QString menuPath;
    };

    static QString normalizePath(const QString &path);
    static QString makeItemId(const QString &service, const QString &path);
    static QString dbusObjectPathToString(const QVariant &value);
    static QString iconPixmapToDataUri(const QVariant &value);
    static QString toolTipToText(const QVariant &value);
    static QString resolveIconFromThemePath(const QString &iconName, const QString &iconThemePath);

    void registerWatcherServices();
    void addOrUpdateItem(const QString &service, const QString &path);
    void removeItem(const QString &itemId);
    void refreshItem(const QString &itemId);
    void subscribeItemSignals(const QString &itemId);
    void emitEntriesChanged();
    bool callItemMethod(const QString &itemId, const QString &method, int x, int y);

    QHash<QString, ItemEntry> m_items;
    QSet<QString> m_subscribedItems;
    QStringList m_registeredItems;
    QDBusServiceWatcher *m_serviceWatcher = nullptr;
    QStringList m_registeredWatcherServices;
    bool m_debugEnabled = false;
    bool m_metricsEnabled = false;
    QVariantMap m_metrics;
};
