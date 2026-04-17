#pragma once

#include <QObject>
#include <QQmlApplicationEngine>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QVariantList>
#include <functional>

class ModuleLoader;
class SearchEngine;
class SettingBinding;
class SettingBindingFactory;
class SettingsPolkitBridge;

class SettingsApp : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ModuleLoader* moduleLoader READ moduleLoader CONSTANT)
    Q_PROPERTY(SearchEngine* searchEngine READ searchEngine CONSTANT)
    Q_PROPERTY(QString currentModuleId READ currentModuleId WRITE setCurrentModuleId NOTIFY currentModuleIdChanged)
    Q_PROPERTY(QString currentSettingId READ currentSettingId WRITE setCurrentSettingId NOTIFY currentSettingIdChanged)
    Q_PROPERTY(QVariantList recentHistory READ recentHistory NOTIFY recentHistoryChanged)
    Q_PROPERTY(QString breadcrumb READ breadcrumb NOTIFY breadcrumbChanged)
    Q_PROPERTY(bool commandPaletteVisible READ commandPaletteVisible WRITE setCommandPaletteVisible NOTIFY commandPaletteVisibleChanged)
    Q_PROPERTY(qint64 lastModuleOpenLatencyMs READ lastModuleOpenLatencyMs NOTIFY telemetryChanged)
    Q_PROPERTY(qint64 lastDeepLinkLatencyMs READ lastDeepLinkLatencyMs NOTIFY telemetryChanged)

public:
    using PortalInvoker = std::function<QVariantMap(const QString &method, const QVariantList &args)>;

    explicit SettingsApp(QQmlApplicationEngine *engine, QObject *parent = nullptr);

    ModuleLoader* moduleLoader() const { return m_moduleLoader; }
    SearchEngine* searchEngine() const { return m_searchEngine; }
    
    QString currentModuleId() const { return m_currentModuleId; }
    void setCurrentModuleId(const QString &id);
    QString currentSettingId() const { return m_currentSettingId; }
    void setCurrentSettingId(const QString &id);
    QVariantList recentHistory() const { return m_recentHistory; }
    QString breadcrumb() const { return m_breadcrumb; }
    bool commandPaletteVisible() const { return m_commandPaletteVisible; }
    void setCommandPaletteVisible(bool value);
    qint64 lastModuleOpenLatencyMs() const { return m_lastModuleOpenLatencyMs; }
    qint64 lastDeepLinkLatencyMs() const { return m_lastDeepLinkLatencyMs; }

    Q_INVOKABLE void openModule(const QString &id);
    Q_INVOKABLE void openModuleSetting(const QString &moduleId, const QString &settingId);
    Q_INVOKABLE bool openDeepLink(const QString &deepLink);
    Q_INVOKABLE void clearRecentHistory();
    Q_INVOKABLE void back();
    Q_INVOKABLE void raiseWindow();

    // Factory methods for bindings
    Q_INVOKABLE SettingBinding* createGSettingsBinding(const QString &schema, const QString &key);
    Q_INVOKABLE SettingBinding* createMockBinding(const QVariant &initial);
    Q_INVOKABLE SettingBinding* createNetworkManagerBinding();
    Q_INVOKABLE SettingBinding* createBinding(const QString &backendBinding,
                                              const QVariant &defaultValue = QVariant());
    Q_INVOKABLE SettingBinding* createBindingFor(const QString &moduleId,
                                                 const QString &settingId,
                                                 const QVariant &defaultValue = QVariant());
    Q_INVOKABLE QVariantMap settingPolicy(const QString &moduleId, const QString &settingId) const;
    Q_INVOKABLE QString requestSettingAuthorization(const QString &moduleId, const QString &settingId);
    Q_INVOKABLE QString requestSettingAuthorizationWithDecision(const QString &moduleId,
                                                                const QString &settingId,
                                                                const QString &decision);
    Q_INVOKABLE QVariantMap settingGrantState(const QString &moduleId, const QString &settingId) const;
    Q_INVOKABLE QVariantList listSettingGrants() const;
    Q_INVOKABLE bool clearSettingGrant(const QString &moduleId, const QString &settingId);
    Q_INVOKABLE void clearAllSettingGrants();
    Q_INVOKABLE QVariantList listSecretApps() const;
    Q_INVOKABLE QVariantMap clearSecretDataForApp(const QString &appId) const;
    Q_INVOKABLE QVariantList listSecretConsentSummary() const;
    Q_INVOKABLE QVariantMap revokeSecretConsentForApp(const QString &appId) const;
    void setPortalInvokerForTests(PortalInvoker invoker);

signals:
    void currentModuleIdChanged();
    void currentSettingIdChanged();
    void recentHistoryChanged();
    void breadcrumbChanged();
    void commandPaletteVisibleChanged();
    void moduleOpened(const QString &id);
    void deepLinkOpened(const QString &moduleId, const QString &settingId);
    void settingAuthorizationFinished(const QString &requestId,
                                      const QString &moduleId,
                                      const QString &settingId,
                                      bool allowed,
                                      const QString &reason);
    void grantsChanged();
    void telemetryChanged();

private:
    QString settingGrantKey(const QString &moduleId, const QString &settingId) const;
    void loadGrantStore();
    void saveGrantStore();
    void touchGrantTimestamp(const QString &grantKey);
    QString resolveSettingId(const QString &moduleId, const QString &settingToken) const;
    void appendRecent(const QString &moduleId, const QString &settingId);
    void updateBreadcrumb();
    QVariantMap callPortal(const QString &method, const QVariantList &args = {}) const;

    QQmlApplicationEngine *m_engine;
    ModuleLoader *m_moduleLoader;
    SearchEngine *m_searchEngine;
    SettingBindingFactory *m_bindingFactory;
    SettingsPolkitBridge *m_polkitBridge;
    QHash<QString, QString> m_pendingDecisionByRequestId;
    QSet<QString> m_allowAlwaysGrants;
    QSet<QString> m_denyAlwaysGrants;
    QHash<QString, qint64> m_grantUpdatedAt;
    QHash<QString, QPointer<SettingBinding>> m_bindingCache;
    QString m_currentModuleId;
    QString m_currentSettingId;
    QVariantList m_recentHistory;
    QString m_breadcrumb;
    bool m_commandPaletteVisible = false;
    qint64 m_lastModuleOpenLatencyMs = 0;
    qint64 m_lastDeepLinkLatencyMs = 0;
    PortalInvoker m_portalInvoker;
};
