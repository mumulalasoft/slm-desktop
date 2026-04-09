#include "policyengine.h"

#include "appidentityclient.h"
#include "nftablesadapter.h"
#include "policystore.h"

#include <QDateTime>
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

    return {
        {QStringLiteral("policyId"), QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {QStringLiteral("createdAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("type"), type},
        {QStringLiteral("scope"), normalizeScope(policy.value(QStringLiteral("scope")))},
        {QStringLiteral("targets"), targets},
        {QStringLiteral("temporary"), temporary},
        {QStringLiteral("duration"), temporary ? (duration.isEmpty() ? QStringLiteral("1h") : duration) : QString()},
        {QStringLiteral("reason"), reason},
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
    const qint64 pid = request.value(QStringLiteral("pid"), -1).toLongLong();
    const QVariantMap identity = m_identity ? m_identity->resolveByPid(pid) : QVariantMap{};
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("decision"), policyDecisionToString(PolicyDecision::Prompt)},
        {QStringLiteral("identity"), identity},
    };
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
    entries.append(normalizedPolicy);

    QString error;
    const bool ok = m_store->setValue(QStringLiteral("firewall.rules.apps.entries"), entries, &error)
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

    QVariantList entries = m_store->value(QStringLiteral("firewall.rules.ipBlocks.entries"), QVariantList{}).toList();
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
    return m_store->value(QStringLiteral("firewall.rules.ipBlocks.entries"), QVariantList{}).toList();
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

    const QVariantList entries = m_store->value(QStringLiteral("firewall.rules.ipBlocks.entries"), QVariantList{}).toList();
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

QVariantList PolicyEngine::listConnections() const
{
    return {};
}

} // namespace Slm::Firewall
