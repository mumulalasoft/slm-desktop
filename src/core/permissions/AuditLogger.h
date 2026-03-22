#pragma once

#include "AccessContext.h"
#include "CallerIdentity.h"
#include "PermissionStore.h"
#include "PolicyDecision.h"

namespace Slm::Permissions {

class AuditLogger {
public:
    explicit AuditLogger(PermissionStore *store = nullptr);
    void setStore(PermissionStore *store);

    void recordDecision(const CallerIdentity &caller,
                        const AccessContext &context,
                        const PolicyDecision &decision) const;

private:
    PermissionStore *m_store = nullptr;
};

} // namespace Slm::Permissions

