#include "policyengine.h"

#include "appidentityclient.h"
#include "nftablesadapter.h"
#include "policystore.h"

#include <QDateTime>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QUuid>

namespace Slm::Firewall {

namespace {
QString normalizedString(const QVariant &value)
{
    return value.toString().trimmed();
}

QString normalizeScope(const QVariant &value)
{
    const QString scope = normalizedString(value).toLower();
    if (scope == QLatin1String("incoming")
            || scope == QLatin1String("outgoing")
            || scope == QLatin1String("both")) {
        return scope;
    }
    return QStringLiteral("both");
}

QString normalizeDirection(const QVariant &value)
{
    const QString direction = normalizedString(value).toLower();
    if (direction == QLatin1String("incoming") || direction == QLatin1String("outgoing")) {
        return direction;
    }
    return QStringLiteral("incoming");
}

QString normalizeType(const QVariantMap &policy)
{
    const QString explicitType = normalizedString(policy.value(QStringLiteral("type"))).toLower();
    if (!explicitType.isEmpty()) {
        return explicitType;
    }
    if (policy.contains(QStringLiteral("cidr"))) {
        return QStringLiteral("subnet");
    }
    if (policy.contains(QStringLiteral("addresses"))) {
        return QStringLiteral("list");
    }
    return QStringLiteral("ip");
}

QString normalizeDecision(const QVariant &value)
{
    const QString decision = normalizedString(value).toLower();
    if (decision == QLatin1String("allow")
            || decision == QLatin1String("deny")
            || decision == QLatin1String("prompt")) {
        return decision;
    }
    return QStringLiteral("prompt");
}

QVariantMap normalizeAppPolicy(const QVariantMap &policy, QString *error)
{
    if (error) {
        *error = QString();
    }
    const QString appId = normalizedString(policy.value(QStringLiteral("appId")));
    if (appId.isEmpty()) {
        if (error) {
            *error = QStringLiteral("app-policy-missing-app-id");
        }
        return {};
    }
    const QString appName = normalizedString(policy.value(QStringLiteral("appName")));
    return {
        {QStringLiteral("policyId"), QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {QStringLiteral("createdAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("appId"), appId},
        {QStringLiteral("appName"), appName},
        {QStringLiteral("direction"), normalizeScope(policy.value(QStringLiteral("direction")))},
        {QStringLiteral("decision"), normalizeDecision(policy.value(QStringLiteral("decision")))},
        {QStringLiteral("remember"), policy.value(QStringLiteral("remember"), true).toBool()},
        {QStringLiteral("enabled"), true},
    };
}

QVariantMap normalizeIpBlockPolicy(const QVariantMap &policy, QString *error)
{
    if (error) {
        error->clear();
    }

    const QString type = normalizeType(policy);
    QStringList targets;
    if (type == QLatin1String("ip")) {
        const QString ip = normalizedString(policy.value(QStringLiteral("ip")));
        if (ip.isEmpty()) {
            if (error) {
                *error = QStringLiteral("ip-policy-missing-ip");
            }
            return {};
        }
        targets << ip;
    } else if (type == QLatin1String("subnet")) {
        const QString cidr = normalizedString(policy.value(QStringLiteral("cidr")));
        if (cidr.isEmpty() || !cidr.contains(QLatin1Char('/'))) {
            if (error) {
                *error = QStringLiteral("ip-policy-invalid-cidr");
            }
            return {};
        }
        targets << cidr;
    } else if (type == QLatin1String("list")) {
        const QVariantList raw = policy.value(QStringLiteral("addresses")).toList();
        for (const QVariant &item : raw) {
            const QString value = normalizedString(item);
            if (!value.isEmpty()) {
                targets << value;
            }
        }
        if (targets.isEmpty()) {
            if (error) {
                *error = QStringLiteral("ip-policy-empty-list");
            }
            return {};
        }
    } else {
        if (error) {
            *error = QStringLiteral("ip-policy-invalid-type");
        }
        return {};
    }

    const bool temporary = policy.value(QStringLiteral("temporary"), false).toBool();
    const QString duration = normalizedString(policy.value(QStringLiteral("duration")));
    const QString reason = normalizedString(policy.value(QStringLiteral("reason")));
    const QString note = normalizedString(policy.value(QStringLiteral("note")));

    return {
        {QStringLiteral("policyId"), QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {QStringLiteral("createdAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("type"), type},
        {QStringLiteral("scope"), normalizeScope(policy.value(QStringLiteral("scope")))},
        {QStringLiteral("targets"), targets},
        {QStringLiteral("temporary"), temporary},
        {QStringLiteral("duration"), temporary ? (duration.isEmpty() ? QStringLiteral("1h") : duration) : QString()},
        {QStringLiteral("reason"), reason},
        {QStringLiteral("note"), note},
        {QStringLiteral("hitCount"), 0},
        {QStringLiteral("enabled"), true},
    };
}

QStringList buildIpBlockRules(const QVariantMap &normalizedPolicy)
{
    const QString scope = normalizedPolicy.value(QStringLiteral("scope"), QStringLiteral("both")).toString();
    const QStringList targets = normalizedPolicy.value(QStringLiteral("targets")).toStringList();
    QStringList rules;
    for (const QString &target : targets) {
        if (scope == QLatin1String("incoming") || scope == QLatin1String("both")) {
            rules << QStringLiteral("add rule inet slm_firewall input ip saddr %1 drop").arg(target);
        }
        if (scope == QLatin1String("outgoing") || scope == QLatin1String("both")) {
            rules << QStringLiteral("add rule inet slm_firewall output ip daddr %1 drop").arg(target);
        }
    }
    return rules;
}

qint64 extractPidFromProcessField(const QString &processField)
{
    static const QRegularExpression pidRx(QStringLiteral("pid=(\\d+)"));
    const QRegularExpressionMatch match = pidRx.match(processField);
    if (!match.hasMatch()) {
        return -1;
    }
    return match.captured(1).toLongLong();
}

bool scopeMatchesDirection(const QString &scope, const QString &direction)
{
    if (scope == QLatin1String("both")) {
        return true;
    }
    return scope == direction;
}

bool readFirewallEnabled(const PolicyStore *store)
{
    if (!store) {
        return true;
    }
    if (store->snapshot().contains(QStringLiteral("firewall.enabled"))) {
        return store->value(QStringLiteral("firewall.enabled"), true).toBool();
    }
    const QVariantMap nested = store->value(QStringLiteral("firewall"), QVariantMap{}).toMap();
    return nested.value(QStringLiteral("enabled"), true).toBool();
}

QString readDefaultPolicy(const PolicyStore *store, const QString &direction)
{
    if (!store) {
        return direction == QLatin1String("outgoing") ? QStringLiteral("allow") : QStringLiteral("deny");
    }

    const QString key = direction == QLatin1String("outgoing")
        ? QStringLiteral("firewall.defaultOutgoingPolicy")
        : QStringLiteral("firewall.defaultIncomingPolicy");
    const QString nestedKey = direction == QLatin1String("outgoing")
        ? QStringLiteral("defaultOutgoingPolicy")
        : QStringLiteral("defaultIncomingPolicy");
    const QString fallback = direction == QLatin1String("outgoing")
        ? QStringLiteral("allow")
        : QStringLiteral("deny");

    if (store->snapshot().contains(key)) {
        return normalizeDecision(store->value(key, fallback));
    }
    const QVariantMap nested = store->value(QStringLiteral("firewall"), QVariantMap{}).toMap();
    return normalizeDecision(nested.value(nestedKey, fallback));
}

int readPromptCooldownSeconds(const PolicyStore *store)
{
    if (!store) {
        return 20;
    }
    const int raw = store->value(QStringLiteral("firewall.promptCooldownSeconds"), 20).toInt();
    if (raw < 1) {
        return 1;
    }
    if (raw > 300) {
        return 300;
    }
    return raw;
}

QString classifyProcessKind(const QVariantMap &identity)
{
    const QString executable = identity.value(QStringLiteral("executable")).toString();
    const QString exeBase = QFileInfo(executable).fileName().toLower();

    if (exeBase == QLatin1String("apt")
            || exeBase == QLatin1String("apt-get")
            || exeBase == QLatin1String("dpkg")
            || exeBase == QLatin1String("systemd")
            || exeBase == QLatin1String("systemd-resolved")
            || exeBase == QLatin1String("networkmanager")) {
        return QStringLiteral("system");
    }

    if (exeBase == QLatin1String("git")
            || exeBase == QLatin1String("npm")
            || exeBase == QLatin1String("node")
            || exeBase == QLatin1String("cargo")
            || exeBase == QLatin1String("pip")
            || exeBase == QLatin1String("python")
            || exeBase == QLatin1String("python3")) {
        return QStringLiteral("developer");
    }

    if (exeBase == QLatin1String("curl")
            || exeBase == QLatin1String("wget")
            || exeBase == QLatin1String("ssh")) {
        return QStringLiteral("network_tools");
    }

    const QString context = identity.value(QStringLiteral("context")).toString();
    if (context == QLatin1String("interpreter")) {
        return QStringLiteral("interpreter");
    }

    return QStringLiteral("unknown");
}

QString requestIpForDirection(const QVariantMap &request, const QString &direction)
{
    const QString preferred = direction == QLatin1String("incoming")
        ? normalizedString(request.value(QStringLiteral("sourceIp")))
        : normalizedString(request.value(QStringLiteral("destinationIp")));
    if (!preferred.isEmpty()) {
        return preferred;
    }

    const QString remoteIp = normalizedString(request.value(QStringLiteral("remoteIp")));
    if (!remoteIp.isEmpty()) {
        return remoteIp;
    }

    return normalizedString(request.value(QStringLiteral("ip")));
}

bool targetMatchesIp(const QString &target, const QString &ip)
{
    const QString normalizedTarget = target.trimmed();
    if (normalizedTarget.isEmpty() || ip.isEmpty()) {
        return false;
    }

    auto parseIpv4 = [](const QString &raw, quint32 *out) -> bool {
        if (!out) {
            return false;
        }
        const QStringList parts = raw.trimmed().split(QLatin1Char('.'));
        if (parts.size() != 4) {
            return false;
        }
        quint32 value = 0;
        for (const QString &part : parts) {
            bool ok = false;
            const int octet = part.toInt(&ok);
            if (!ok || octet < 0 || octet > 255) {
                return false;
            }
            value = (value << 8) | static_cast<quint32>(octet);
        }
        *out = value;
        return true;
    };

    quint32 candidate = 0;
    if (!parseIpv4(ip, &candidate)) {
        return false;
    }

    if (normalizedTarget.contains(QLatin1Char('/'))) {
        const QStringList subnetParts = normalizedTarget.split(QLatin1Char('/'));
        if (subnetParts.size() != 2) {
            return false;
        }
        quint32 network = 0;
        if (!parseIpv4(subnetParts.at(0), &network)) {
            return false;
        }
        bool ok = false;
        const int prefix = subnetParts.at(1).toInt(&ok);
        if (!ok || prefix < 0 || prefix > 32) {
            return false;
        }
        const quint32 mask = prefix == 0 ? 0 : (0xFFFFFFFFu << (32 - prefix));
        return (candidate & mask) == (network & mask);
    }

    quint32 single = 0;
    if (!parseIpv4(normalizedTarget, &single)) {
        return false;
    }
    return candidate == single;
}
} // namespace

PolicyEngine::PolicyEngine(PolicyStore *store,
                           NftablesAdapter *nft,
                           AppIdentityClient *identity)
    : m_store(store)
    , m_nft(nft)
    , m_identity(identity)
{
}

QVariantMap PolicyEngine::evaluateConnection(const QVariantMap &request) const
{
    const QString direction = normalizeDirection(request.value(QStringLiteral("direction")));
    const QString requestIp = requestIpForDirection(request, direction);
    const qint64 pid = request.value(QStringLiteral("pid"), -1).toLongLong();
    const QVariantMap identity = m_identity ? m_identity->resolveByPid(pid) : QVariantMap{};
    const QString appId = normalizedString(identity.value(QStringLiteral("app_id")));

    if (!readFirewallEnabled(m_store)) {
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("decision"), policyDecisionToString(PolicyDecision::Allow)},
            {QStringLiteral("direction"), direction},
            {QStringLiteral("source"), QStringLiteral("firewall-disabled")},
            {QStringLiteral("identity"), identity},
        };
    }

    if (!requestIp.isEmpty()) {
        const QVariantList ipEntries = listIpPolicies();
        for (auto it = ipEntries.crbegin(); it != ipEntries.crend(); ++it) {
            const QVariantMap entry = it->toMap();
            if (!entry.value(QStringLiteral("enabled"), true).toBool()) {
                continue;
            }
            if (!scopeMatchesDirection(normalizeScope(entry.value(QStringLiteral("scope"))), direction)) {
                continue;
            }
            const QStringList targets = entry.value(QStringLiteral("targets")).toStringList();
            for (const QString &target : targets) {
                if (!targetMatchesIp(target, requestIp)) {
                    continue;
                }
                const QString policyId = entry.value(QStringLiteral("policyId")).toString();
                incrementIpPolicyHitCount(policyId, target, QDateTime::currentDateTimeUtc());
                return {
                    {QStringLiteral("ok"), true},
                    {QStringLiteral("decision"), QStringLiteral("deny")},
                    {QStringLiteral("direction"), direction},
                    {QStringLiteral("source"), QStringLiteral("ip-policy")},
                    {QStringLiteral("policyId"), policyId},
                    {QStringLiteral("matchedTarget"), target},
                    {QStringLiteral("matchedIp"), requestIp},
                    {QStringLiteral("identity"), identity},
                };
            }
        }
    }

    const QVariantList appEntries = listAppPolicies();
    for (auto it = appEntries.crbegin(); it != appEntries.crend(); ++it) {
        const QVariantMap entry = it->toMap();
        if (!entry.value(QStringLiteral("enabled"), true).toBool()) {
            continue;
        }
        if (normalizedString(entry.value(QStringLiteral("appId"))) != appId) {
            continue;
        }
        if (!scopeMatchesDirection(normalizeScope(entry.value(QStringLiteral("direction"))), direction)) {
            continue;
        }
        const QString decision = normalizeDecision(entry.value(QStringLiteral("decision")));
        if (decision == QLatin1String("prompt")) {
            return applyPromptCooldown(identity, direction, QStringLiteral("app-policy"));
        }
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("decision"), decision},
            {QStringLiteral("direction"), direction},
            {QStringLiteral("source"), QStringLiteral("app-policy")},
            {QStringLiteral("policyId"), entry.value(QStringLiteral("policyId")).toString()},
            {QStringLiteral("identity"), identity},
        };
    }

    const QString defaultDecision = readDefaultPolicy(m_store, direction);
    if (direction == QLatin1String("outgoing") && defaultDecision == QLatin1String("allow")) {
        const QString processKind = classifyProcessKind(identity);
        if (processKind == QLatin1String("system") || processKind == QLatin1String("developer")) {
            return {
                {QStringLiteral("ok"), true},
                {QStringLiteral("decision"), QStringLiteral("allow")},
                {QStringLiteral("direction"), direction},
                {QStringLiteral("source"), QStringLiteral("cli-default-allow")},
                {QStringLiteral("processKind"), processKind},
                {QStringLiteral("identity"), identity},
            };
        }
        if (processKind == QLatin1String("network_tools")
                || processKind == QLatin1String("interpreter")
                || processKind == QLatin1String("unknown")) {
            QVariantMap result = applyPromptCooldown(identity, direction, QStringLiteral("cli-default-prompt"));
            result.insert(QStringLiteral("processKind"), processKind);
            return result;
        }
    }

    if (defaultDecision == QLatin1String("prompt")) {
        return applyPromptCooldown(identity,
                                   direction,
                                   direction == QLatin1String("outgoing")
                                       ? QStringLiteral("default-outgoing")
                                       : QStringLiteral("default-incoming"));
    }

    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("decision"), defaultDecision},
        {QStringLiteral("direction"), direction},
        {QStringLiteral("source"), direction == QLatin1String("outgoing")
             ? QStringLiteral("default-outgoing")
             : QStringLiteral("default-incoming")},
        {QStringLiteral("identity"), identity},
    };
}

QVariantMap PolicyEngine::applyPromptCooldown(const QVariantMap &identity,
                                              const QString &direction,
                                              const QString &source) const
{
    const QString appId = normalizedString(identity.value(QStringLiteral("app_id")));
    const QString key = QStringLiteral("%1|%2").arg(appId.isEmpty() ? QStringLiteral("unknown") : appId,
                                                   direction);
    const int cooldownSeconds = readPromptCooldownSeconds(m_store);
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QDateTime last = m_lastPromptAt.value(key);
    if (last.isValid()) {
        const qint64 elapsed = last.secsTo(now);
        if (elapsed >= 0 && elapsed < cooldownSeconds) {
            return {
                {QStringLiteral("ok"), true},
                {QStringLiteral("decision"), QStringLiteral("deny")},
                {QStringLiteral("direction"), direction},
                {QStringLiteral("source"), QStringLiteral("prompt-cooldown")},
                {QStringLiteral("promptSuppressed"), true},
                {QStringLiteral("cooldownRemainingSec"), cooldownSeconds - static_cast<int>(elapsed)},
                {QStringLiteral("identity"), identity},
            };
        }
    }
    m_lastPromptAt.insert(key, now);
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("decision"), QStringLiteral("prompt")},
        {QStringLiteral("direction"), direction},
        {QStringLiteral("source"), source},
        {QStringLiteral("promptSuppressed"), false},
        {QStringLiteral("identity"), identity},
    };
}

QVariantMap PolicyEngine::resolveConnectionDecision(const QVariantMap &request,
                                                    const QString &decision,
                                                    bool remember)
{
    const QString normalized = normalizeDecision(decision);
    if (normalized == QLatin1String("prompt")) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("decision-must-be-allow-or-deny")},
        };
    }

    const QVariantMap evaluation = evaluateConnection(request);
    const QVariantMap identity = evaluation.value(QStringLiteral("identity")).toMap();
    const QString direction = normalizeDirection(request.value(QStringLiteral("direction")));

    QVariantMap result{
        {QStringLiteral("ok"), true},
        {QStringLiteral("decision"), normalized},
        {QStringLiteral("remember"), remember},
        {QStringLiteral("persisted"), false},
    };

    if (!remember) {
        return result;
    }

    const QString appId = normalizedString(identity.value(QStringLiteral("app_id")));
    if (appId.isEmpty()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("identity-app-id-missing")},
        };
    }

    const QVariantMap persist = setAppPolicy(QVariantMap{
        {QStringLiteral("appId"), appId},
        {QStringLiteral("appName"), identity.value(QStringLiteral("app_name")).toString()},
        {QStringLiteral("decision"), normalized},
        {QStringLiteral("direction"), direction},
        {QStringLiteral("remember"), true},
    });
    if (!persist.value(QStringLiteral("ok"), false).toBool()) {
        return persist;
    }

    result.insert(QStringLiteral("persisted"), true);
    result.insert(QStringLiteral("policy"), persist.value(QStringLiteral("policy")).toMap());
    return result;
}

QVariantMap PolicyEngine::applyBasePolicy(const QVariantMap &state)
{
    if (!m_nft) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("nft-unavailable")},
        };
    }
    QString error;
    const bool ok = m_nft->applyBasePolicy(state, &error);
    return {
        {QStringLiteral("ok"), ok},
        {QStringLiteral("error"), error},
    };
}

QVariantMap PolicyEngine::setAppPolicy(const QVariantMap &policy)
{
    if (!m_store) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("store-unavailable")},
        };
    }
    QString normalizeError;
    const QVariantMap normalizedPolicy = normalizeAppPolicy(policy, &normalizeError);
    if (normalizedPolicy.isEmpty()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), normalizeError.isEmpty() ? QStringLiteral("app-policy-invalid") : normalizeError},
        };
    }

    QVariantList entries = m_store->value(QStringLiteral("firewall.rules.apps.entries"), QVariantList{}).toList();
    QVariantList nextEntries;
    bool replaced = false;
    for (const QVariant &entry : entries) {
        const QVariantMap map = entry.toMap();
        const bool sameApp = normalizedString(map.value(QStringLiteral("appId")))
                == normalizedString(normalizedPolicy.value(QStringLiteral("appId")));
        const bool sameDirection = normalizeScope(map.value(QStringLiteral("direction")))
                == normalizeScope(normalizedPolicy.value(QStringLiteral("direction")));
        if (!replaced && sameApp && sameDirection) {
            QVariantMap updated = normalizedPolicy;
            const QString existingPolicyId = map.value(QStringLiteral("policyId")).toString();
            if (!existingPolicyId.isEmpty()) {
                updated.insert(QStringLiteral("policyId"), existingPolicyId);
            }
            const QString createdAt = map.value(QStringLiteral("createdAt")).toString();
            if (!createdAt.isEmpty()) {
                updated.insert(QStringLiteral("createdAt"), createdAt);
            }
            nextEntries.append(updated);
            replaced = true;
            continue;
        }
        nextEntries.append(map);
    }
    if (!replaced) {
        nextEntries.append(normalizedPolicy);
    }

    QString error;
    const bool ok = m_store->setValue(QStringLiteral("firewall.rules.apps.entries"), nextEntries, &error)
        && m_store->setValue(QStringLiteral("firewall.rules.apps.last"), normalizedPolicy, &error);
    return {
        {QStringLiteral("ok"), ok},
        {QStringLiteral("error"), error},
        {QStringLiteral("policy"), normalizedPolicy},
    };
}

QVariantList PolicyEngine::listAppPolicies() const
{
    if (!m_store) {
        return {};
    }
    return m_store->value(QStringLiteral("firewall.rules.apps.entries"), QVariantList{}).toList();
}

QVariantMap PolicyEngine::clearAppPolicies()
{
    if (!m_store) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("store-unavailable")},
        };
    }

    QString error;
    const bool ok = m_store->setValue(QStringLiteral("firewall.rules.apps.entries"), QVariantList{}, &error);
    return {
        {QStringLiteral("ok"), ok},
        {QStringLiteral("error"), error},
    };
}

QVariantMap PolicyEngine::removeAppPolicy(const QString &policyId)
{
    if (!m_store) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("store-unavailable")},
        };
    }

    const QString id = policyId.trimmed();
    if (id.isEmpty()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("policy-id-empty")},
        };
    }

    const QVariantList entries = m_store->value(QStringLiteral("firewall.rules.apps.entries"), QVariantList{}).toList();
    QVariantList kept;
    bool removed = false;
    for (const QVariant &entry : entries) {
        const QVariantMap map = entry.toMap();
        if (!removed && map.value(QStringLiteral("policyId")).toString() == id) {
            removed = true;
            continue;
        }
        kept.append(map);
    }

    if (!removed) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("policy-id-not-found")},
        };
    }

    QString error;
    const bool ok = m_store->setValue(QStringLiteral("firewall.rules.apps.entries"), kept, &error);
    return {
        {QStringLiteral("ok"), ok},
        {QStringLiteral("error"), error},
    };
}

QVariantMap PolicyEngine::setIpPolicy(const QVariantMap &policy)
{
    if (!m_store) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("store-unavailable")},
        };
    }
    QString normalizeError;
    const QVariantMap normalizedPolicy = normalizeIpBlockPolicy(policy, &normalizeError);
    if (normalizedPolicy.isEmpty()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), normalizeError.isEmpty() ? QStringLiteral("ip-policy-invalid") : normalizeError},
        };
    }

    QVariantList entries = pruneExpiredIpPolicies();
    entries.append(normalizedPolicy);

    QString error;
    const bool ok = m_store->setValue(QStringLiteral("firewall.rules.ipBlocks.entries"), entries, &error);
    if (ok && m_nft) {
        const QStringList rules = buildIpBlockRules(normalizedPolicy);
        QString nftError;
        if (!m_nft->applyAtomicBatch(rules, &nftError)) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("error"), nftError.isEmpty() ? QStringLiteral("nft-apply-failed") : nftError},
            };
        }
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("error"), QString()},
            {QStringLiteral("appliedRules"), rules.size()},
            {QStringLiteral("policy"), normalizedPolicy},
        };
    }
    return {
        {QStringLiteral("ok"), ok},
        {QStringLiteral("error"), error},
        {QStringLiteral("policy"), normalizedPolicy},
    };
}

QVariantList PolicyEngine::listIpPolicies() const
{
    if (!m_store) {
        return {};
    }
    return pruneExpiredIpPolicies();
}

QVariantMap PolicyEngine::clearIpPolicies()
{
    if (!m_store) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("store-unavailable")},
        };
    }

    QString error;
    const bool ok = m_store->setValue(QStringLiteral("firewall.rules.ipBlocks.entries"), QVariantList{}, &error);
    if (!ok) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), error},
        };
    }

    if (m_nft) {
        QString nftError;
        if (!m_nft->reconcileState(&nftError)) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("error"), nftError.isEmpty() ? QStringLiteral("nft-reconcile-failed") : nftError},
            };
        }
    }

    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("error"), QString()},
    };
}

QVariantMap PolicyEngine::removeIpPolicy(const QString &policyId)
{
    if (!m_store) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("store-unavailable")},
        };
    }

    const QString id = policyId.trimmed();
    if (id.isEmpty()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("policy-id-empty")},
        };
    }

    const QVariantList entries = pruneExpiredIpPolicies();
    QVariantList kept;
    bool removed = false;
    for (const QVariant &entry : entries) {
        const QVariantMap map = entry.toMap();
        if (!removed && map.value(QStringLiteral("policyId")).toString() == id) {
            removed = true;
            continue;
        }
        kept.append(map);
    }

    if (!removed) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("policy-id-not-found")},
        };
    }

    QString error;
    if (!m_store->setValue(QStringLiteral("firewall.rules.ipBlocks.entries"), kept, &error)) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), error},
        };
    }

    if (m_nft) {
        QString nftError;
        if (!m_nft->reconcileState(&nftError)) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("error"), nftError.isEmpty() ? QStringLiteral("nft-reconcile-failed") : nftError},
            };
        }
    }

    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("error"), QString()},
    };
}

QVariantList PolicyEngine::pruneExpiredIpPolicies() const
{
    if (!m_store) {
        return {};
    }

    const QVariantList entries = m_store->value(QStringLiteral("firewall.rules.ipBlocks.entries"), QVariantList{}).toList();
    if (entries.isEmpty()) {
        return entries;
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();
    QVariantList kept;
    kept.reserve(entries.size());
    bool changed = false;
    for (const QVariant &entry : entries) {
        const QVariantMap map = entry.toMap();
        if (isIpPolicyExpired(map, now)) {
            changed = true;
            continue;
        }
        kept.append(map);
    }

    if (!changed) {
        return entries;
    }

    QString error;
    if (!m_store->setValue(QStringLiteral("firewall.rules.ipBlocks.entries"), kept, &error)) {
        return kept;
    }

    if (m_nft) {
        QString nftError;
        m_nft->reconcileState(&nftError);
    }

    return kept;
}

void PolicyEngine::incrementIpPolicyHitCount(const QString &policyId,
                                             const QString &matchedTarget,
                                             const QDateTime &hitAtUtc) const
{
    if (!m_store) {
        return;
    }
    const QString id = policyId.trimmed();
    if (id.isEmpty()) {
        return;
    }

    QVariantList entries = pruneExpiredIpPolicies();
    bool updated = false;
    for (int i = 0; i < entries.size(); ++i) {
        QVariantMap map = entries.at(i).toMap();
        if (map.value(QStringLiteral("policyId")).toString() != id) {
            continue;
        }
        map.insert(QStringLiteral("hitCount"), map.value(QStringLiteral("hitCount"), 0).toInt() + 1);
        map.insert(QStringLiteral("lastHitAt"), hitAtUtc.toString(Qt::ISODate));
        if (!matchedTarget.trimmed().isEmpty()) {
            map.insert(QStringLiteral("lastMatchedTarget"), matchedTarget.trimmed());
        }
        entries[i] = map;
        updated = true;
        break;
    }

    if (!updated) {
        return;
    }

    QString error;
    m_store->setValue(QStringLiteral("firewall.rules.ipBlocks.entries"), entries, &error);
}

qint64 PolicyEngine::parseDurationSeconds(const QString &rawDuration)
{
    const QString value = rawDuration.trimmed().toLower();
    if (value.isEmpty()) {
        return -1;
    }

    static const QRegularExpression rx(QStringLiteral("^(\\d+)\\s*([smhd]?)$"));
    const QRegularExpressionMatch match = rx.match(value);
    if (!match.hasMatch()) {
        return -1;
    }

    bool ok = false;
    const qint64 amount = match.captured(1).toLongLong(&ok);
    if (!ok || amount <= 0) {
        return -1;
    }

    const QString unit = match.captured(2);
    if (unit == QLatin1String("m")) {
        return amount * 60;
    }
    if (unit == QLatin1String("h")) {
        return amount * 3600;
    }
    if (unit == QLatin1String("d")) {
        return amount * 86400;
    }
    return amount;
}

bool PolicyEngine::isIpPolicyExpired(const QVariantMap &policy, const QDateTime &nowUtc)
{
    if (!policy.value(QStringLiteral("temporary"), false).toBool()) {
        return false;
    }

    const QString createdAtRaw = policy.value(QStringLiteral("createdAt")).toString();
    const QDateTime createdAt = QDateTime::fromString(createdAtRaw, Qt::ISODate);
    if (!createdAt.isValid()) {
        return false;
    }

    qint64 durationSeconds = parseDurationSeconds(policy.value(QStringLiteral("duration")).toString());
    if (durationSeconds <= 0) {
        durationSeconds = 3600;
    }

    return createdAt.addSecs(durationSeconds) <= nowUtc;
}

QVariantList PolicyEngine::listConnections() const
{
    QProcess proc;
    proc.start(QStringLiteral("ss"),
               QStringList{
                   QStringLiteral("-tunp"),
                   QStringLiteral("-H"),
               });
    if (!proc.waitForFinished(1000) || proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        return {};
    }

    const QString output = QString::fromUtf8(proc.readAllStandardOutput());
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    QVariantList result;
    for (const QString &rawLine : lines) {
        const QString line = rawLine.simplified();
        const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.size() < 6) {
            continue;
        }

        const QString protocol = parts.value(0);
        const QString state = parts.value(1);
        const QString localAddress = parts.value(4);
        const QString remoteAddress = parts.value(5);
        const QString processField = parts.mid(6).join(QLatin1Char(' '));
        const qint64 pid = extractPidFromProcessField(processField);
        const QVariantMap identity = (pid > 0 && m_identity) ? m_identity->resolveByPid(pid) : QVariantMap{};

        result.append(QVariantMap{
            {QStringLiteral("protocol"), protocol},
            {QStringLiteral("state"), state},
            {QStringLiteral("local"), localAddress},
            {QStringLiteral("remote"), remoteAddress},
            {QStringLiteral("pid"), pid},
            {QStringLiteral("process"), processField},
            {QStringLiteral("identity"), identity},
        });
    }

    return result;
}

} // namespace Slm::Firewall
