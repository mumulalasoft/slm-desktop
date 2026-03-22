#pragma once

#include "AccessContext.h"
#include "CallerIdentity.h"
#include "PermissionStore.h"
#include "PolicyDecision.h"
#include "SensitiveContentPolicy.h"

#include <QHash>

namespace Slm::Permissions {

class PolicyEngine {
public:
    PolicyEngine();

    void setStore(PermissionStore *store);
    void setSensitiveContentPolicy(const SensitiveContentPolicy &policy);

    PolicyDecision evaluate(const CallerIdentity &caller, const AccessContext &context) const;

    SensitivityLevel sensitivityForCapability(Capability capability) const;
    bool requiresUserGesture(Capability capability) const;

private:
    PolicyDecision evaluateCoreComponent(const CallerIdentity &caller, const AccessContext &context) const;
    PolicyDecision evaluatePrivilegedService(const CallerIdentity &caller, const AccessContext &context) const;
    PolicyDecision evaluateThirdParty(const CallerIdentity &caller, const AccessContext &context) const;
    DecisionType mapStoredDecision(DecisionType stored) const;

    PermissionStore *m_store = nullptr;
    SensitiveContentPolicy m_sensitivePolicy;
    QHash<Capability, SensitivityLevel> m_sensitivity;
    QHash<Capability, bool> m_requiresGesture;
};

} // namespace Slm::Permissions

