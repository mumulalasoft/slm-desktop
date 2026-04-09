#include "policyengine.h"

#include "appidentityclient.h"
#include "nftablesadapter.h"
#include "policystore.h"

namespace Slm::Firewall {

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
    QString error;
    const bool ok = m_store->setValue(QStringLiteral("firewall.rules.apps.last"), policy, &error);
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
    QString error;
    const bool ok = m_store->setValue(QStringLiteral("firewall.rules.ipBlocks.last"), policy, &error);
    if (ok && m_nft) {
        QString nftError;
        if (!m_nft->applyAtomicBatch(QStringList{QStringLiteral("# firewall skeleton placeholder")}, &nftError)) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("error"), nftError.isEmpty() ? QStringLiteral("nft-apply-failed") : nftError},
            };
        }
    }
    return {
        {QStringLiteral("ok"), ok},
        {QStringLiteral("error"), error},
    };
}

QVariantList PolicyEngine::listConnections() const
{
    return {};
}

} // namespace Slm::Firewall
