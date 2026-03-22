#pragma once

#include "AccessContext.h"
#include "CallerIdentity.h"
#include "PermissionBroker.h"
#include "PolicyDecision.h"
#include "TrustResolver.h"

#include <QDBusMessage>
#include <QHash>
#include <QString>
#include <QVariantMap>

namespace Slm::Permissions {

class DBusSecurityGuard {
public:
    DBusSecurityGuard();

    void setTrustResolver(TrustResolver *resolver);
    void setPermissionBroker(PermissionBroker *broker);
    void setEnabled(bool enabled);
    bool isEnabled() const;

    void registerMethodCapability(const QString &interfaceName,
                                  const QString &methodName,
                                  Capability capability);
    Capability capabilityForMethod(const QString &interfaceName,
                                   const QString &methodName) const;

    PolicyDecision check(const QDBusMessage &message,
                         Capability capability,
                         const QVariantMap &rawContext = {}) const;
    PolicyDecision checkMethod(const QDBusMessage &message,
                               const QString &interfaceName,
                               const QString &methodName,
                               const QVariantMap &rawContext = {}) const;

private:
    AccessContext normalizeContext(const CallerIdentity &caller,
                                   Capability capability,
                                   const QVariantMap &rawContext) const;

    TrustResolver *m_resolver = nullptr;
    PermissionBroker *m_broker = nullptr;
    bool m_enabled = true;
    QHash<QString, Capability> m_methodToCapability;
};

} // namespace Slm::Permissions

