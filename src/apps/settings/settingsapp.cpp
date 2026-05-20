#include "settingsapp.h"
#include "moduleloader.h"
#include "searchengine.h"
#include "include/settingbinding.h"
#include "include/settingbindingfactory.h"
#include "include/settingspolkitbridge.h"
#include "include/gsettingsbinding.h"
#include "include/mockbinding.h"
#include "include/networkmanagerbinding.h"

#include <QQmlContext>
#include <QDir>
#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QStandardPaths>
#include <QSettings>
#include <QQuickWindow>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>

#include <algorithm>
#include <utility>

namespace {
constexpr const char kPortalService[] = "org.slm.Desktop.Portal";
constexpr const char kPortalPath[] = "/org/slm/Desktop/Portal";
constexpr const char kPortalIface[] = "org.slm.Desktop.Portal";

QVariantMap portalCallMap(const QString &method, const QVariantList &args = {})
{
    QDBusInterface iface(QString::fromLatin1(kPortalService),
                         QString::fromLatin1(kPortalPath),
                         QString::fromLatin1(kPortalIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("portal-unavailable")},
            {QStringLiteral("message"), QStringLiteral("Portal service is unavailable.")},
        };
    }

    QDBusReply<QVariantMap> reply = iface.callWithArgumentList(QDBus::Block, method, args);
    if (!reply.isValid()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("portal-call-failed")},
            {QStringLiteral("message"), reply.error().message()},
        };
    }
    return reply.value();
}

QString defaultPrivilegedActionFor(const QString &moduleId, const QString &settingId)
{
    const QString mod = moduleId.trimmed().toLower();
    const QString setting = settingId.trimmed().toLower();
    if (mod == QStringLiteral("network")) {
        if (setting == QStringLiteral("wifi") || setting == QStringLiteral("ethernet")) {
            return QStringLiteral("org.slm.settings.network.modify");
        }
    }
    if (mod == QStringLiteral("power")) {
        return QStringLiteral("org.slm.settings.power.modify");
    }
    if (mod == QStringLiteral("applications")) {
        return QStringLiteral("org.slm.settings.applications.modify");
    }
    return {};
}
}

SettingsApp::SettingsApp(QQmlApplicationEngine *engine, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_moduleLoader(new ModuleLoader(this))
    , m_searchEngine(new SearchEngine(m_moduleLoader, this))
    , m_bindingFactory(new SettingBindingFactory(this))
    , m_polkitBridge(new SettingsPolkitBridge(this))
{
    // Register types for QML
    m_engine->rootContext()->setContextProperty("SettingsApp", this);
    m_engine->rootContext()->setContextProperty("ModuleLoader", m_moduleLoader);
    m_engine->rootContext()->setContextProperty("SearchEngine", m_searchEngine);
    loadGrantStore();

    // Initial scan: user + system plugin folders + bundled modules.
    QStringList paths;
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    const QString userModules = QDir::homePath() + "/.local/lib/settings/modules";
    const QString systemModules = "/usr/lib/settings/modules";
    const QString localSystemModules = "/usr/local/lib/settings/modules";
    const QString bundledModules = appDir + "/../src/apps/settings/modules";
    const QString cwdModules = QDir::currentPath() + "/src/apps/settings/modules";
    const QString appLibModules = QDir(appDir).absoluteFilePath("../lib/settings/modules");
    // Dev-first lookup order:
    // 1) source tree / cwd (workspace module edits),
    // 2) bundled next to binary,
    // 3) user/appdata overrides,
    // 4) system modules.
#ifdef SLM_SOURCE_DIR
    const QString sourceModules = QStringLiteral(SLM_SOURCE_DIR) + "/src/apps/settings/modules";
    if (!paths.contains(sourceModules))
        paths << sourceModules;
#endif
    paths << cwdModules
          << bundledModules
          << userModules
          << appData + "/modules"
          << appLibModules
          << localSystemModules
          << systemModules;
    m_moduleLoader->scanModules(paths);

    connect(m_polkitBridge, &SettingsPolkitBridge::authorizationFinished,
            this, [this](const QString &requestId,
                         const QString &moduleId,
                         const QString &settingId,
                         bool allowed,
                         const QString &reason) {
                const QString decision = m_pendingDecisionByRequestId.take(requestId);
                QString effectiveDecision = decision;
                if (effectiveDecision.isEmpty()) {
                    const QString provisional = QStringLiteral("pending:%1/%2").arg(moduleId, settingId);
                    effectiveDecision = m_pendingDecisionByRequestId.take(provisional);
                }
                const QString key = settingGrantKey(moduleId, settingId);
                if (!key.isEmpty() && allowed && effectiveDecision == QStringLiteral("allow-always")) {
                    m_allowAlwaysGrants.insert(key);
                    m_denyAlwaysGrants.remove(key);
                    touchGrantTimestamp(key);
                    saveGrantStore();
                }
                emit settingAuthorizationFinished(requestId, moduleId, settingId, allowed, reason);
            });
}

void SettingsApp::setDesktopSettingsClient(DesktopSettingsClient *client)
{
    m_bindingFactory->setDesktopSettingsClient(client);
}

void SettingsApp::setCurrentModuleId(const QString &id)
{
    if (m_currentModuleId != id) {
        m_currentModuleId = id;
        emit currentModuleIdChanged();
        updateBreadcrumb();
    }
}

void SettingsApp::setCurrentSettingId(const QString &id)
{
    if (m_currentSettingId != id) {
        m_currentSettingId = id;
        emit currentSettingIdChanged();
        updateBreadcrumb();
    }
}

void SettingsApp::setCommandPaletteVisible(bool value)
{
    if (m_commandPaletteVisible == value) {
        return;
    }
    m_commandPaletteVisible = value;
    emit commandPaletteVisibleChanged();
}

void SettingsApp::openModule(const QString &id)
{
    QElapsedTimer timer;
    timer.start();
    if (id.isEmpty()) {
        return;
    }
    setCurrentModuleId(id);
    setCurrentSettingId({});
    appendRecent(id, {});
    emit moduleOpened(id);
    emit deepLinkOpened(id, {});
    raiseWindow();
    m_lastModuleOpenLatencyMs = timer.elapsed();
    emit telemetryChanged();
}

void SettingsApp::openModuleSetting(const QString &moduleId, const QString &settingId)
{
    QElapsedTimer timer;
    timer.start();
    if (moduleId.isEmpty()) {
        return;
    }
    const QString resolvedSettingId = resolveSettingId(moduleId, settingId);
    setCurrentModuleId(moduleId);
    setCurrentSettingId(resolvedSettingId);
    appendRecent(moduleId, resolvedSettingId);
    emit moduleOpened(moduleId);
    emit deepLinkOpened(moduleId, resolvedSettingId);
    raiseWindow();
    m_lastModuleOpenLatencyMs = timer.elapsed();
    emit telemetryChanged();
}

bool SettingsApp::openDeepLink(const QString &deepLink)
{
    if (deepLink.isEmpty()) {
        return false;
    }
    const QUrl url(deepLink);
    if (url.scheme() != QStringLiteral("settings")) {
        return false;
    }
    const QString moduleId = url.host();
    const QString settingId = url.path().mid(1); // remove leading /
    if (moduleId.isEmpty()) {
        return false;
    }
    openModuleSetting(moduleId, settingId);
    return true;
}

void SettingsApp::clearRecentHistory()
{
    if (m_recentHistory.isEmpty()) {
        return;
    }
    m_recentHistory.clear();
    emit recentHistoryChanged();
}

void SettingsApp::back()
{
    if (m_recentHistory.size() < 2) {
        return;
    }
    // current is at 0, prev at 1
    m_recentHistory.removeFirst();
    const QVariantMap prev = m_recentHistory.first().toMap();
    const QString modId = prev.value(QStringLiteral("moduleId")).toString();
    const QString setId = prev.value(QStringLiteral("settingId")).toString();
    setCurrentModuleId(modId);
    setCurrentSettingId(setId);
    emit moduleOpened(modId);
    emit deepLinkOpened(modId, setId);
    emit recentHistoryChanged();
}

void SettingsApp::raiseWindow()
{
    const QList<QQuickWindow*> windows = m_engine->findChildren<QQuickWindow*>();
    for (QQuickWindow *window : windows) {
        window->raise();
        window->requestActivate();
    }
}

SettingBinding* SettingsApp::createGSettingsBinding(const QString &schema, const QString &key)
{
    return new GSettingsBinding(schema, key, this);
}

SettingBinding* SettingsApp::createMockBinding(const QVariant &initial)
{
    return new MockBinding(initial, this);
}

SettingBinding* SettingsApp::createNetworkManagerBinding()
{
    return new NetworkManagerBinding(this);
}

SettingBinding *SettingsApp::createBinding(const QString &backendBinding,
                                           const QVariant &defaultValue)
{
    const QString spec = backendBinding.trimmed();
    if (spec.isEmpty()) {
        return createMockBinding(defaultValue);
    }
    if (m_bindingCache.contains(spec) && m_bindingCache.value(spec)) {
        return m_bindingCache.value(spec);
    }
    SettingBinding *binding = m_bindingFactory->create(spec, defaultValue, this);
    m_bindingCache.insert(spec, binding);
    return binding;
}

SettingBinding *SettingsApp::createBindingFor(const QString &moduleId,
                                              const QString &settingId,
                                              const QVariant &defaultValue)
{
    const QVariantMap mod = m_moduleLoader->moduleById(moduleId);
    if (mod.isEmpty()) {
        return createMockBinding(defaultValue);
    }
    const QVariantList settings = mod.value(QStringLiteral("settings")).toList();
    for (const QVariant &settingVar : settings) {
        const QVariantMap setting = settingVar.toMap();
        if (setting.value(QStringLiteral("id")).toString().trimmed() != settingId.trimmed()) {
            continue;
        }
        const QString spec = setting.value(QStringLiteral("backendBinding")).toString().trimmed();
        return createBinding(spec, defaultValue);
    }
    return createMockBinding(defaultValue);
}

QVariantMap SettingsApp::settingPolicy(const QString &moduleId, const QString &settingId) const
{
    QVariantMap out;
    out.insert(QStringLiteral("moduleId"), moduleId);
    out.insert(QStringLiteral("settingId"), settingId);

    QString privilegedAction;
    const QVariantMap mod = m_moduleLoader->moduleById(moduleId);
    if (!mod.isEmpty()) {
        const QVariantList settings = mod.value(QStringLiteral("settings")).toList();
        for (const QVariant &settingVar : settings) {
            const QVariantMap setting = settingVar.toMap();
            if (setting.value(QStringLiteral("id")).toString().trimmed() == settingId.trimmed()) {
                privilegedAction = setting.value(QStringLiteral("privilegedAction")).toString().trimmed();
                break;
            }
        }
    }
    QString source = QStringLiteral("none");
    if (!privilegedAction.isEmpty()) {
        source = QStringLiteral("module-metadata");
    } else {
        privilegedAction = defaultPrivilegedActionFor(moduleId, settingId);
        if (!privilegedAction.isEmpty()) {
            source = QStringLiteral("default-policy");
        }
    }

    out.insert(QStringLiteral("privilegedAction"), privilegedAction);
    out.insert(QStringLiteral("requiresAuthorization"), !privilegedAction.isEmpty());
    out.insert(QStringLiteral("policySource"), source);

    const QString key = settingGrantKey(moduleId, settingId);
    if (!key.isEmpty()) {
        if (m_allowAlwaysGrants.contains(key)) {
            out.insert(QStringLiteral("effectiveGrant"), QStringLiteral("allow-always"));
        } else if (m_denyAlwaysGrants.contains(key)) {
            out.insert(QStringLiteral("effectiveGrant"), QStringLiteral("deny-always"));
        } else {
            out.insert(QStringLiteral("effectiveGrant"), QStringLiteral("none"));
        }
    }

    return out;
}

QString SettingsApp::requestSettingAuthorization(const QString &moduleId, const QString &settingId)
{
    return requestSettingAuthorizationWithDecision(moduleId, settingId, QStringLiteral("none"));
}

QString SettingsApp::requestSettingAuthorizationWithDecision(const QString &moduleId,
                                                            const QString &settingId,
                                                            const QString &decision)
{
    const QVariantMap policy = settingPolicy(moduleId, settingId);
    const QString action = policy.value(QStringLiteral("privilegedAction")).toString();
    if (action.isEmpty()) {
        return {};
    }

    const QString requestId = m_polkitBridge->requestAuthorization(action, moduleId, settingId);
    if (!decision.isEmpty() && decision != QStringLiteral("none")) {
        m_pendingDecisionByRequestId.insert(requestId, decision);
    }
    return requestId;
}

QVariantMap SettingsApp::settingGrantState(const QString &moduleId, const QString &settingId) const
{
    const QString key = settingGrantKey(moduleId, settingId);
    QVariantMap out;
    if (key.isEmpty()) return out;

    if (m_allowAlwaysGrants.contains(key)) {
        out.insert(QStringLiteral("grant"), QStringLiteral("allow-always"));
        out.insert(QStringLiteral("timestamp"), m_grantUpdatedAt.value(key));
    } else if (m_denyAlwaysGrants.contains(key)) {
        out.insert(QStringLiteral("grant"), QStringLiteral("deny-always"));
        out.insert(QStringLiteral("timestamp"), m_grantUpdatedAt.value(key));
    } else {
        out.insert(QStringLiteral("grant"), QStringLiteral("none"));
    }
    return out;
}

QVariantList SettingsApp::listSettingGrants() const
{
    QVariantList out;
    QSet<QString> keys = m_allowAlwaysGrants;
    keys.unite(m_denyAlwaysGrants);

    for (const QString &key : keys) {
        const QStringList parts = key.split('/', Qt::KeepEmptyParts);
        if (parts.size() < 2) continue;
        QVariantMap row;
        row.insert(QStringLiteral("moduleId"), parts.at(0));
        row.insert(QStringLiteral("settingId"), parts.at(1));
        row.insert(QStringLiteral("grant"), m_allowAlwaysGrants.contains(key) ? "allow-always" : "deny-always");
        row.insert(QStringLiteral("timestamp"), m_grantUpdatedAt.value(key));
        out.append(row);
    }
    return out;
}

bool SettingsApp::clearSettingGrant(const QString &moduleId, const QString &settingId)
{
    const QString key = settingGrantKey(moduleId, settingId);
    if (key.isEmpty()) return false;

    bool removed = m_allowAlwaysGrants.remove(key) || m_denyAlwaysGrants.remove(key);
    if (removed) {
        m_grantUpdatedAt.remove(key);
        saveGrantStore();
        emit grantsChanged();
    }
    return removed;
}

void SettingsApp::clearAllSettingGrants()
{
    if (m_allowAlwaysGrants.isEmpty() && m_denyAlwaysGrants.isEmpty()) return;
    m_allowAlwaysGrants.clear();
    m_denyAlwaysGrants.clear();
    m_grantUpdatedAt.clear();
    saveGrantStore();
    emit grantsChanged();
}

QVariantList SettingsApp::listSecretApps() const
{
    const QVariantMap result = portalCallMap(QStringLiteral("ListSecretApps"));
    return result.value(QStringLiteral("apps")).toList();
}

QVariantMap SettingsApp::clearSecretDataForApp(const QString &appId) const
{
    return portalCallMap(QStringLiteral("ClearSecretDataForApp"), {appId});
}

QVariantList SettingsApp::listSecretConsentSummary() const
{
    const QVariantMap result = portalCallMap(QStringLiteral("ListSecretConsentSummary"));
    return result.value(QStringLiteral("consents")).toList();
}

QVariantMap SettingsApp::revokeSecretConsentForApp(const QString &appId) const
{
    return portalCallMap(QStringLiteral("RevokeSecretConsentForApp"), {appId});
}

void SettingsApp::setPortalInvokerForTests(PortalInvoker invoker)
{
    Q_UNUSED(invoker);
}

void SettingsApp::updateBreadcrumb()
{
    emit breadcrumbChanged();
}

void SettingsApp::appendRecent(const QString &moduleId, const QString &settingId)
{
    QVariantMap entry;
    entry.insert(QStringLiteral("moduleId"), moduleId);
    entry.insert(QStringLiteral("settingId"), settingId);
    entry.insert(QStringLiteral("timestamp"), QDateTime::currentDateTime().toMSecsSinceEpoch());

    // Remove if already exists (move to top)
    for (int i = 0; i < m_recentHistory.size(); ++i) {
        const QVariantMap row = m_recentHistory.at(i).toMap();
        if (row.value(QStringLiteral("moduleId")).toString() == moduleId
            && row.value(QStringLiteral("settingId")).toString() == settingId) {
            m_recentHistory.removeAt(i);
            break;
        }
    }

    m_recentHistory.prepend(entry);
    if (m_recentHistory.size() > 20) {
        m_recentHistory.removeLast();
    }
    emit recentHistoryChanged();
}

QString SettingsApp::resolveSettingId(const QString &moduleId, const QString &settingId) const
{
    if (!settingId.isEmpty()) {
        return settingId;
    }
    // Pick first available setting if none specified
    const QVariantMap mod = m_moduleLoader->moduleById(moduleId);
    if (mod.isEmpty()) return {};
    const QVariantList settings = mod.value(QStringLiteral("settings")).toList();
    if (settings.isEmpty()) return {};
    return settings.first().toMap().value(QStringLiteral("id")).toString();
}

QString SettingsApp::settingGrantKey(const QString &moduleId, const QString &settingId) const
{
    if (moduleId.trimmed().isEmpty() || settingId.trimmed().isEmpty()) {
        return {};
    }
    return QStringLiteral("%1/%2").arg(moduleId.trimmed(), settingId.trimmed());
}

void SettingsApp::loadGrantStore()
{
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/grants.json";
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    const QVariantMap root = doc.toVariant().toMap();
    const QVariantList allows = root.value(QStringLiteral("allow-always")).toList();
    const QVariantList denies = root.value(QStringLiteral("deny-always")).toList();
    const QVariantMap times = root.value(QStringLiteral("timestamps")).toMap();

    for (const QVariant &v : allows) m_allowAlwaysGrants.insert(v.toString());
    for (const QVariant &v : denies) m_denyAlwaysGrants.insert(v.toString());
    for (auto it = times.begin(); it != times.end(); ++it) {
        m_grantUpdatedAt.insert(it.key(), it.value().toLongLong());
    }
}

void SettingsApp::saveGrantStore()
{
    const QString dirPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dirPath);
    const QString path = dirPath + "/grants.json";
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return;

    QVariantMap root;
    QVariantList allows;
    for (const QString &s : m_allowAlwaysGrants) allows.append(s);
    QVariantList denies;
    for (const QString &s : m_denyAlwaysGrants) denies.append(s);

    QVariantMap times;
    for (auto it = m_grantUpdatedAt.begin(); it != m_grantUpdatedAt.end(); ++it) {
        times.insert(it.key(), it.value());
    }

    root.insert(QStringLiteral("allow-always"), allows);
    root.insert(QStringLiteral("deny-always"), denies);
    root.insert(QStringLiteral("timestamps"), times);

    file.write(QJsonDocument::fromVariant(root).toJson());
}

void SettingsApp::touchGrantTimestamp(const QString &key)
{
    m_grantUpdatedAt.insert(key, QDateTime::currentDateTime().toMSecsSinceEpoch());
}
