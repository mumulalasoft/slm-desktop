#include "PortalBackendService.h"

#include "PortalResponseSerializer.h"

namespace Slm::PortalAdapter {

PortalBackendService::PortalBackendService(QObject *parent)
    : QObject(parent)
    , m_requestManager(this)
    , m_sessionManager(this)
    , m_capabilityMapper(this)
    , m_storeAdapter(this)
    , m_dialogBridge(this)
    , m_accessMediator(this)
{
    m_accessMediator.setCapabilityMapper(&m_capabilityMapper);
    m_accessMediator.setDialogBridge(&m_dialogBridge);
    m_accessMediator.setPermissionStoreAdapter(&m_storeAdapter);
    m_accessMediator.setRequestManager(&m_requestManager);
    m_accessMediator.setSessionManager(&m_sessionManager);
}

void PortalBackendService::setBus(const QDBusConnection &bus)
{
    m_bus = bus;
    m_requestManager.setBus(bus);
    m_sessionManager.setBus(bus);
}

PortalRequestManager *PortalBackendService::requestManager() const
{
    return const_cast<PortalRequestManager *>(&m_requestManager);
}

PortalSessionManager *PortalBackendService::sessionManager() const
{
    return const_cast<PortalSessionManager *>(&m_sessionManager);
}

PortalCapabilityMapper *PortalBackendService::capabilityMapper() const
{
    return const_cast<PortalCapabilityMapper *>(&m_capabilityMapper);
}

PortalPermissionStoreAdapter *PortalBackendService::permissionStoreAdapter() const
{
    return const_cast<PortalPermissionStoreAdapter *>(&m_storeAdapter);
}

PortalDialogBridge *PortalBackendService::dialogBridge() const
{
    return const_cast<PortalDialogBridge *>(&m_dialogBridge);
}

PortalAccessMediator *PortalBackendService::accessMediator() const
{
    return const_cast<PortalAccessMediator *>(&m_accessMediator);
}

void PortalBackendService::configurePermissions(Slm::Permissions::TrustResolver *trustResolver,
                                                Slm::Permissions::PermissionBroker *permissionBroker,
                                                Slm::Permissions::AuditLogger *auditLogger,
                                                Slm::Permissions::PermissionStore *permissionStore)
{
    m_storeAdapter.setStore(permissionStore);
    m_accessMediator.setTrustResolver(trustResolver);
    m_accessMediator.setPermissionBroker(permissionBroker);
    m_accessMediator.setAuditLogger(auditLogger);
}

QVariantMap PortalBackendService::handleRequest(const QString &portalMethod,
                                                const QDBusMessage &message,
                                                const QVariantMap &parameters)
{
    return m_accessMediator.handlePortalRequest(portalMethod, message, parameters);
}

QVariantMap PortalBackendService::handleDirect(const QString &portalMethod,
                                               const QDBusMessage &message,
                                               const QVariantMap &parameters)
{
    return m_accessMediator.handlePortalDirect(portalMethod, message, parameters);
}

QVariantMap PortalBackendService::handleSessionRequest(const QString &portalMethod,
                                                       const QDBusMessage &message,
                                                       const QVariantMap &parameters)
{
    return m_accessMediator.handlePortalSessionRequest(portalMethod, message, parameters);
}

QVariantMap PortalBackendService::handleSessionOperation(const QString &portalMethod,
                                                         const QDBusMessage &message,
                                                         const QString &sessionPath,
                                                         const QVariantMap &parameters,
                                                         bool requireActive)
{
    return m_accessMediator.handlePortalSessionOperation(portalMethod,
                                                         message,
                                                         sessionPath,
                                                         parameters,
                                                         requireActive);
}

QVariantMap PortalBackendService::closeSession(const QString &sessionPath)
{
    if (m_sessionManager.closeSession(sessionPath)) {
        return PortalResponseSerializer::success({
            {QStringLiteral("closed"), true},
            {QStringLiteral("sessionPath"), sessionPath},
            {QStringLiteral("sessionHandle"), sessionPath},
        });
    }
    return PortalResponseSerializer::error(PortalError::SessionNotActive,
                                           QStringLiteral("session-not-found"),
                                           {{QStringLiteral("sessionPath"), sessionPath}});
}

QVariantMap PortalBackendService::revokeSession(const QString &sessionPath, const QString &reason)
{
    PortalSessionObject *session = m_sessionManager.session(sessionPath);
    if (!session) {
        return PortalResponseSerializer::error(PortalError::SessionNotActive,
                                               QStringLiteral("session-not-found"),
                                               {{QStringLiteral("sessionPath"), sessionPath}});
    }
    if (!session->isRevocable()) {
        return PortalResponseSerializer::error(PortalError::UnsupportedOperation,
                                               QStringLiteral("session-not-revocable"),
                                               {{QStringLiteral("sessionPath"), sessionPath}});
    }

    const QString normalizedReason = reason.trimmed().isEmpty()
                                         ? QStringLiteral("revoked-by-policy")
                                         : reason.trimmed();
    session->revoke(normalizedReason);
    return PortalResponseSerializer::success({
        {QStringLiteral("revoked"), true},
        {QStringLiteral("sessionPath"), sessionPath},
        {QStringLiteral("sessionHandle"), sessionPath},
        {QStringLiteral("state"), QStringLiteral("Revoked")},
        {QStringLiteral("reason"), normalizedReason},
    });
}

QVariantMap PortalBackendService::getSessionMetadata(const QString &sessionPath) const
{
    PortalSessionObject *session = m_sessionManager.session(sessionPath);
    if (!session) {
        return PortalResponseSerializer::error(PortalError::SessionNotActive,
                                               QStringLiteral("session-not-found"),
                                               {{QStringLiteral("sessionPath"), sessionPath}});
    }
    return PortalResponseSerializer::success({
        {QStringLiteral("sessionPath"), sessionPath},
        {QStringLiteral("sessionHandle"), sessionPath},
        {QStringLiteral("state"), session->stateString()},
        {QStringLiteral("active"), session->isActive()},
        {QStringLiteral("metadata"), session->metadata()},
    });
}

} // namespace Slm::PortalAdapter
