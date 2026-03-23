#include "settingsapp.h"
#include "moduleloader.h"
#include "searchengine.h"
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

#include <algorithm>

namespace {
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
    const QString bundledModules = appDir + "/../src/apps/settings/modules";
    const QString cwdModules = QDir::currentPath() + "/src/apps/settings/modules";
    paths << userModules << systemModules << appData + "/modules" << bundledModules << cwdModules;
    m_moduleLoader->scanModules(paths);

    if (!m_moduleLoader->modules().isEmpty()) {
        const QString first = m_moduleLoader->modules().first().toMap().value("id").toString();
        if (!first.isEmpty()) {
            openModule(first);
        }
    }

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
    QElapsedTimer timer;
    timer.start();
    if (deepLink.isEmpty()) {
        return false;
    }

    const QUrl url(deepLink);
    if (url.scheme() != "settings") {
        openModule(deepLink);
        return true;
    }

    const QString path = url.path().trimmed();
    QString moduleId = url.host().trimmed();
    QString settingToken;
    if (!path.isEmpty() && path != "/") {
        const QString clean = path.startsWith('/') ? path.mid(1) : path;
        const QStringList parts = clean.split('/', Qt::SkipEmptyParts);
        if (moduleId.isEmpty() && !parts.isEmpty()) {
            moduleId = parts.value(0);
            settingToken = parts.value(1);
        } else if (!parts.isEmpty()) {
            settingToken = parts.value(0);
        }
    }
    if (settingToken.isEmpty()) {
        settingToken = url.fragment().trimmed();
    }

    if (moduleId.isEmpty()) {
        return false;
    }
    openModuleSetting(moduleId, settingToken);
    m_lastDeepLinkLatencyMs = timer.elapsed();
    emit telemetryChanged();
    return true;
}

QString SettingsApp::resolveSettingId(const QString &moduleId, const QString &settingToken) const
{
    const QString raw = settingToken.trimmed();
    if (moduleId.isEmpty() || raw.isEmpty()) {
        return raw;
    }

    QString token = raw;
    const int slash = token.indexOf(QLatin1Char('/'));
    if (slash >= 0) {
        token = token.mid(slash + 1).trimmed();
    }
    if (token.isEmpty()) {
        token = raw;
    }

    const QVariantMap mod = m_moduleLoader->moduleById(moduleId);
    const QVariantList settings = mod.value(QStringLiteral("settings")).toList();
    if (settings.isEmpty()) {
        return token;
    }

    for (const QVariant &settingVar : settings) {
        const QVariantMap setting = settingVar.toMap();
        const QString id = setting.value(QStringLiteral("id")).toString().trimmed();
        if (id.isEmpty()) {
            continue;
        }
        const QString anchor = setting.value(QStringLiteral("anchor")).toString().trimmed();
        const QString deepLink = setting.value(QStringLiteral("deepLink")).toString().trimmed();
        if (token.compare(id, Qt::CaseInsensitive) == 0
            || (!anchor.isEmpty() && token.compare(anchor, Qt::CaseInsensitive) == 0)
            || (!deepLink.isEmpty()
                && (raw.compare(deepLink, Qt::CaseInsensitive) == 0
                    || token.compare(deepLink, Qt::CaseInsensitive) == 0
                    || deepLink.endsWith(QLatin1Char('/') + token, Qt::CaseInsensitive)))) {
            return id;
        }
    }

    return token;
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
    m_recentHistory.removeFirst();
    const QVariantMap prev = m_recentHistory.first().toMap();
    setCurrentModuleId(prev.value("moduleId").toString());
    setCurrentSettingId(prev.value("settingId").toString());
    emit recentHistoryChanged();
    emit deepLinkOpened(m_currentModuleId, m_currentSettingId);
}

void SettingsApp::raiseWindow()
{
    const auto objects = m_engine->rootObjects();
    for (QObject *obj : objects) {
        QQuickWindow *window = qobject_cast<QQuickWindow*>(obj);
        if (window) {
            window->show();
            window->raise();
            window->requestActivate();
            return;
        }
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
    return out;
}

QString SettingsApp::requestSettingAuthorization(const QString &moduleId, const QString &settingId)
{
    return requestSettingAuthorizationWithDecision(moduleId, settingId, QStringLiteral("allow-once"));
}

QString SettingsApp::requestSettingAuthorizationWithDecision(const QString &moduleId,
                                                             const QString &settingId,
                                                             const QString &decision)
{
    const QString key = settingGrantKey(moduleId, settingId);
    if (!key.isEmpty() && m_denyAlwaysGrants.contains(key)) {
        const QString requestId = QStringLiteral("auth-denied-%1").arg(QDateTime::currentMSecsSinceEpoch());
        emit settingAuthorizationFinished(requestId, moduleId, settingId, false, QStringLiteral("deny-always"));
        return requestId;
    }

    if (!key.isEmpty() && m_allowAlwaysGrants.contains(key)) {
        const QString requestId = QStringLiteral("auth-granted-%1").arg(QDateTime::currentMSecsSinceEpoch());
        emit settingAuthorizationFinished(requestId, moduleId, settingId, true, QStringLiteral("allow-always-cached"));
        return requestId;
    }

    const QString normalizedDecision = decision.trimmed().toLower();
    if (normalizedDecision == QStringLiteral("deny")
        || normalizedDecision == QStringLiteral("deny-always")) {
        if (!key.isEmpty() && normalizedDecision == QStringLiteral("deny-always")) {
            m_denyAlwaysGrants.insert(key);
            m_allowAlwaysGrants.remove(key);
            touchGrantTimestamp(key);
            saveGrantStore();
        }
        const QString requestId = QStringLiteral("auth-denied-%1").arg(QDateTime::currentMSecsSinceEpoch());
        emit settingAuthorizationFinished(requestId, moduleId, settingId, false, normalizedDecision);
        return requestId;
    }

    const QVariantMap policy = settingPolicy(moduleId, settingId);
    const QString actionId = policy.value(QStringLiteral("privilegedAction")).toString();
    if (normalizedDecision == QStringLiteral("allow-always")) {
        // Must be inserted before requestAuthorization() because bridge can emit
        // authorizationFinished synchronously in dev-override mode.
        m_pendingDecisionByRequestId.insert(QStringLiteral("pending:%1/%2").arg(moduleId, settingId),
                                            normalizedDecision);
    }
    const QString requestId = m_polkitBridge->requestAuthorization(actionId, moduleId, settingId);
    if (normalizedDecision == QStringLiteral("allow-always")) {
        const QString provisional = QStringLiteral("pending:%1/%2").arg(moduleId, settingId);
        if (m_pendingDecisionByRequestId.contains(provisional)) {
            m_pendingDecisionByRequestId.remove(provisional);
            m_pendingDecisionByRequestId.insert(requestId, normalizedDecision);
        } else if (!m_pendingDecisionByRequestId.contains(requestId)) {
            m_pendingDecisionByRequestId.insert(requestId, normalizedDecision);
        }
    }
    return requestId;
}

QVariantMap SettingsApp::settingGrantState(const QString &moduleId, const QString &settingId) const
{
    QVariantMap out;
    const QString key = settingGrantKey(moduleId, settingId);
    out.insert(QStringLiteral("allowAlways"), !key.isEmpty() && m_allowAlwaysGrants.contains(key));
    out.insert(QStringLiteral("denyAlways"), !key.isEmpty() && m_denyAlwaysGrants.contains(key));
    return out;
}

QVariantList SettingsApp::listSettingGrants() const
{
    QVariantList out;
    auto appendEntries = [this, &out](const QSet<QString> &src, const QString &decision) {
        for (const QString &entry : src) {
            const QStringList parts = entry.split(QLatin1Char('/'));
            if (parts.size() < 2) {
                continue;
            }
            QVariantMap row;
            row.insert(QStringLiteral("key"), entry);
            row.insert(QStringLiteral("moduleId"), parts.at(0));
            row.insert(QStringLiteral("settingId"), parts.mid(1).join(QStringLiteral("/")));
            row.insert(QStringLiteral("decision"), decision);
            const qint64 updatedAt = m_grantUpdatedAt.value(entry, 0);
            row.insert(QStringLiteral("updatedAt"), updatedAt);
            row.insert(QStringLiteral("updatedAtIso"),
                       updatedAt > 0
                           ? QDateTime::fromMSecsSinceEpoch(updatedAt).toUTC().toString(Qt::ISODateWithMs)
                           : QString());
            out.push_back(row);
        }
    };
    appendEntries(m_allowAlwaysGrants, QStringLiteral("allow-always"));
    appendEntries(m_denyAlwaysGrants, QStringLiteral("deny-always"));
    std::sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(QStringLiteral("updatedAt")).toLongLong()
            > b.toMap().value(QStringLiteral("updatedAt")).toLongLong();
    });
    return out;
}

bool SettingsApp::clearSettingGrant(const QString &moduleId, const QString &settingId)
{
    const QString key = settingGrantKey(moduleId, settingId);
    if (key.isEmpty()) {
        return false;
    }
    const bool removedAllow = m_allowAlwaysGrants.remove(key) > 0;
    const bool removedDeny = m_denyAlwaysGrants.remove(key) > 0;
    if (removedAllow || removedDeny) {
        m_grantUpdatedAt.remove(key);
        saveGrantStore();
        return true;
    }
    return false;
}

void SettingsApp::clearAllSettingGrants()
{
    if (m_allowAlwaysGrants.isEmpty() && m_denyAlwaysGrants.isEmpty()) {
        return;
    }
    m_allowAlwaysGrants.clear();
    m_denyAlwaysGrants.clear();
    m_grantUpdatedAt.clear();
    saveGrantStore();
}

QString SettingsApp::settingGrantKey(const QString &moduleId, const QString &settingId) const
{
    const QString mod = moduleId.trimmed().toLower();
    const QString setting = settingId.trimmed().toLower();
    if (mod.isEmpty() || setting.isEmpty()) {
        return {};
    }
    return mod + QLatin1Char('/') + setting;
}

void SettingsApp::loadGrantStore()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    QSettings settings(dir + QStringLiteral("/settings-permissions.ini"), QSettings::IniFormat);

    const QStringList allow = settings.value(QStringLiteral("grants/allowAlways")).toStringList();
    for (const QString &entry : allow) {
        const QString k = entry.trimmed().toLower();
        if (!k.isEmpty()) {
            m_allowAlwaysGrants.insert(k);
        }
    }

    const QStringList deny = settings.value(QStringLiteral("grants/denyAlways")).toStringList();
    for (const QString &entry : deny) {
        const QString k = entry.trimmed().toLower();
        if (!k.isEmpty()) {
            m_denyAlwaysGrants.insert(k);
        }
    }

    const QVariantMap updatedAt = settings.value(QStringLiteral("grants/updatedAt")).toMap();
    for (auto it = updatedAt.constBegin(); it != updatedAt.constEnd(); ++it) {
        const QString k = it.key().trimmed().toLower();
        if (k.isEmpty()) {
            continue;
        }
        m_grantUpdatedAt.insert(k, it.value().toLongLong());
    }
}

void SettingsApp::saveGrantStore()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    QSettings settings(dir + QStringLiteral("/settings-permissions.ini"), QSettings::IniFormat);
    settings.setValue(QStringLiteral("grants/allowAlways"), QStringList(m_allowAlwaysGrants.values()));
    settings.setValue(QStringLiteral("grants/denyAlways"), QStringList(m_denyAlwaysGrants.values()));
    QVariantMap updatedAt;
    for (auto it = m_grantUpdatedAt.constBegin(); it != m_grantUpdatedAt.constEnd(); ++it) {
        updatedAt.insert(it.key(), it.value());
    }
    settings.setValue(QStringLiteral("grants/updatedAt"), updatedAt);
    settings.sync();
    emit grantsChanged();
}

void SettingsApp::touchGrantTimestamp(const QString &grantKey)
{
    if (grantKey.trimmed().isEmpty()) {
        return;
    }
    m_grantUpdatedAt.insert(grantKey.trimmed().toLower(),
                            QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());
}

void SettingsApp::appendRecent(const QString &moduleId, const QString &settingId)
{
    QVariantMap entry;
    entry.insert("moduleId", moduleId);
    entry.insert("settingId", settingId);
    entry.insert("timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));

    for (int i = 0; i < m_recentHistory.size(); ++i) {
        const QVariantMap old = m_recentHistory.at(i).toMap();
        if (old.value("moduleId").toString() == moduleId &&
            old.value("settingId").toString() == settingId) {
            m_recentHistory.removeAt(i);
            break;
        }
    }
    m_recentHistory.prepend(entry);
    while (m_recentHistory.size() > 15) {
        m_recentHistory.removeLast();
    }
    emit recentHistoryChanged();
}

void SettingsApp::updateBreadcrumb()
{
    const QVariantMap mod = m_moduleLoader->moduleById(m_currentModuleId);
    const QString moduleName = mod.value("name").toString();
    QString settingName;
    const QVariantList settings = mod.value("settings").toList();
    for (const QVariant &settingVar : settings) {
        const QVariantMap setting = settingVar.toMap();
        if (setting.value("id").toString() == m_currentSettingId) {
            settingName = setting.value("label").toString();
            break;
        }
    }
    const QString next = settingName.isEmpty()
                             ? moduleName
                             : QStringLiteral("%1 > %2").arg(moduleName, settingName);
    if (next != m_breadcrumb) {
        m_breadcrumb = next;
        emit breadcrumbChanged();
    }
}
