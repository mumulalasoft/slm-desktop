#include "firewallserviceclient.h"

#include <QDBusConnection>
#include <QDBusArgument>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDateTime>
#include <QJsonDocument>
#include <QMetaType>

#include <algorithm>
#include <limits>

namespace {
constexpr const char kService[] = "org.slm.Desktop.Firewall";
constexpr const char kPath[] = "/org/slm/Desktop/Firewall";
constexpr const char kInterface[] = "org.slm.Desktop.Firewall";
constexpr const char kSettingsService[] = "org.slm.Desktop.Settings";
constexpr const char kSettingsPath[] = "/org/slm/Desktop/Settings";
constexpr const char kSettingsInterface[] = "org.slm.Desktop.Settings";
constexpr const char kQuickBlockPolicyPath[] = "firewall.quickBlockUndo.policyId";
constexpr const char kQuickBlockTargetPath[] = "firewall.quickBlockUndo.target";
constexpr const char kConfirmBatchTriagePresetPath[] = "firewall.pendingTriage.confirmBatchPreset";
constexpr qint64 kPendingPromptTtlSecs = 15 * 60;
constexpr qint64 kPromptDuplicateSuppressSecs = 8;
constexpr qint64 kPromptDuplicateSuppressCacheSecs = kPromptDuplicateSuppressSecs * 6;

qint64 pendingPromptRemainingSeconds(const QVariantMap &row, const QDateTime &nowUtc)
{
    const QString receivedRaw = row.value(QStringLiteral("receivedAt")).toString().trimmed();
    if (receivedRaw.isEmpty()) {
        return std::numeric_limits<qint64>::max();
    }
    const QDateTime receivedAt = QDateTime::fromString(receivedRaw, Qt::ISODate);
    if (!receivedAt.isValid()) {
        return std::numeric_limits<qint64>::max();
    }
    qint64 ageSecs = receivedAt.secsTo(nowUtc);
    if (ageSecs < 0) {
        ageSecs = 0;
    }
    qint64 remaining = kPendingPromptTtlSecs - ageSecs;
    if (remaining < 0) {
        remaining = 0;
    }
    return remaining;
}

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

QVariant normalizeDbusVariant(const QVariant &input);

QVariantList normalizeDbusList(const QVariantList &list)
{
    QVariantList normalized;
    normalized.reserve(list.size());
    for (const QVariant &entry : list) {
        normalized.append(normalizeDbusVariant(entry));
    }
    return normalized;
}

QVariantMap normalizeDbusMap(const QVariantMap &map)
{
    QVariantMap normalized;
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        normalized.insert(it.key(), normalizeDbusVariant(it.value()));
    }
    return normalized;
}

QVariant normalizeDbusVariant(const QVariant &input)
{
    if (input.userType() == qMetaTypeId<QDBusVariant>()) {
        const QDBusVariant dbusVariant = qvariant_cast<QDBusVariant>(input);
        return normalizeDbusVariant(dbusVariant.variant());
    }

    if (input.userType() == qMetaTypeId<QDBusArgument>()) {
        const QDBusArgument arg = qvariant_cast<QDBusArgument>(input);
        const QVariantMap asMap = qdbus_cast<QVariantMap>(arg);
        if (!asMap.isEmpty()) {
            return normalizeDbusMap(asMap);
        }
        const QVariantList asList = qdbus_cast<QVariantList>(arg);
        if (!asList.isEmpty()) {
            return normalizeDbusList(asList);
        }
        return {};
    }

    if (input.userType() == QMetaType::QVariantMap) {
        const QVariantMap map = input.toMap();
        if (!map.isEmpty()) {
            return normalizeDbusMap(map);
        }
    }
    if (input.userType() == QMetaType::QVariantList) {
        const QVariantList list = input.toList();
        if (!list.isEmpty()) {
            return normalizeDbusList(list);
        }
    }
    return input;
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

int FirewallServiceClient::pendingPromptTtlSeconds() const
{
    return static_cast<int>(kPendingPromptTtlSecs);
}

bool FirewallServiceClient::confirmBatchTriagePreset() const
{
    return m_confirmBatchTriagePreset;
}

void FirewallServiceClient::setConfirmBatchTriagePreset(bool enabled)
{
    if (m_confirmBatchTriagePreset == enabled) {
        return;
    }
    m_confirmBatchTriagePreset = enabled;
    if (!m_restoringQuickBlockState) {
        setSettingsValue(QString::fromLatin1(kConfirmBatchTriagePresetPath), enabled);
    }
    emit triagePreferencesChanged();
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
    bool okConfirm = false;
    const QString policyId = valueByPath(settings, QString::fromLatin1(kQuickBlockPolicyPath), &okPolicy)
                                 .toString().trimmed();
    const QString target = valueByPath(settings, QString::fromLatin1(kQuickBlockTargetPath), &okTarget)
                               .toString().trimmed();
    const bool confirmBatchPreset = valueByPath(settings,
                                                QString::fromLatin1(kConfirmBatchTriagePresetPath),
                                                &okConfirm).toBool();
    if (!okPolicy && !okTarget && !okConfirm) {
        return;
    }

    m_restoringQuickBlockState = true;
    if (okPolicy) {
        setLastQuickBlockPolicyId(policyId);
    }
    if (okTarget) {
        setLastQuickBlockTarget(target);
    }
    if (okConfirm) {
        setConfirmBatchTriagePreset(confirmBatchPreset);
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

bool FirewallServiceClient::resolvePendingPrompt(int index,
                                                 const QString &decision,
                                                 bool remember,
                                                 bool onlyLocal)
{
    const bool hadPrunedRows = pruneStalePendingPrompts();
    if (hadPrunedRows) {
        emit pendingPromptsChanged();
    }
    if (index < 0 || index >= m_pendingPrompts.size()) {
        return false;
    }
    const QVariantMap row = m_pendingPrompts.at(index).toMap();
    QVariantMap request = row.value(QStringLiteral("request")).toMap();
    if (request.isEmpty()) {
        return false;
    }
    if (onlyLocal) {
        request.insert(QStringLiteral("localOnly"), true);
    }
    const bool ok = resolveConnectionDecision(request, decision, remember);
    if (!ok) {
        return false;
    }

    m_pendingPrompts.removeAt(index);
    emit pendingPromptsChanged();
    return true;
}

int FirewallServiceClient::resolveAllPendingPrompts(const QString &decision,
                                                    bool remember,
                                                    bool onlyLocal)
{
    const bool hadPrunedRows = pruneStalePendingPrompts();
    if (m_pendingPrompts.isEmpty()) {
        if (hadPrunedRows) {
            emit pendingPromptsChanged();
        }
        return 0;
    }

    int resolved = 0;
    for (int i = m_pendingPrompts.size() - 1; i >= 0; --i) {
        const QVariantMap row = m_pendingPrompts.at(i).toMap();
        QVariantMap request = row.value(QStringLiteral("request")).toMap();
        if (request.isEmpty()) {
            continue;
        }
        if (onlyLocal) {
            request.insert(QStringLiteral("localOnly"), true);
        }
        if (!resolveConnectionDecision(request, decision, remember)) {
            continue;
        }
        m_pendingPrompts.removeAt(i);
        ++resolved;
    }

    if (resolved > 0 || hadPrunedRows) {
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

int FirewallServiceClient::prunePendingPrompts()
{
    const int before = m_pendingPrompts.size();
    if (!pruneStalePendingPrompts()) {
        return 0;
    }
    emit pendingPromptsChanged();
    const int after = m_pendingPrompts.size();
    return before > after ? (before - after) : 0;
}

bool FirewallServiceClient::pruneStalePendingPrompts()
{
    if (m_pendingPrompts.isEmpty()) {
        return false;
    }

    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    bool changed = false;
    for (int i = m_pendingPrompts.size() - 1; i >= 0; --i) {
        const QVariantMap row = m_pendingPrompts.at(i).toMap();
        const QString receivedRaw = row.value(QStringLiteral("receivedAt")).toString().trimmed();
        if (receivedRaw.isEmpty()) {
            continue;
        }
        const QDateTime receivedAt = QDateTime::fromString(receivedRaw, Qt::ISODate);
        if (!receivedAt.isValid()) {
            continue;
        }
        const qint64 ageSecs = receivedAt.secsTo(nowUtc);
        if (ageSecs <= kPendingPromptTtlSecs) {
            continue;
        }
        m_pendingPrompts.removeAt(i);
        changed = true;
    }
    return changed;
}

bool FirewallServiceClient::sortPendingPromptsByExpiry()
{
    if (m_pendingPrompts.size() < 2) {
        return false;
    }

    const QVariantList before = m_pendingPrompts;
    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    std::stable_sort(m_pendingPrompts.begin(), m_pendingPrompts.end(),
                     [&](const QVariant &leftVar, const QVariant &rightVar) {
        const QVariantMap left = leftVar.toMap();
        const QVariantMap right = rightVar.toMap();

        const qint64 leftRemaining = pendingPromptRemainingSeconds(left, nowUtc);
        const qint64 rightRemaining = pendingPromptRemainingSeconds(right, nowUtc);
        if (leftRemaining != rightRemaining) {
            return leftRemaining < rightRemaining;
        }

        const QString leftReceived = left.value(QStringLiteral("receivedAt")).toString();
        const QString rightReceived = right.value(QStringLiteral("receivedAt")).toString();
        const qint64 leftMs = QDateTime::fromString(leftReceived, Qt::ISODate).toMSecsSinceEpoch();
        const qint64 rightMs = QDateTime::fromString(rightReceived, Qt::ISODate).toMSecsSinceEpoch();
        return leftMs < rightMs;
    });

    return m_pendingPrompts != before;
}

QString FirewallServiceClient::promptThrottleKey(const QVariantMap &request,
                                                 const QVariantMap &evaluation) const
{
    const QVariantMap actor = evaluation.value(QStringLiteral("actor")).toMap();
    const QVariantMap identity = evaluation.value(QStringLiteral("identity")).toMap();
    const QVariantMap target = evaluation.value(QStringLiteral("target")).toMap();

    const QString appId = identity.value(QStringLiteral("app_id")).toString().trimmed().toLower();
    const QString actorName = actor.value(QStringLiteral("appName")).toString().trimmed().toLower();
    const QString executable = actor.value(QStringLiteral("executable")).toString().trimmed().toLower();
    const QString scriptTarget = actor.value(QStringLiteral("scriptTarget")).toString().trimmed().toLower();
    const QString direction = request.value(QStringLiteral("direction"),
                                            evaluation.value(QStringLiteral("direction"),
                                                             QStringLiteral("incoming")))
                                  .toString().trimmed().toLower();

    const QString endpointIp = target.value(QStringLiteral("ip"),
                                            request.value(QStringLiteral("destinationIp"),
                                                          request.value(QStringLiteral("remoteIp"),
                                                                        request.value(QStringLiteral("ip")))))
                                   .toString().trimmed().toLower();
    const QString endpointHost = target.value(QStringLiteral("host"),
                                              request.value(QStringLiteral("destinationHost"),
                                                            request.value(QStringLiteral("host"))))
                                     .toString().trimmed().toLower();
    const QString endpointPort = target.value(QStringLiteral("port"),
                                              request.value(QStringLiteral("destinationPort"),
                                                            request.value(QStringLiteral("port"))))
                                     .toString().trimmed().toLower();

    const QString actorKey = !appId.isEmpty()
            ? appId
            : (!actorName.isEmpty()
               ? actorName
               : (!scriptTarget.isEmpty() ? scriptTarget : executable));
    const QString endpointKey = !endpointHost.isEmpty()
            ? endpointHost
            : endpointIp;

    if (actorKey.isEmpty() || endpointKey.isEmpty()) {
        return {};
    }

    return QStringLiteral("%1|%2|%3|%4")
            .arg(actorKey, direction, endpointKey, endpointPort);
}

void FirewallServiceClient::prunePromptThrottleCache(qint64 nowSecs)
{
    if (m_lastPromptEpochByKey.isEmpty()) {
        return;
    }

    for (auto it = m_lastPromptEpochByKey.begin(); it != m_lastPromptEpochByKey.end(); ) {
        if ((nowSecs - it.value()) > kPromptDuplicateSuppressCacheSecs) {
            it = m_lastPromptEpochByKey.erase(it);
        } else {
            ++it;
        }
    }
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
    m_lastPromptEpochByKey.clear();
    delete m_iface;
    m_iface = nullptr;
}

void FirewallServiceClient::onFirewallStateChanged(const QVariantMap &state)
{
    applyStateMap(state);
    refreshAppPolicies();
    refreshIpPolicies();
    refreshConnections();
    if (pruneStalePendingPrompts()) {
        emit pendingPromptsChanged();
    }
}

void FirewallServiceClient::onPolicyChanged(const QVariantMap &change)
{
    const QVariantMap normalized = normalizeDbusMap(change);
    const QString kind = normalized.value(QStringLiteral("kind")).toString().trimmed().toLower();
    if (kind == QLatin1String("app")) {
        refreshAppPolicies();
        return;
    }
    if (kind == QLatin1String("ip")) {
        refreshIpPolicies();
        refreshConnections();
        return;
    }
    refreshAppPolicies();
    refreshIpPolicies();
    refreshConnections();
}

void FirewallServiceClient::onConnectionPromptRequested(const QVariantMap &prompt)
{
    const bool hadPrunedRows = pruneStalePendingPrompts();
    const QVariantMap normalizedPrompt = normalizeDbusMap(prompt);
    const QVariantMap request = normalizedPrompt.value(QStringLiteral("request")).toMap();
    const QVariantMap evaluation = normalizedPrompt.value(QStringLiteral("evaluation")).toMap();
    if (request.isEmpty() || evaluation.isEmpty()) {
        if (hadPrunedRows) {
            emit pendingPromptsChanged();
        }
        return;
    }

    const qint64 nowSecs = QDateTime::currentSecsSinceEpoch();
    prunePromptThrottleCache(nowSecs);
    const QString throttleKey = promptThrottleKey(request, evaluation);
    if (!throttleKey.isEmpty()) {
        const qint64 lastSeenSecs = m_lastPromptEpochByKey.value(throttleKey, 0);
        const bool inSuppressWindow = lastSeenSecs > 0
                && (nowSecs - lastSeenSecs) < kPromptDuplicateSuppressSecs;
        m_lastPromptEpochByKey.insert(throttleKey, nowSecs);
        if (inSuppressWindow) {
            for (int i = 0; i < m_pendingPrompts.size(); ++i) {
                QVariantMap existing = m_pendingPrompts.at(i).toMap();
                if (existing.value(QStringLiteral("throttleKey")).toString() != throttleKey) {
                    continue;
                }
                const int priorCount = existing.value(QStringLiteral("duplicateCount"), 0).toInt();
                existing.insert(QStringLiteral("duplicateCount"), (priorCount < 0 ? 0 : priorCount) + 1);
                m_pendingPrompts[i] = existing;
                emit pendingPromptsChanged();
                return;
            }
            if (hadPrunedRows) {
                emit pendingPromptsChanged();
            }
            return;
        }
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
            const int priorCount = existing.value(QStringLiteral("duplicateCount"), 0).toInt();
            const int duplicateCount = (priorCount < 0 ? 0 : priorCount) + 1;
            QVariantMap updated = existing;
            updated.insert(QStringLiteral("duplicateCount"), duplicateCount);
            updated.insert(QStringLiteral("receivedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
            if (!throttleKey.isEmpty()) {
                updated.insert(QStringLiteral("throttleKey"), throttleKey);
            }
            const int rowIndex = m_pendingPrompts.indexOf(item);
            if (rowIndex >= 0) {
                m_pendingPrompts[rowIndex] = updated;
                sortPendingPromptsByExpiry();
                emit pendingPromptsChanged();
            }
            return;
        }
    }

    QVariantMap row = normalizedPrompt;
    row.insert(QStringLiteral("duplicateCount"), 0);
    row.insert(QStringLiteral("receivedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    if (!throttleKey.isEmpty()) {
        row.insert(QStringLiteral("throttleKey"), throttleKey);
    }
    m_pendingPrompts.append(row);
    while (m_pendingPrompts.size() > 20) {
        m_pendingPrompts.removeFirst();
    }
    sortPendingPromptsByExpiry();
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
                                              QStringLiteral("PolicyChanged"),
                                              this,
                                              SLOT(onPolicyChanged(QVariantMap)));
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
