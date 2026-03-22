#include "DBusSecurityGuard.h"

#include <QDateTime>
#include <QProcessEnvironment>

namespace Slm::Permissions {
namespace {

QString methodKey(const QString &iface, const QString &method)
{
    return iface.trimmed().toLower() + QStringLiteral(".") + method.trimmed().toLower();
}

} // namespace

DBusSecurityGuard::DBusSecurityGuard()
{
    m_enabled = QProcessEnvironment::systemEnvironment().value(QStringLiteral("SLM_SECURITY_DISABLED")).toInt() != 1;
}

void DBusSecurityGuard::setTrustResolver(TrustResolver *resolver)
{
    m_resolver = resolver;
}

void DBusSecurityGuard::setPermissionBroker(PermissionBroker *broker)
{
    m_broker = broker;
}
 
void DBusSecurityGuard::setEnabled(bool enabled)
{
    m_enabled = enabled;
}
 
bool DBusSecurityGuard::isEnabled() const
{
    return m_enabled;
}

void DBusSecurityGuard::registerMethodCapability(const QString &interfaceName,
                                                 const QString &methodName,
                                                 Capability capability)
{
    m_methodToCapability.insert(methodKey(interfaceName, methodName), capability);
}

Capability DBusSecurityGuard::capabilityForMethod(const QString &interfaceName,
                                                  const QString &methodName) const
{
    return m_methodToCapability.value(methodKey(interfaceName, methodName), Capability::Unknown);
}

AccessContext DBusSecurityGuard::normalizeContext(const CallerIdentity &caller,
                                                  Capability capability,
                                                  const QVariantMap &rawContext) const
{
    AccessContext context = accessContextFromVariantMap(rawContext);
    context.capability = capability;
    if (context.timestamp <= 0) {
        context.timestamp = QDateTime::currentMSecsSinceEpoch();
    }

    // Only trusted desktop components may propagate gesture marker.
    if (caller.trustLevel == TrustLevel::ThirdPartyApplication && !caller.isOfficialComponent) {
        context.initiatedByUserGesture = false;
        context.initiatedFromOfficialUI = false;
    }
    return context;
}

PolicyDecision DBusSecurityGuard::check(const QDBusMessage &message,
                                        Capability capability,
                                        const QVariantMap &rawContext) const
{
    PolicyDecision denied;
    denied.capability = capability;
    denied.type = DecisionType::Deny;
    denied.reason = QStringLiteral("security-guard-uninitialized");

    if (!m_enabled) {
        PolicyDecision allowed;
        allowed.capability = capability;
        allowed.type = DecisionType::Allow;
        allowed.reason = QStringLiteral("security-guard-disabled");
        return allowed;
    }
 
    if (!m_resolver || !m_broker) {
        return denied;
    }
    CallerIdentity caller = resolveCallerIdentityFromDbus(message);
    caller = m_resolver->resolveTrust(caller);
    const AccessContext context = normalizeContext(caller, capability, rawContext);
    return m_broker->requestAccess(caller, context);
}

PolicyDecision DBusSecurityGuard::checkMethod(const QDBusMessage &message,
                                              const QString &interfaceName,
                                              const QString &methodName,
                                              const QVariantMap &rawContext) const
{
    const Capability capability = capabilityForMethod(interfaceName, methodName);
    if (capability == Capability::Unknown) {
        PolicyDecision denied;
        denied.type = DecisionType::Deny;
        denied.capability = Capability::Unknown;
        denied.reason = QStringLiteral("method-capability-not-mapped");
        return denied;
    }
    return check(message, capability, rawContext);
}

} // namespace Slm::Permissions

