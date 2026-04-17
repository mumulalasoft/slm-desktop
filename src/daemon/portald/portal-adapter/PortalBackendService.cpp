#include "PortalBackendService.h"

#include "PortalResponseSerializer.h"

#include "../../../core/permissions/AccessContext.h"
#include "../../../core/permissions/CallerIdentity.h"
#include "../../../core/permissions/Capability.h"
#include "../../../core/permissions/PermissionBroker.h"
#include "../../../core/permissions/PolicyDecision.h"
#include "../../../core/permissions/TrustResolver.h"

#include <QDateTime>
#include <QPointer>
#include <QTimer>

namespace Slm::PortalAdapter {
namespace {

bool secretMethodAllowsPersistentGrant(const QString &portalMethod)
{
    return portalMethod == QStringLiteral("StoreSecret")
           || portalMethod == QStringLiteral("GetSecret")
           || portalMethod == QStringLiteral("DeleteSecret");
}

bool isSecretCapabilityString(const QString &value)
{
    return value.trimmed().startsWith(QStringLiteral("Secret."), Qt::CaseInsensitive);
}

bool isTrustedSecretAdminCaller(const Slm::Permissions::CallerIdentity &caller)
{
    return caller.isOfficialComponent
           || caller.trustLevel == Slm::Permissions::TrustLevel::CoreDesktopComponent
           || caller.trustLevel == Slm::Permissions::TrustLevel::PrivilegedDesktopService;
}

}

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
    m_trustResolver = trustResolver;
    m_permissionBroker = permissionBroker;
    m_auditLogger = auditLogger;
    m_permissionStore = permissionStore;
    m_storeAdapter.setStore(permissionStore);
    m_accessMediator.setTrustResolver(trustResolver);
    m_accessMediator.setPermissionBroker(permissionBroker);
    m_accessMediator.setAuditLogger(auditLogger);
}

QVariantMap PortalBackendService::handleRequest(const QString &portalMethod,
                                                const QDBusMessage &message,
                                                const QVariantMap &parameters)
{
    if (isSecretRequestMethod(portalMethod)) {
        return handleSecretRequest(portalMethod, message, parameters);
    }
    return m_accessMediator.handlePortalRequest(portalMethod, message, parameters);
}

QVariantMap PortalBackendService::handleDirect(const QString &portalMethod,
                                               const QDBusMessage &message,
                                               const QVariantMap &parameters)
{
    if (isSecretDirectMethod(portalMethod)) {
        return handleSecretDirect(portalMethod, message, parameters);
    }
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

QVariantMap PortalBackendService::handleSecretRequest(const QString &portalMethod,
                                                      const QDBusMessage &message,
                                                      const QVariantMap &parameters)
{
    if (!m_trustResolver || !m_permissionBroker) {
        return PortalResponseSerializer::error(PortalError::BackendUnavailable,
                                               QStringLiteral("secret-policy-not-initialized"));
    }

    const PortalMethodSpec spec = m_capabilityMapper.mapMethod(portalMethod);
    if (!spec.valid) {
        return PortalResponseSerializer::error(PortalError::CapabilityNotMapped,
                                               QStringLiteral("portal-method-not-mapped"),
                                               {{QStringLiteral("method"), portalMethod}});
    }

    Slm::Permissions::CallerIdentity caller = Slm::Permissions::resolveCallerIdentityFromDbus(message);
    caller = m_trustResolver->resolveTrust(caller);

    Slm::Permissions::AccessContext context;
    context.capability = spec.capability;
    context.resourceType = QStringLiteral("secret");
    context.resourceId = parameters.value(QStringLiteral("key")).toString().trimmed();
    context.initiatedByUserGesture = parameters.value(QStringLiteral("initiatedByUserGesture")).toBool();
    const bool callerCanAssertOfficialUi =
        caller.isOfficialComponent
        || caller.trustLevel == Slm::Permissions::TrustLevel::PrivilegedDesktopService;
    context.initiatedFromOfficialUI = caller.isOfficialComponent
                                      || (callerCanAssertOfficialUi
                                          && parameters.value(QStringLiteral("initiatedFromOfficialUI")).toBool());
    context.sensitivityLevel = spec.sensitivity;
    context.timestamp = QDateTime::currentMSecsSinceEpoch();
    context.sessionOnly = false;

    const Slm::Permissions::PolicyDecision policy = m_permissionBroker->requestAccess(caller, context);

    PortalRequestObject *request = nullptr;
    const QString requestPath = m_requestManager.createRequest(caller, spec.capability, context, &request);
    if (!request) {
        return PortalResponseSerializer::error(PortalError::BackendUnavailable,
                                               QStringLiteral("request-creation-failed"));
    }

    const QString appId = resolvePortalSecretAppId(caller, parameters);
    const QVariantMap requestParameters = parameters;
    auto completeOperation = [this, requestPath, portalMethod, appId, requestParameters](PortalRequestObject *req) {
        if (!req || req->isCompleted()) {
            return;
        }

        QVariantMap response;
        if (portalMethod == QStringLiteral("StoreSecret")) {
            response = m_secretBridge.StoreSecret(appId,
                                                  requestParameters,
                                                  requestParameters.value(QStringLiteral("secret")).toByteArray());
        } else if (portalMethod == QStringLiteral("GetSecret")) {
            response = m_secretBridge.GetSecret(appId, requestParameters);
        } else if (portalMethod == QStringLiteral("DeleteSecret")) {
            response = m_secretBridge.DeleteSecret(appId, requestParameters);
        } else if (portalMethod == QStringLiteral("ClearAppSecrets")) {
            response = m_secretBridge.ClearAppSecrets(appId);
        } else {
            req->respondFailed(QStringLiteral("unsupported-secret-method"));
            return;
        }

        if (response.value(QStringLiteral("ok")).toBool()) {
            req->respondSuccess(PortalResponseSerializer::success({
                {QStringLiteral("requestPath"), requestPath},
                {QStringLiteral("requestHandle"), requestPath},
                {QStringLiteral("handle"), requestPath},
                {QStringLiteral("appId"), appId},
                {QStringLiteral("results"), response},
            }));
            return;
        }
        const QString reason = response.value(QStringLiteral("error")).toString().trimmed().isEmpty()
                                   ? QStringLiteral("secret-operation-failed")
                                   : response.value(QStringLiteral("error")).toString().trimmed();
        req->respondFailed(reason);
    };

    if (policy.type == Slm::Permissions::DecisionType::Allow
        || policy.type == Slm::Permissions::DecisionType::AllowOnce
        || policy.type == Slm::Permissions::DecisionType::AllowAlways) {
        QPointer<PortalRequestObject> requestPtr(request);
        QTimer::singleShot(0, this, [completeOperation, requestPtr]() { completeOperation(requestPtr.data()); });
        return successWithHandle(requestPath,
                                 {{QStringLiteral("pending"), true},
                                  {QStringLiteral("appId"), appId}});
    }

    if (policy.type == Slm::Permissions::DecisionType::Deny
        || policy.type == Slm::Permissions::DecisionType::DenyAlways) {
        request->respondDenied(policy.reason.isEmpty() ? QStringLiteral("denied-by-policy") : policy.reason);
        return successWithHandle(requestPath,
                                 {{QStringLiteral("pending"), false},
                                  {QStringLiteral("appId"), appId}});
    }

    request->setAwaitingUser();
    const bool persistentEligible = secretMethodAllowsPersistentGrant(portalMethod)
                                    && spec.persistenceAllowed
                                    && policy.persistentEligible;
    m_dialogBridge.requestConsent(
        requestPath,
        caller,
        spec.capability,
        context,
        persistentEligible,
        [this, request, caller, spec, context, completeOperation, persistentEligible](const ConsentResult &consent) {
            if (!request || request->isCompleted()) {
                return;
            }
            const Slm::Permissions::DecisionType mapped = decisionFromConsent(consent.decision);
            const bool shouldPersist = persistentEligible
                                       && (consent.persist
                                           || consent.decision == UserDecision::AllowAlways
                                           || consent.decision == UserDecision::DenyAlways);
            if (shouldPersist && !caller.appId.isEmpty()) {
                m_storeAdapter.saveDecision(caller.appId,
                                            spec.capability,
                                            context.resourceType,
                                            context.resourceId,
                                            mapped,
                                            consent.scope.isEmpty() ? QStringLiteral("persistent")
                                                                    : consent.scope);
            }

            switch (consent.decision) {
            case UserDecision::AllowOnce:
            case UserDecision::AllowAlways:
                completeOperation(request);
                break;
            case UserDecision::Deny:
            case UserDecision::DenyAlways:
                request->respondDenied(consent.reason.isEmpty() ? QStringLiteral("denied-by-user")
                                                                : consent.reason);
                break;
            case UserDecision::Cancelled:
            default:
                request->respondCancelled(consent.reason.isEmpty() ? QStringLiteral("cancelled-by-user")
                                                                   : consent.reason);
                break;
            }
        });

    return successWithHandle(requestPath,
                             {{QStringLiteral("pending"), true},
                              {QStringLiteral("appId"), appId}});
}

QVariantMap PortalBackendService::handleSecretDirect(const QString &portalMethod,
                                                     const QDBusMessage &message,
                                                     const QVariantMap &parameters)
{
    if (!m_trustResolver || !m_permissionBroker) {
        return PortalResponseSerializer::error(PortalError::BackendUnavailable,
                                               QStringLiteral("secret-policy-not-initialized"));
    }

    if (portalMethod == QStringLiteral("ListSecretConsentGrants")
        || portalMethod == QStringLiteral("RevokeSecretConsentGrants")) {
        Slm::Permissions::CallerIdentity caller = Slm::Permissions::resolveCallerIdentityFromDbus(message);
        caller = m_trustResolver->resolveTrust(caller);
        if (!isTrustedSecretAdminCaller(caller)) {
            return PortalResponseSerializer::denied(QStringLiteral("admin-required"));
        }

        const QString appId = parameters.value(QStringLiteral("appId")).toString().trimmed().isEmpty()
                                  ? parameters.value(QStringLiteral("app_id")).toString().trimmed().toLower()
                                  : parameters.value(QStringLiteral("appId")).toString().trimmed().toLower();
        const QVariantList decisions = m_storeAdapter.listDecisions(appId);

        if (portalMethod == QStringLiteral("ListSecretConsentGrants")) {
            QVariantList items;
            items.reserve(decisions.size());
            for (const QVariant &rowVar : decisions) {
                const QVariantMap row = rowVar.toMap();
                if (isSecretCapabilityString(row.value(QStringLiteral("capability")).toString())) {
                    items.push_back(row);
                }
            }
            return PortalResponseSerializer::success(
                {{QStringLiteral("items"), items},
                 {QStringLiteral("count"), items.size()}});
        }

        if (appId.isEmpty()) {
            return PortalResponseSerializer::error(PortalError::InvalidRequest,
                                                   QStringLiteral("app-id-required"));
        }

        int revokeCount = 0;
        int failedCount = 0;
        for (const QVariant &rowVar : decisions) {
            const QVariantMap row = rowVar.toMap();
            const QString capabilityStr = row.value(QStringLiteral("capability")).toString();
            if (!isSecretCapabilityString(capabilityStr)) {
                continue;
            }
            const Slm::Permissions::Capability capability =
                Slm::Permissions::capabilityFromString(capabilityStr);
            if (capability == Slm::Permissions::Capability::Unknown) {
                ++failedCount;
                continue;
            }
            if (m_storeAdapter.removeDecision(appId,
                                              capability,
                                              row.value(QStringLiteral("resourceType")).toString(),
                                              row.value(QStringLiteral("resourceId")).toString())) {
                ++revokeCount;
            } else {
                ++failedCount;
            }
        }

        return PortalResponseSerializer::success(
            {{QStringLiteral("appId"), appId},
             {QStringLiteral("revokeCount"), revokeCount},
             {QStringLiteral("failedCount"), failedCount}});
    }

    const PortalMethodSpec spec = m_capabilityMapper.mapMethod(portalMethod);
    if (!spec.valid || !spec.directResponse) {
        return PortalResponseSerializer::error(PortalError::UnsupportedOperation,
                                               QStringLiteral("unsupported-secret-direct-method"),
                                               {{QStringLiteral("method"), portalMethod}});
    }

    Slm::Permissions::CallerIdentity caller = Slm::Permissions::resolveCallerIdentityFromDbus(message);
    caller = m_trustResolver->resolveTrust(caller);
    Slm::Permissions::AccessContext context;
    context.capability = spec.capability;
    context.resourceType = QStringLiteral("secret");
    context.resourceId = parameters.value(QStringLiteral("key")).toString().trimmed();
    context.initiatedByUserGesture = parameters.value(QStringLiteral("initiatedByUserGesture")).toBool();
    context.sensitivityLevel = spec.sensitivity;
    context.timestamp = QDateTime::currentMSecsSinceEpoch();
    const Slm::Permissions::PolicyDecision policy = m_permissionBroker->requestAccess(caller, context);
    if (!(policy.type == Slm::Permissions::DecisionType::Allow
          || policy.type == Slm::Permissions::DecisionType::AllowOnce
          || policy.type == Slm::Permissions::DecisionType::AllowAlways)) {
        if (policy.type == Slm::Permissions::DecisionType::AskUser) {
            return PortalResponseSerializer::error(
                PortalError::PermissionDenied,
                QStringLiteral("mediation-required-use-request-flow"),
                {{QStringLiteral("capability"), Slm::Permissions::capabilityToString(spec.capability)}});
        }
        return PortalResponseSerializer::denied(policy.reason.isEmpty() ? QStringLiteral("permission-denied")
                                                                        : policy.reason);
    }

    const QString appId = resolvePortalSecretAppId(caller, parameters);
    if (portalMethod == QStringLiteral("DescribeSecret")) {
        return m_secretBridge.DescribeSecret(appId, parameters);
    }
    if (portalMethod == QStringLiteral("ListOwnSecretMetadata")) {
        return m_secretBridge.ListOwnSecretMetadata(appId, parameters);
    }
    if (portalMethod == QStringLiteral("ListSecretAppIds")) {
        return m_secretBridge.ListSecretAppIds(parameters);
    }
    return PortalResponseSerializer::error(PortalError::UnsupportedOperation,
                                           QStringLiteral("unsupported-secret-direct-method"),
                                           {{QStringLiteral("method"), portalMethod}});
}

Slm::Permissions::DecisionType PortalBackendService::decisionFromConsent(UserDecision decision)
{
    using Slm::Permissions::DecisionType;
    switch (decision) {
    case UserDecision::AllowOnce: return DecisionType::AllowOnce;
    case UserDecision::AllowAlways: return DecisionType::AllowAlways;
    case UserDecision::Deny: return DecisionType::Deny;
    case UserDecision::DenyAlways: return DecisionType::DenyAlways;
    case UserDecision::Cancelled:
    default:
        return DecisionType::Deny;
    }
}

bool PortalBackendService::isSecretRequestMethod(const QString &portalMethod)
{
    return portalMethod == QStringLiteral("StoreSecret")
           || portalMethod == QStringLiteral("GetSecret")
           || portalMethod == QStringLiteral("DeleteSecret")
           || portalMethod == QStringLiteral("ClearAppSecrets");
}

bool PortalBackendService::isSecretDirectMethod(const QString &portalMethod)
{
    return portalMethod == QStringLiteral("DescribeSecret")
           || portalMethod == QStringLiteral("ListOwnSecretMetadata")
           || portalMethod == QStringLiteral("ListSecretAppIds")
           || portalMethod == QStringLiteral("ListSecretConsentGrants")
           || portalMethod == QStringLiteral("RevokeSecretConsentGrants");
}

QVariantMap PortalBackendService::successWithHandle(const QString &requestPath,
                                                    const QVariantMap &extra)
{
    QVariantMap out = PortalResponseSerializer::success({
        {QStringLiteral("requestPath"), requestPath},
        {QStringLiteral("requestHandle"), requestPath},
        {QStringLiteral("handle"), requestPath},
    });
    for (auto it = extra.constBegin(); it != extra.constEnd(); ++it) {
        out.insert(it.key(), it.value());
    }
    return out;
}

QString PortalBackendService::resolvePortalSecretAppId(const Slm::Permissions::CallerIdentity &caller,
                                                       const QVariantMap &parameters)
{
    const QString requestedAppId = parameters.value(QStringLiteral("appId")).toString().trimmed().isEmpty()
                                       ? parameters.value(QStringLiteral("app_id")).toString().trimmed()
                                       : parameters.value(QStringLiteral("appId")).toString().trimmed();
    if (!requestedAppId.isEmpty()
        && (caller.isOfficialComponent
            || caller.trustLevel == Slm::Permissions::TrustLevel::PrivilegedDesktopService)) {
        return requestedAppId.toLower();
    }

    QString appId = caller.desktopFileId.trimmed();
    if (appId.isEmpty()) {
        appId = caller.appId.trimmed();
    }
    if (appId.isEmpty()) {
        appId = requestedAppId;
    }
    if (appId.isEmpty()) {
        appId = QStringLiteral("unknown.desktop");
    }
    return appId.toLower();
}

} // namespace Slm::PortalAdapter
