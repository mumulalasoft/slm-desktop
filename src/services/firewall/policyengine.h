#pragma once

#include "src/daemon/firewalld/firewalltypes.h"

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
    QVariantMap applyBasePolicy(const QVariantMap &state);
    QVariantMap setAppPolicy(const QVariantMap &policy);
    QVariantMap setIpPolicy(const QVariantMap &policy);
    QVariantList listIpPolicies() const;
    QVariantMap clearIpPolicies();
    QVariantList listConnections() const;

private:
    PolicyStore *m_store = nullptr;
    NftablesAdapter *m_nft = nullptr;
    AppIdentityClient *m_identity = nullptr;
};

} // namespace Slm::Firewall
