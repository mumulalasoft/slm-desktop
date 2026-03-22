#include "PermissionBroker.h"

namespace Slm::Permissions {

PermissionBroker::PermissionBroker() = default;

void PermissionBroker::setStore(PermissionStore *store)
{
    m_store = store;
}

void PermissionBroker::setPolicyEngine(PolicyEngine *engine)
{
    m_engine = engine;
}

void PermissionBroker::setAuditLogger(AuditLogger *logger)
{
    m_audit = logger;
}

PolicyDecision PermissionBroker::requestAccess(const CallerIdentity &caller,
                                               const AccessContext &context) const
{
    PolicyDecision denied;
    denied.capability = context.capability;
    denied.type = DecisionType::Deny;
    denied.reason = QStringLiteral("permission-broker-uninitialized");

    if (!m_engine) {
        if (m_audit) {
            m_audit->recordDecision(caller, context, denied);
        }
        return denied;
    }

    if (m_store && !caller.appId.trimmed().isEmpty()) {
        m_store->upsertAppRegistry(caller);
    }

    PolicyDecision decision = m_engine->evaluate(caller, context);
    if (m_audit) {
        m_audit->recordDecision(caller, context, decision);
    }
    return decision;
}

} // namespace Slm::Permissions

