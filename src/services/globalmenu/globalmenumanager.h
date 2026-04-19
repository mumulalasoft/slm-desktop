#pragma once

#include <QObject>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

class QDBusServiceWatcher;
class ExternalMenuSignalProxy;

class GlobalMenuManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(QVariantList topLevelMenus READ topLevelMenus NOTIFY changed)
    Q_PROPERTY(QString activeMenuService READ activeMenuService NOTIFY changed)
    Q_PROPERTY(QString activeAppId READ activeAppId NOTIFY appSwitched)
    Q_PROPERTY(bool appSwitching READ appSwitching NOTIFY appSwitchingChanged)

public:
    explicit GlobalMenuManager(QObject *parent = nullptr);

    bool available() const;
    QVariantList topLevelMenus() const;
    QString activeMenuService() const;
    QString activeAppId() const;
    bool appSwitching() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void resetAfterResume();
    Q_INVOKABLE bool activateMenu(int menuId);
    Q_INVOKABLE QVariantList menuItemsFor(int menuId) const;
    Q_INVOKABLE bool activateMenuItem(int menuId, int itemId);
    Q_INVOKABLE QVariantList queryDbusMenuTopLevel(const QString &service, const QString &path) const;
    Q_INVOKABLE QVariantList queryDbusMenuItems(const QString &service, const QString &path, int menuId) const;
    Q_INVOKABLE bool activateDbusMenuItem(const QString &service, const QString &path, int itemId);
    Q_INVOKABLE QVariantMap diagnosticSnapshot() const;
    Q_INVOKABLE void setOverrideMenus(const QVariantList &menus,
                                      const QString &context = QStringLiteral("override"));
    Q_INVOKABLE void clearOverrideMenus();
    void notifyExternalMenuChanged(const QString &service, const QString &path);

signals:
    void changed();
    void appSwitched(QString appId);
    void appSwitchingChanged();
    void overrideMenuActivated(int menuId, QString label, QString context);
    void overrideMenuItemActivated(int menuId, int itemId, QString label, QString context);
    void dbusMenuChanged(const QString &service, const QString &path);

private:
    struct RegisteredMenu {
        QString service;
        QString path;
        qint64 seenMs = 0;
    };

    void updatePollingInterval();
    quint32 detectActiveWindowId() const;
    bool resolveMenuForWindow(quint32 windowId, QString *service, QString *path) const;
    bool resolveMenuForWindowViaRegistrar(const QString &registrarService,
                                          const QString &registrarPath,
                                          const QString &registrarIface,
                                          const QString &method,
                                          quint32 windowId,
                                          QString *service,
                                          QString *path) const;
    void seedRegisteredMenus();
    void seedRegisteredMenusFromRegistrar(const QString &registrarService,
                                          const QString &registrarPath,
                                          const QString &registrarIface,
                                          const QString &method,
                                          QHash<quint32, RegisteredMenu> *out) const;
    bool selectMenuForActiveApp(QString *service, QString *path, int *score = nullptr) const;
    void bindActiveMenuSignals();
    void unbindActiveMenuSignals();
    qint64 connectionPid(const QString &service) const;
    QString normalizeAppToken(const QString &raw) const;
    QVariantList queryTopLevelMenus(const QString &service, const QString &path) const;
    QVariantList queryMenuItemsNative(const QString &service, const QString &path, int menuId) const;
    QString normalizeLabel(const QString &raw) const;
    static QString parseDbusIconDataUri(const QVariant &iconDataVariant);
    QString menuCacheKey(const QString &service, const QString &path) const;
    void invalidateMenuCache(const QString &service, const QString &path);
    void ensureExternalMenuSignalBinding(const QString &service, const QString &path) const;
private slots:
    void onSessionActiveAppChanged(const QString &appId);
    void onActiveMenuLayoutChanged();
    void flushExternalMenuChanges();

private:
    bool m_available = false;
    bool m_appSwitching = false;
    QTimer *m_appSwitchTimer = nullptr;
    QVariantList m_topLevelMenus;
    QString m_activeMenuService;
    QString m_activeMenuPath;
    quint32 m_activeWindowId = 0;
    QString m_lastActiveAppId;
    QTimer *m_timer = nullptr;
    bool m_overrideEnabled = false;
    QVariantList m_overrideMenus;
    QString m_overrideContext;
    QHash<quint32, RegisteredMenu> m_registeredMenus;
    mutable QHash<QString, qint64> m_connectionPidCache;
    QString m_lastSelectionSource;
    int m_lastSelectionScore = 0;
    bool m_debugEnabled = false;
    QString m_boundMenuService;
    QString m_boundMenuPath;
    QDBusServiceWatcher *m_canonicalRegistrarWatcher = nullptr;
    QDBusServiceWatcher *m_kdeRegistrarWatcher = nullptr;
    QDBusServiceWatcher *m_slmMenuWatcher = nullptr;
    QTimer *m_externalMenuDebounceTimer = nullptr;
    QSet<QString> m_pendingExternalMenuKeys;
    mutable QHash<QString, QHash<int, QVariantList>> m_menuItemsCache;
    mutable QHash<QString, QPointer<ExternalMenuSignalProxy>> m_externalMenuProxies;
};
