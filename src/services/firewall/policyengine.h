#pragma once

#include "src/daemon/firewalld/firewalltypes.h"

#include <QDateTime>
#include <QHash>
#include <QVariantList>
#include <QVariantMap>

namespace Slm::Firewall {

class PolicyStore;
class NftablesAdapter;
class AppIdentityClient;

class PolicyEngine
{
public:
    PolicyEngine(PolicyStore *store,
                 NftablesAdapter *nft,
                 AppIdentityClient *identity);

    QVariantMap evaluateConnection(const QVariantMap &request) const;
    QVariantMap resolveConnectionDecision(const QVariantMap &request,
                                          const QString &decision,
                                          bool remember);
    QVariantMap applyBasePolicy(const QVariantMap &state);
    QVariantMap setAppPolicy(const QVariantMap &policy);
    QVariantList listAppPolicies() const;
    QVariantMap clearAppPolicies();
    QVariantMap removeAppPolicy(const QString &policyId);
    QVariantMap setIpPolicy(const QVariantMap &policy);
    QVariantList listIpPolicies() const;
    QVariantMap clearIpPolicies();
    QVariantMap removeIpPolicy(const QString &policyId);
    QVariantList listConnections() const;

private:
    QVariantMap applyPromptCooldown(const QVariantMap &identity,
                                    const QString &direction,
                                    const QString &source) const;
    QVariantList pruneExpiredIpPolicies() const;
    static qint64 parseDurationSeconds(const QString &rawDuration);
    static bool isIpPolicyExpired(const QVariantMap &policy, const QDateTime &nowUtc);

    PolicyStore *m_store = nullptr;
    NftablesAdapter *m_nft = nullptr;
    AppIdentityClient *m_identity = nullptr;
    mutable QHash<QString, QDateTime> m_lastPromptAt;
};

} // namespace Slm::Firewall
