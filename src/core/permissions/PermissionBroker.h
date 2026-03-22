#pragma once

#include "AccessContext.h"
#include "AuditLogger.h"
#include "CallerIdentity.h"
#include "PermissionStore.h"
#include "PolicyDecision.h"
#include "PolicyEngine.h"

namespace Slm::Permissions {

class PermissionBroker {
public:
    PermissionBroker();

    void setStore(PermissionStore *store);
    void setPolicyEngine(PolicyEngine *engine);
    void setAuditLogger(AuditLogger *logger);

    PolicyDecision requestAccess(const CallerIdentity &caller, const AccessContext &context) const;

private:
    PermissionStore *m_store = nullptr;
    PolicyEngine *m_engine = nullptr;
    AuditLogger *m_audit = nullptr;
};

} // namespace Slm::Permissions

