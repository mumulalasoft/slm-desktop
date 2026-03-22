#include "AuditLogger.h"

namespace Slm::Permissions {

AuditLogger::AuditLogger(PermissionStore *store)
    : m_store(store)
{
}

void AuditLogger::setStore(PermissionStore *store)
{
    m_store = store;
}

void AuditLogger::recordDecision(const CallerIdentity &caller,
                                 const AccessContext &context,
                                 const PolicyDecision &decision) const
{
    if (!m_store) {
        return;
    }
    const QString appId = caller.appId.trimmed().isEmpty() ? caller.busName : caller.appId;
    const QString reason = decision.reason.left(512);
    m_store->appendAudit(appId, context.capability, decision.type, context.resourceType, reason);
}

} // namespace Slm::Permissions

