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
            return {
                {QStringLiteral("ok"), true},
                {QStringLiteral("decision"), QStringLiteral("prompt")},
                {QStringLiteral("direction"), direction},
                {QStringLiteral("source"), QStringLiteral("cli-default-prompt")},
                {QStringLiteral("processKind"), processKind},
                {QStringLiteral("identity"), identity},
            };
        }
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
