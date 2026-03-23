#pragma once

#include "PortalCapabilityMapper.h"
#include "PortalDialogBridge.h"
#include "PortalPermissionStoreAdapter.h"
#include "PortalRequestManager.h"
#include "PortalSessionManager.h"

#include "../../../core/permissions/AccessContext.h"
#include "../../../core/permissions/CallerIdentity.h"
#include "../../../core/permissions/PolicyDecision.h"

#include <QDBusMessage>
#include <QObject>
#include <QVariantMap>

namespace Slm::Permissions {
class AuditLogger;
class PermissionBroker;
class TrustResolver;
}

namespace Slm::PortalAdapter {

class PortalAccessMediator : public QObject
{
    Q_OBJECT

public:
    explicit PortalAccessMediator(QObject *parent = nullptr);

    void setTrustResolver(Slm::Permissions::TrustResolver *resolver);
    void setPermissionBroker(Slm::Permissions::PermissionBroker *broker);
    void setAuditLogger(Slm::Permissions::AuditLogger *auditLogger);
    void setPermissionStoreAdapter(PortalPermissionStoreAdapter *storeAdapter);
    void setCapabilityMapper(PortalCapabilityMapper *mapper);
    void setDialogBridge(PortalDialogBridge *dialogBridge);
    void setRequestManager(PortalRequestManager *requestManager);
    void setSessionManager(PortalSessionManager *sessionManager);

    QVariantMap handlePortalRequest(const QString &portalMethod,
                                    const QDBusMessage &message,
                                    const QVariantMap &parameters);
    QVariantMap handlePortalDirect(const QString &portalMethod,
                                   const QDBusMessage &message,
                                   const QVariantMap &parameters);
    QVariantMap handlePortalSessionRequest(const QString &portalMethod,
                                           const QDBusMessage &message,
                                           const QVariantMap &parameters);
    QVariantMap handlePortalSessionOperation(const QString &portalMethod,
                                             const QDBusMessage &message,
                                             const QString &sessionPath,
                                             const QVariantMap &parameters,
                                             bool requireActive = true);

private:
    Slm::Permissions::AccessContext buildAccessContext(const PortalMethodSpec &spec,
                                                       const Slm::Permissions::CallerIdentity &caller,
                                                       const QVariantMap &parameters) const;
    QVariantMap successWithHandle(const QString &requestPath,
                                  const QVariantMap &extra = {}) const;
    static Slm::Permissions::DecisionType decisionFromConsent(UserDecision decision);

    Slm::Permissions::TrustResolver *m_trustResolver = nullptr;
    Slm::Permissions::PermissionBroker *m_permissionBroker = nullptr;
    Slm::Permissions::AuditLogger *m_auditLogger = nullptr;
    PortalPermissionStoreAdapter *m_storeAdapter = nullptr;
    PortalCapabilityMapper *m_mapper = nullptr;
    PortalDialogBridge *m_dialogBridge = nullptr;
    PortalRequestManager *m_requestManager = nullptr;
    PortalSessionManager *m_sessionManager = nullptr;
};

} // namespace Slm::PortalAdapter
