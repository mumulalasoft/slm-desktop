#include "firewallserviceclient.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDateTime>
#include <QJsonDocument>

namespace {
constexpr const char kService[] = "org.slm.Desktop.Firewall";
constexpr const char kPath[] = "/org/slm/Desktop/Firewall";
constexpr const char kInterface[] = "org.slm.Desktop.Firewall";
constexpr const char kSettingsService[] = "org.slm.Desktop.Settings";
constexpr const char kSettingsPath[] = "/org/slm/Desktop/Settings";
constexpr const char kSettingsInterface[] = "org.slm.Desktop.Settings";
constexpr const char kQuickBlockPolicyPath[] = "firewall.quickBlockUndo.policyId";
constexpr const char kQuickBlockTargetPath[] = "firewall.quickBlockUndo.target";

QString normalizeMode(const QString &value)
{
    const QString mode = value.trimmed().toLower();
    if (mode == QLatin1String("public") || mode == QLatin1String("custom")) {
        return mode;
    }
    return QStringLiteral("home");
}

QString normalizePolicy(const QString &value, const QString &fallback)
{
    const QString policy = value.trimmed().toLower();
    if (policy == QLatin1String("allow")
            || policy == QLatin1String("deny")
            || policy == QLatin1String("prompt")) {
        return policy;
    }
    return fallback;
}

QVariant valueByPath(const QVariantMap &root, const QString &path, bool *ok)
{
    if (ok) {
        *ok = false;
    }
    const QStringList segments = path.split('.', Qt::SkipEmptyParts);
    if (segments.isEmpty()) {
        return {};
    }

    QVariant current = root;
    for (int i = 0; i < segments.size(); ++i) {
        const QVariantMap map = current.toMap();
        if (!map.contains(segments.at(i))) {
            return {};
        }
        current = map.value(segments.at(i));
        if (i < segments.size() - 1 && !current.canConvert<QVariantMap>()) {
            return {};
        }
    }
    if (ok) {
        *ok = true;
    }
    return current;
}
}

FirewallServiceClient::FirewallServiceClient(QObject *parent)
    : QObject(parent)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(QStringLiteral("org.freedesktop.DBus"),
                QStringLiteral("/org/freedesktop/DBus"),
                QStringLiteral("org.freedesktop.DBus"),
                QStringLiteral("NameOwnerChanged"),
                this,
                SLOT(onNameOwnerChanged(QString,QString,QString)));

    ensureIface();
    refresh();
    restoreQuickBlockStateFromSettings();
}

bool FirewallServiceClient::available() const
{
    return m_available;
}

bool FirewallServiceClient::enabled() const
{
    return m_enabled;
}

QString FirewallServiceClient::mode() const
{
    return m_mode;
}

QString FirewallServiceClient::defaultIncomingPolicy() const
{
    return m_defaultIncomingPolicy;
}

QString FirewallServiceClient::defaultOutgoingPolicy() const
{
    return m_defaultOutgoingPolicy;
}

int FirewallServiceClient::promptCooldownSeconds() const
{
    return m_promptCooldownSeconds;
}

QVariantList FirewallServiceClient::appPolicies() const
{
    return m_appPolicies;
}

QVariantList FirewallServiceClient::ipPolicies() const
{
    return m_ipPolicies;
}

QVariantList FirewallServiceClient::activeConnections() const
{
    return m_activeConnections;
}

QVariantList FirewallServiceClient::pendingPrompts() const
{
    return m_pendingPrompts;
}

QString FirewallServiceClient::lastQuickBlockPolicyId() const
{
    return m_lastQuickBlockPolicyId;
}

QString FirewallServiceClient::lastQuickBlockTarget() const
{
    return m_lastQuickBlockTarget;
}

QString FirewallServiceClient::quickBlockUndoNotice() const
{
    return m_quickBlockUndoNotice;
}

void FirewallServiceClient::setLastQuickBlockPolicyId(const QString &policyId)
{
    const QString normalized = policyId.trimmed();
    if (m_lastQuickBlockPolicyId == normalized) {
        return;
    }
    m_lastQuickBlockPolicyId = normalized;
    if (!normalized.isEmpty() && !m_quickBlockUndoNotice.isEmpty()) {
        m_quickBlockUndoNotice.clear();
    }
    if (!m_restoringQuickBlockState) {
        setSettingsValue(QString::fromLatin1(kQuickBlockPolicyPath), normalized);
    }
    emit quickBlockStateChanged();
}

void FirewallServiceClient::setLastQuickBlockTarget(const QString &target)
{
    const QString normalized = target.trimmed();
    if (m_lastQuickBlockTarget == normalized) {
        return;
    }
    m_lastQuickBlockTarget = normalized;
    if (!m_restoringQuickBlockState) {
        setSettingsValue(QString::fromLatin1(kQuickBlockTargetPath), normalized);
    }
    emit quickBlockStateChanged();
}

bool FirewallServiceClient::setSettingsValue(const QString &path, const QVariant &value) const
{
    QDBusInterface settingsIface(QString::fromLatin1(kSettingsService),
                                 QString::fromLatin1(kSettingsPath),
                                 QString::fromLatin1(kSettingsInterface),
                                 QDBusConnection::sessionBus());
    if (!settingsIface.isValid()) {
        return false;
    }
    QDBusReply<QVariantMap> reply = settingsIface.call(QStringLiteral("SetSetting"),
                                                       path,
                                                       QVariant::fromValue(QDBusVariant(value)));
    return reply.isValid() && reply.value().value(QStringLiteral("ok"), false).toBool();
}

void FirewallServiceClient::restoreQuickBlockStateFromSettings()
{
    QDBusInterface settingsIface(QString::fromLatin1(kSettingsService),
                                 QString::fromLatin1(kSettingsPath),
                                 QString::fromLatin1(kSettingsInterface),
                                 QDBusConnection::sessionBus());
    if (!settingsIface.isValid()) {
        return;
    }

    QDBusReply<QVariantMap> reply = settingsIface.call(QStringLiteral("GetSettings"));
    if (!reply.isValid()) {
        return;
    }
    const QVariantMap payload = reply.value();
    if (!payload.value(QStringLiteral("ok"), false).toBool()) {
        return;
    }

    const QVariantMap settings = payload.value(QStringLiteral("settings")).toMap();
    bool okPolicy = false;
    bool okTarget = false;
    const QString policyId = valueByPath(settings, QString::fromLatin1(kQuickBlockPolicyPath), &okPolicy)
                                 .toString().trimmed();
    const QString target = valueByPath(settings, QString::fromLatin1(kQuickBlockTargetPath), &okTarget)
                               .toString().trimmed();
    if (!okPolicy && !okTarget) {
        return;
    }

    m_restoringQuickBlockState = true;
    if (okPolicy) {
        setLastQuickBlockPolicyId(policyId);
    }
    if (okTarget) {
        setLastQuickBlockTarget(target);
    }
    m_restoringQuickBlockState = false;
    syncQuickBlockTokenWithIpPolicies();
}

void FirewallServiceClient::syncQuickBlockTokenWithIpPolicies()
{
    const QString policyId = m_lastQuickBlockPolicyId.trimmed();
    if (policyId.isEmpty()) {
        return;
    }

    bool exists = false;
    for (const QVariant &entry : m_ipPolicies) {
        const QVariantMap row = entry.toMap();
        if (row.value(QStringLiteral("policyId")).toString() == policyId) {
            exists = true;
            break;
        }
    }
    if (exists) {
        return;
    }

    m_quickBlockUndoNotice = QStringLiteral("Last quick block is no longer available (expired or removed).");
    setLastQuickBlockPolicyId(QString());
    setLastQuickBlockTarget(QString());
    emit quickBlockStateChanged();
}

bool FirewallServiceClient::refresh()
{
    if (!ensureIface()) {
        return false;
    }
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("GetStatus"));
    if (!reply.isValid()) {
        return false;
    }
    const QVariantMap payload = reply.value();
    if (!payload.value(QStringLiteral("ok"), false).toBool()) {
        return false;
    }
    const bool stateOk = applyStateMap(payload);
    refreshAppPolicies();
    refreshIpPolicies();
    refreshConnections();
    return stateOk;
}

bool FirewallServiceClient::setEnabled(bool enabled)
{
    return callBoolMapMethod(QStringLiteral("SetEnabled"), enabled);
}

bool FirewallServiceClient::setMode(const QString &mode)
{
    return callBoolMapMethod(QStringLiteral("SetMode"), normalizeMode(mode));
}

bool FirewallServiceClient::setDefaultIncomingPolicy(const QString &policy)
{
    return callBoolMapMethod(QStringLiteral("SetDefaultIncomingPolicy"),
                             normalizePolicy(policy, m_defaultIncomingPolicy));
}

bool FirewallServiceClient::setDefaultOutgoingPolicy(const QString &policy)
{
    return callBoolMapMethod(QStringLiteral("SetDefaultOutgoingPolicy"),
                             normalizePolicy(policy, m_defaultOutgoingPolicy));
}

bool FirewallServiceClient::setPromptCooldownSeconds(int seconds)
{
    return callBoolMapMethod(QStringLiteral("SetPromptCooldownSeconds"), seconds);
}

QVariantMap FirewallServiceClient::evaluateConnection(const QVariantMap &request)
{
    if (!ensureIface()) {
        return QVariantMap{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("service-unavailable")},
        };
    }
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("EvaluateConnection"), request);
    if (!reply.isValid()) {
        return QVariantMap{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("dbus-call-failed")},
        };
    }
    return reply.value();
}

bool FirewallServiceClient::resolveConnectionDecision(const QVariantMap &request,
                                                      const QString &decision,
                                                      bool remember)
{
    if (!ensureIface()) {
        return false;
    }
    const QString normalized = normalizePolicy(decision, QStringLiteral("prompt"));
    if (normalized == QLatin1String("prompt")) {
        return false;
    }
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("ResolveConnectionDecision"),
                                                  request,
                                                  normalized,
                                                  remember);
    if (!reply.isValid()) {
        return false;
    }
    const QVariantMap payload = reply.value();
    const bool ok = payload.value(QStringLiteral("ok"), false).toBool();
    if (ok && remember) {
        refreshAppPolicies();
    }
    return ok;
}

bool FirewallServiceClient::setIpPolicy(const QVariantMap &policy)
{
    const QVariantMap payload = setIpPolicyDetailed(policy);
    return payload.value(QStringLiteral("ok"), false).toBool();
}

QVariantMap FirewallServiceClient::setIpPolicyDetailed(const QVariantMap &policy)
{
    if (!ensureIface()) {
        return QVariantMap{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("service-unavailable")},
        };
    }
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("SetIpPolicy"), policy);
    if (!reply.isValid()) {
        return QVariantMap{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("dbus-call-failed")},
        };
    }
    const QVariantMap payload = reply.value();
    const bool ok = payload.value(QStringLiteral("ok"), false).toBool();
    if (ok) {
        refreshIpPolicies();
    }
    return payload;
}

bool FirewallServiceClient::setAppPolicy(const QVariantMap &policy)
{
    if (!ensureIface()) {
        return false;
    }
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("SetAppPolicy"), policy);
    if (!reply.isValid()) {
        return false;
    }
    const QVariantMap payload = reply.value();
    const bool ok = payload.value(QStringLiteral("ok"), false).toBool();
    if (ok) {
        refreshAppPolicies();
    }
    return ok;
}

bool FirewallServiceClient::refreshAppPolicies()
{
    if (!ensureIface()) {
        return false;
    }
    QDBusReply<QVariantList> reply = m_iface->call(QStringLiteral("ListAppPolicies"));
    if (!reply.isValid()) {
        return false;
    }
    const QVariantList next = reply.value();
    if (m_appPolicies != next) {
        m_appPolicies = next;
        emit appPoliciesChanged();
    }
    return true;
}

bool FirewallServiceClient::clearAppPolicies()
{
    if (!ensureIface()) {
        return false;
    }
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("ClearAppPolicies"));
    if (!reply.isValid()) {
        return false;
    }
    const QVariantMap payload = reply.value();
    const bool ok = payload.value(QStringLiteral("ok"), false).toBool();
    if (ok && !m_appPolicies.isEmpty()) {
        m_appPolicies.clear();
        emit appPoliciesChanged();
    }
    return ok;
}

bool FirewallServiceClient::removeAppPolicy(const QString &policyId)
{
    if (!ensureIface()) {
        return false;
    }
    const QString id = policyId.trimmed();
    if (id.isEmpty()) {
        return false;
    }
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("RemoveAppPolicy"), id);
    if (!reply.isValid()) {
        return false;
    }
    const QVariantMap payload = reply.value();
    const bool ok = payload.value(QStringLiteral("ok"), false).toBool();
    if (ok) {
        refreshAppPolicies();
    }
    return ok;
}

bool FirewallServiceClient::refreshIpPolicies()
{
    if (!ensureIface()) {
        return false;
    }
    QDBusReply<QVariantList> reply = m_iface->call(QStringLiteral("ListIpPolicies"));
    if (!reply.isValid()) {
        return false;
    }
    const QVariantList next = reply.value();
    if (m_ipPolicies != next) {
        m_ipPolicies = next;
        emit ipPoliciesChanged();
    }
    syncQuickBlockTokenWithIpPolicies();
    return true;
}

bool FirewallServiceClient::clearIpPolicies()
{
    if (!ensureIface()) {
        return false;
    }
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("ClearIpPolicies"));
    if (!reply.isValid()) {
        return false;
    }
    const QVariantMap payload = reply.value();
    const bool ok = payload.value(QStringLiteral("ok"), false).toBool();
    if (ok) {
        if (!m_ipPolicies.isEmpty()) {
            m_ipPolicies.clear();
            emit ipPoliciesChanged();
        }
    }
    return ok;
}

bool FirewallServiceClient::removeIpPolicy(const QString &policyId)
{
    if (!ensureIface()) {
        return false;
    }
    const QString id = policyId.trimmed();
    if (id.isEmpty()) {
        return false;
    }
    QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("RemoveIpPolicy"), id);
    if (!reply.isValid()) {
        return false;
    }
    const QVariantMap payload = reply.value();
    const bool ok = payload.value(QStringLiteral("ok"), false).toBool();
    if (ok) {
        refreshIpPolicies();
    }
    return ok;
}

bool FirewallServiceClient::refreshConnections()
{
    if (!ensureIface()) {
        return false;
    }
    QDBusReply<QVariantList> reply = m_iface->call(QStringLiteral("ListConnections"));
    if (!reply.isValid()) {
        return false;
    }
    const QVariantList next = reply.value();
    if (m_activeConnections != next) {
        m_activeConnections = next;
        emit activeConnectionsChanged();
    }
    return true;
}

bool FirewallServiceClient::resolvePendingPrompt(int index, const QString &decision, bool remember)
{
    if (index < 0 || index >= m_pendingPrompts.size()) {
        return false;
    }
    const QVariantMap row = m_pendingPrompts.at(index).toMap();
    const QVariantMap request = row.value(QStringLiteral("request")).toMap();
    if (request.isEmpty()) {
        return false;
    }
    const bool ok = resolveConnectionDecision(request, decision, remember);
    if (!ok) {
        return false;
    }

    m_pendingPrompts.removeAt(index);
    emit pendingPromptsChanged();
    return true;
}

int FirewallServiceClient::resolveAllPendingPrompts(const QString &decision, bool remember)
{
    if (m_pendingPrompts.isEmpty()) {
        return 0;
    }

    int resolved = 0;
    for (int i = m_pendingPrompts.size() - 1; i >= 0; --i) {
        const QVariantMap row = m_pendingPrompts.at(i).toMap();
        const QVariantMap request = row.value(QStringLiteral("request")).toMap();
        if (request.isEmpty()) {
            continue;
        }
        if (!resolveConnectionDecision(request, decision, remember)) {
            continue;
        }
        m_pendingPrompts.removeAt(i);
        ++resolved;
    }

    if (resolved > 0) {
        emit pendingPromptsChanged();
    }
    return resolved;
}

void FirewallServiceClient::clearPendingPrompts()
{
    if (m_pendingPrompts.isEmpty()) {
        return;
    }
    m_pendingPrompts.clear();
    emit pendingPromptsChanged();
}

void FirewallServiceClient::onNameOwnerChanged(const QString &name,
                                               const QString &oldOwner,
                                               const QString &newOwner)
{
    if (name != QLatin1String(kService)) {
        return;
    }
    Q_UNUSED(oldOwner)
    if (!newOwner.isEmpty()) {
        ensureIface();
        refresh();
        return;
    }

    if (m_available) {
        m_available = false;
        emit availableChanged();
    }
    if (!m_appPolicies.isEmpty()) {
        m_appPolicies.clear();
        emit appPoliciesChanged();
    }
    if (!m_ipPolicies.isEmpty()) {
        m_ipPolicies.clear();
        emit ipPoliciesChanged();
    }
    if (!m_activeConnections.isEmpty()) {
        m_activeConnections.clear();
        emit activeConnectionsChanged();
    }
    if (!m_pendingPrompts.isEmpty()) {
        m_pendingPrompts.clear();
        emit pendingPromptsChanged();
    }
    delete m_iface;
    m_iface = nullptr;
}

void FirewallServiceClient::onFirewallStateChanged(const QVariantMap &state)
{
    applyStateMap(state);
    refreshAppPolicies();
    refreshIpPolicies();
    refreshConnections();
}

void FirewallServiceClient::onConnectionPromptRequested(const QVariantMap &prompt)
{
    const QVariantMap request = prompt.value(QStringLiteral("request")).toMap();
    const QVariantMap evaluation = prompt.value(QStringLiteral("evaluation")).toMap();
    if (request.isEmpty() || evaluation.isEmpty()) {
        return;
    }

    const QString requestKey = QString::fromUtf8(
        QJsonDocument::fromVariant(request).toJson(QJsonDocument::Compact));
    const QString evaluationKey = QString::fromUtf8(
        QJsonDocument::fromVariant(evaluation).toJson(QJsonDocument::Compact));
    for (const QVariant &item : m_pendingPrompts) {
        const QVariantMap existing = item.toMap();
        const QString existingRequestKey = QString::fromUtf8(
            QJsonDocument::fromVariant(existing.value(QStringLiteral("request")).toMap())
                .toJson(QJsonDocument::Compact));
        const QString existingEvaluationKey = QString::fromUtf8(
            QJsonDocument::fromVariant(existing.value(QStringLiteral("evaluation")).toMap())
                .toJson(QJsonDocument::Compact));
        if (existingRequestKey == requestKey && existingEvaluationKey == evaluationKey) {
            return;
        }
    }

    QVariantMap row = prompt;
    row.insert(QStringLiteral("receivedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    m_pendingPrompts.append(row);
    while (m_pendingPrompts.size() > 20) {
        m_pendingPrompts.removeFirst();
    }
    emit pendingPromptsChanged();
}

bool FirewallServiceClient::ensureIface()
{
    if (m_iface && m_iface->isValid()) {
        return true;
    }

    delete m_iface;
    m_iface = new QDBusInterface(QLatin1String(kService),
                                 QLatin1String(kPath),
                                 QLatin1String(kInterface),
                                 QDBusConnection::sessionBus(),
                                 this);
    const bool nowAvailable = m_iface->isValid();
    if (nowAvailable != m_available) {
        m_available = nowAvailable;
        emit availableChanged();
    }
    if (nowAvailable) {
        QDBusConnection::sessionBus().connect(QLatin1String(kService),
                                              QLatin1String(kPath),
                                              QLatin1String(kInterface),
                                              QStringLiteral("FirewallStateChanged"),
                                              this,
                                              SLOT(onFirewallStateChanged(QVariantMap)));
        QDBusConnection::sessionBus().connect(QLatin1String(kService),
                                              QLatin1String(kPath),
                                              QLatin1String(kInterface),
                                              QStringLiteral("ConnectionPromptRequested"),
                                              this,
                                              SLOT(onConnectionPromptRequested(QVariantMap)));
    }
    return nowAvailable;
}

bool FirewallServiceClient::callBoolMapMethod(const QString &method, const QVariant &arg)
{
    if (!ensureIface()) {
        return false;
    }

    QDBusReply<QVariantMap> reply = arg.isValid()
            ? m_iface->call(method, arg)
            : m_iface->call(method);
    if (!reply.isValid()) {
        return false;
    }

    const QVariantMap payload = reply.value();
    const bool ok = payload.value(QStringLiteral("ok"), false).toBool();
    if (ok) {
        applyStateMap(payload);
    }
    return ok;
}

bool FirewallServiceClient::applyStateMap(const QVariantMap &map)
{
    const bool nextEnabled = map.value(QStringLiteral("enabled"), m_enabled).toBool();
    const QString nextMode = normalizeMode(map.value(QStringLiteral("mode"), m_mode).toString());
    const QString nextIncoming = normalizePolicy(
        map.value(QStringLiteral("defaultIncomingPolicy"), m_defaultIncomingPolicy).toString(),
        m_defaultIncomingPolicy);
    const QString nextOutgoing = normalizePolicy(
        map.value(QStringLiteral("defaultOutgoingPolicy"), m_defaultOutgoingPolicy).toString(),
        m_defaultOutgoingPolicy);
    int nextCooldown = map.value(QStringLiteral("promptCooldownSeconds"), m_promptCooldownSeconds).toInt();
    if (nextCooldown < 1) {
        nextCooldown = 1;
    } else if (nextCooldown > 300) {
        nextCooldown = 300;
    }

    const bool changed = (m_enabled != nextEnabled)
            || (m_mode != nextMode)
            || (m_defaultIncomingPolicy != nextIncoming)
            || (m_defaultOutgoingPolicy != nextOutgoing)
            || (m_promptCooldownSeconds != nextCooldown);

    m_enabled = nextEnabled;
    m_mode = nextMode;
    m_defaultIncomingPolicy = nextIncoming;
    m_defaultOutgoingPolicy = nextOutgoing;
    m_promptCooldownSeconds = nextCooldown;

    if (changed) {
        emit stateChanged();
    }
    return true;
}
