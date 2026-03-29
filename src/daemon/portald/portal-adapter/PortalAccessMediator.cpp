#include "PortalAccessMediator.h"

#include "PortalResponseSerializer.h"

#include "../../../core/permissions/AuditLogger.h"
#include "../../../core/permissions/CallerIdentity.h"
#include "../../../core/permissions/PermissionBroker.h"
#include "../../../core/permissions/TrustResolver.h"

#include <QDateTime>
#include <QCryptographicHash>

namespace Slm::PortalAdapter {
namespace {

bool isSessionCapabilityCompatible(Slm::Permissions::Capability sessionCapability,
                                   Slm::Permissions::Capability operationCapability)
{
    using Slm::Permissions::Capability;
    if (sessionCapability == operationCapability) {
        return true;
    }

    // Screencast sessions may execute screencast operations only.
    if (sessionCapability == Capability::ScreencastCreateSession) {
        return operationCapability == Capability::ScreencastSelectSources
               || operationCapability == Capability::ScreencastStart
               || operationCapability == Capability::ScreencastStop;
    }

    // Global shortcuts sessions may execute shortcut operations only.
    if (sessionCapability == Capability::GlobalShortcutsCreateSession) {
        return operationCapability == Capability::GlobalShortcutsRegister
               || operationCapability == Capability::GlobalShortcutsList
               || operationCapability == Capability::GlobalShortcutsActivate
               || operationCapability == Capability::GlobalShortcutsDeactivate;
    }
    if (sessionCapability == Capability::InputCaptureCreateSession) {
        return operationCapability == Capability::InputCaptureSetPointerBarriers
               || operationCapability == Capability::InputCaptureEnable
               || operationCapability == Capability::InputCaptureDisable
               || operationCapability == Capability::InputCaptureRelease;
    }

    return false;
}

QVariantMap normalizeScreencastSelection(const QVariantMap &parameters)
{
    QVariantMap selected;
    const QVariantList sources = parameters.value(QStringLiteral("sources")).toList();
    if (!sources.isEmpty()) {
        selected.insert(QStringLiteral("sources"), sources);
    }
    const QVariantList types = parameters.value(QStringLiteral("types")).toList();
    if (!types.isEmpty()) {
        selected.insert(QStringLiteral("types"), types);
    }
    if (parameters.contains(QStringLiteral("multiple"))) {
        selected.insert(QStringLiteral("multiple"),
                        parameters.value(QStringLiteral("multiple")).toBool());
    }
    if (selected.isEmpty()) {
        selected.insert(QStringLiteral("sources"), QVariantList{QStringLiteral("monitor")});
        selected.insert(QStringLiteral("multiple"), false);
    }
    return selected;
}

QVariantMap makeSyntheticStream(const QString &sessionPath, const QVariantMap &selection)
{
    const QByteArray digest =
        QCryptographicHash::hash(sessionPath.toUtf8(), QCryptographicHash::Sha1);
    quint32 streamId = 0;
    for (int i = 0; i < 4 && i < digest.size(); ++i) {
        streamId = (streamId << 8) | static_cast<unsigned char>(digest.at(i));
    }
    if (streamId == 0u) {
        streamId = 1u;
    }
    const quint32 nodeId = (streamId % 65535u) + 1u;

    QString sourceType = QStringLiteral("monitor");
    const QVariantList sourceList = selection.value(QStringLiteral("sources")).toList();
    if (!sourceList.isEmpty()) {
        sourceType = sourceList.constFirst().toString().trimmed();
        if (sourceType.isEmpty()) {
            sourceType = QStringLiteral("monitor");
        }
    }

    return {
        {QStringLiteral("stream_id"), static_cast<uint>(streamId)},
        {QStringLiteral("node_id"), static_cast<uint>(nodeId)},
        {QStringLiteral("source_type"), sourceType},
        {QStringLiteral("cursor_mode"), QStringLiteral("embedded")},
        {QStringLiteral("synthetic"), true},
    };
}

QVariantList normalizeProvidedStreams(const QVariantMap &parameters)
{
    QVariantList rawStreams = parameters.value(QStringLiteral("streams")).toList();
    if (rawStreams.isEmpty()) {
        const QVariantMap stream{
            {QStringLiteral("stream_id"), parameters.value(QStringLiteral("stream_id"))},
            {QStringLiteral("node_id"),
             parameters.value(QStringLiteral("node_id"),
                              parameters.value(QStringLiteral("pipewire_node_id")))},
            {QStringLiteral("source_type"), parameters.value(QStringLiteral("source_type"))},
            {QStringLiteral("cursor_mode"), parameters.value(QStringLiteral("cursor_mode"))},
        };
        bool any = false;
        QVariantMap compact;
        for (auto it = stream.constBegin(); it != stream.constEnd(); ++it) {
            if (it.value().isValid() && !it.value().toString().trimmed().isEmpty()) {
                compact.insert(it.key(), it.value());
                any = true;
            }
        }
        if (any) {
            rawStreams = QVariantList{compact};
        }
    }

    QVariantList out;
    for (const QVariant &v : rawStreams) {
        const QVariantMap m = v.toMap();
        if (m.isEmpty()) {
            continue;
        }
        QVariantMap normalized = m;
        if (!normalized.contains(QStringLiteral("node_id"))
            && normalized.contains(QStringLiteral("pipewire_node_id"))) {
            normalized.insert(QStringLiteral("node_id"),
                              normalized.value(QStringLiteral("pipewire_node_id")));
        }
        if (!normalized.contains(QStringLiteral("source_type"))) {
            normalized.insert(QStringLiteral("source_type"), QStringLiteral("monitor"));
        }
        if (!normalized.contains(QStringLiteral("cursor_mode"))) {
            normalized.insert(QStringLiteral("cursor_mode"), QStringLiteral("embedded"));
        }
        out.push_back(normalized);
    }
    return out;
}

bool persistenceAllowedForConsent(const PortalMethodSpec &spec,
                                  const Slm::Permissions::PolicyDecision &policy)
{
    return spec.persistenceAllowed && policy.persistentEligible;
}

} // namespace

PortalAccessMediator::PortalAccessMediator(QObject *parent)
    : QObject(parent)
{
}

void PortalAccessMediator::setTrustResolver(Slm::Permissions::TrustResolver *resolver)
{
    m_trustResolver = resolver;
}

void PortalAccessMediator::setPermissionBroker(Slm::Permissions::PermissionBroker *broker)
{
    m_permissionBroker = broker;
}

void PortalAccessMediator::setAuditLogger(Slm::Permissions::AuditLogger *auditLogger)
{
    m_auditLogger = auditLogger;
}

void PortalAccessMediator::setPermissionStoreAdapter(PortalPermissionStoreAdapter *storeAdapter)
{
    m_storeAdapter = storeAdapter;
}

void PortalAccessMediator::setCapabilityMapper(PortalCapabilityMapper *mapper)
{
    m_mapper = mapper;
}

void PortalAccessMediator::setDialogBridge(PortalDialogBridge *dialogBridge)
{
    m_dialogBridge = dialogBridge;
}

void PortalAccessMediator::setRequestManager(PortalRequestManager *requestManager)
{
    m_requestManager = requestManager;
}

void PortalAccessMediator::setSessionManager(PortalSessionManager *sessionManager)
{
    m_sessionManager = sessionManager;
}

QVariantMap PortalAccessMediator::handlePortalRequest(const QString &portalMethod,
                                                      const QDBusMessage &message,
                                                      const QVariantMap &parameters)
{
    if (!m_mapper || !m_trustResolver || !m_permissionBroker || !m_requestManager) {
        return PortalResponseSerializer::error(PortalError::BackendUnavailable,
                                               QStringLiteral("mediator-not-initialized"));
    }
    const PortalMethodSpec spec = m_mapper->mapMethod(portalMethod);
    if (!spec.valid) {
        return PortalResponseSerializer::error(PortalError::CapabilityNotMapped,
                                               QStringLiteral("portal-method-not-mapped"),
                                               {{QStringLiteral("method"), portalMethod}});
    }
    if (spec.requestKind != PortalRequestKind::OneShot) {
        return PortalResponseSerializer::error(PortalError::InvalidRequest,
                                               QStringLiteral("method-requires-session"),
                                               {{QStringLiteral("method"), portalMethod}});
    }

    Slm::Permissions::CallerIdentity caller = Slm::Permissions::resolveCallerIdentityFromDbus(message);
    caller = m_trustResolver->resolveTrust(caller);
    const Slm::Permissions::AccessContext context = buildAccessContext(spec, caller, parameters);
    const Slm::Permissions::PolicyDecision policy = m_permissionBroker->requestAccess(caller, context);

    PortalRequestObject *request = nullptr;
    const QString requestPath =
        m_requestManager->createRequest(caller, spec.capability, context, &request);
    if (!request) {
        return PortalResponseSerializer::error(PortalError::BackendUnavailable,
                                               QStringLiteral("request-creation-failed"));
    }

    if (policy.type == Slm::Permissions::DecisionType::Allow
        || policy.type == Slm::Permissions::DecisionType::AllowOnce
        || policy.type == Slm::Permissions::DecisionType::AllowAlways) {
        request->respondSuccess(PortalResponseSerializer::success(
            {{QStringLiteral("requestPath"), requestPath},
             {QStringLiteral("method"), portalMethod},
             {QStringLiteral("capability"), Slm::Permissions::capabilityToString(spec.capability)}}));
        return successWithHandle(requestPath);
    }

    if (policy.type == Slm::Permissions::DecisionType::Deny
        || policy.type == Slm::Permissions::DecisionType::DenyAlways) {
        request->respondDenied(policy.reason.isEmpty() ? QStringLiteral("denied-by-policy") : policy.reason);
        return successWithHandle(requestPath, {{QStringLiteral("pending"), false}});
    }

    request->setAwaitingUser();
    if (!m_dialogBridge) {
        request->respondDenied(QStringLiteral("missing-consent-ui"));
        return successWithHandle(requestPath, {{QStringLiteral("pending"), false}});
    }

    const bool persistentEligible = persistenceAllowedForConsent(spec, policy);
    m_dialogBridge->requestConsent(
        requestPath,
        caller,
        spec.capability,
        context,
        persistentEligible,
        [this, requestPath, request, spec, caller, context, persistentEligible](const ConsentResult &consent) {
            if (!request || request->isCompleted()) {
                return;
            }
            const Slm::Permissions::DecisionType mapped = decisionFromConsent(consent.decision);
            const bool shouldPersist = persistentEligible
                                       && (consent.persist
                                           || consent.decision == UserDecision::AllowAlways
                                           || consent.decision == UserDecision::DenyAlways);
            if (shouldPersist && m_storeAdapter && !caller.appId.isEmpty()) {
                m_storeAdapter->saveDecision(caller.appId,
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
                request->respondSuccess(PortalResponseSerializer::success(
                    {{QStringLiteral("requestPath"), requestPath},
                     {QStringLiteral("capability"), Slm::Permissions::capabilityToString(spec.capability)},
                     {QStringLiteral("reason"), consent.reason}}));
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

    return successWithHandle(requestPath, {{QStringLiteral("pending"), true}});
}

QVariantMap PortalAccessMediator::handlePortalDirect(const QString &portalMethod,
                                                     const QDBusMessage &message,
                                                     const QVariantMap &parameters)
{
    if (!m_mapper || !m_trustResolver || !m_permissionBroker) {
        return PortalResponseSerializer::error(PortalError::BackendUnavailable,
                                               QStringLiteral("direct-mediator-not-initialized"));
    }
    const PortalMethodSpec spec = m_mapper->mapMethod(portalMethod);
    if (!spec.valid) {
        return PortalResponseSerializer::error(PortalError::CapabilityNotMapped,
                                               QStringLiteral("portal-method-not-mapped"),
                                               {{QStringLiteral("method"), portalMethod}});
    }
    if (spec.requestKind != PortalRequestKind::OneShot || !spec.directResponse) {
        return PortalResponseSerializer::error(PortalError::InvalidRequest,
                                               QStringLiteral("method-is-not-direct"),
                                               {{QStringLiteral("method"), portalMethod}});
    }

    Slm::Permissions::CallerIdentity caller = Slm::Permissions::resolveCallerIdentityFromDbus(message);
    caller = m_trustResolver->resolveTrust(caller);
    const Slm::Permissions::AccessContext context = buildAccessContext(spec, caller, parameters);
    const Slm::Permissions::PolicyDecision policy = m_permissionBroker->requestAccess(caller, context);

    if (policy.type == Slm::Permissions::DecisionType::Allow
        || policy.type == Slm::Permissions::DecisionType::AllowOnce
        || policy.type == Slm::Permissions::DecisionType::AllowAlways) {
        return PortalResponseSerializer::success({
            {QStringLiteral("authorized"), true},
            {QStringLiteral("capability"), Slm::Permissions::capabilityToString(spec.capability)},
            {QStringLiteral("reason"), policy.reason},
        });
    }

    if (policy.type == Slm::Permissions::DecisionType::AskUser) {
        // Direct query endpoints are summary-only and synchronous.
        // Escalated mediation must use explicit request-based methods.
        return PortalResponseSerializer::error(PortalError::PermissionDenied,
                                               QStringLiteral("mediation-required-use-request-flow"),
                                               {{QStringLiteral("capability"),
                                                 Slm::Permissions::capabilityToString(spec.capability)}});
    }

    return PortalResponseSerializer::denied(policy.reason.isEmpty() ? QStringLiteral("denied-by-policy")
                                                                    : policy.reason);
}

QVariantMap PortalAccessMediator::handlePortalSessionRequest(const QString &portalMethod,
                                                             const QDBusMessage &message,
                                                             const QVariantMap &parameters)
{
    if (!m_mapper || !m_trustResolver || !m_permissionBroker || !m_requestManager || !m_sessionManager) {
        return PortalResponseSerializer::error(PortalError::BackendUnavailable,
                                               QStringLiteral("session-mediator-not-initialized"));
    }
    const PortalMethodSpec spec = m_mapper->mapMethod(portalMethod);
    if (!spec.valid) {
        return PortalResponseSerializer::error(PortalError::CapabilityNotMapped,
                                               QStringLiteral("portal-method-not-mapped"),
                                               {{QStringLiteral("method"), portalMethod}});
    }
    if (spec.requestKind != PortalRequestKind::Session) {
        return PortalResponseSerializer::error(PortalError::InvalidRequest,
                                               QStringLiteral("method-is-not-session-based"),
                                               {{QStringLiteral("method"), portalMethod}});
    }

    Slm::Permissions::CallerIdentity caller = Slm::Permissions::resolveCallerIdentityFromDbus(message);
    caller = m_trustResolver->resolveTrust(caller);
    const Slm::Permissions::AccessContext context = buildAccessContext(spec, caller, parameters);
    const Slm::Permissions::PolicyDecision policy = m_permissionBroker->requestAccess(caller, context);

    PortalRequestObject *request = nullptr;
    const QString requestPath =
        m_requestManager->createRequest(caller, spec.capability, context, &request);
    if (!request) {
        return PortalResponseSerializer::error(PortalError::BackendUnavailable,
                                               QStringLiteral("request-creation-failed"));
    }

    auto createActiveSession = [this, caller, spec, requestPath, parameters]() -> QVariantMap {
        PortalSessionObject *sessionObj = nullptr;
        QString requestedPath = parameters.value(QStringLiteral("sessionHandle")).toString().trimmed();
        if (requestedPath.isEmpty()) {
            requestedPath = parameters.value(QStringLiteral("session_handle")).toString().trimmed();
        }
        if (requestedPath.isEmpty()) {
            requestedPath = parameters.value(QStringLiteral("sessionPath")).toString().trimmed();
        }
        const QString sessionPath =
            m_sessionManager->createSession(caller, spec.capability, spec.revocableSession, requestedPath, &sessionObj);
        if (!sessionObj) {
            return PortalResponseSerializer::error(PortalError::BackendUnavailable,
                                                   QStringLiteral("session-creation-failed"));
        }
        sessionObj->activate();
        return PortalResponseSerializer::success({
            {QStringLiteral("requestPath"), requestPath},
            {QStringLiteral("requestHandle"), requestPath},
            {QStringLiteral("sessionPath"), sessionPath},
            {QStringLiteral("sessionHandle"), sessionPath},
            {QStringLiteral("active"), true},
        });
    };

    if (policy.type == Slm::Permissions::DecisionType::Allow
        || policy.type == Slm::Permissions::DecisionType::AllowOnce
        || policy.type == Slm::Permissions::DecisionType::AllowAlways) {
        const QVariantMap payload = createActiveSession();
        request->respondSuccess(payload);
        return successWithHandle(requestPath, payload);
    }

    if (policy.type == Slm::Permissions::DecisionType::Deny
        || policy.type == Slm::Permissions::DecisionType::DenyAlways) {
        request->respondDenied(policy.reason.isEmpty() ? QStringLiteral("denied-by-policy") : policy.reason);
        return successWithHandle(requestPath, {{QStringLiteral("pending"), false}});
    }

    request->setAwaitingUser();
    if (!m_dialogBridge) {
        request->respondDenied(QStringLiteral("missing-consent-ui"));
        return successWithHandle(requestPath, {{QStringLiteral("pending"), false}});
    }

    const bool persistentEligible = persistenceAllowedForConsent(spec, policy);
    m_dialogBridge->requestConsent(
        requestPath,
        caller,
        spec.capability,
        context,
        persistentEligible,
        [this, request, requestPath, spec, caller, context, createActiveSession, persistentEligible](const ConsentResult &consent) mutable {
            if (!request || request->isCompleted()) {
                return;
            }
            const Slm::Permissions::DecisionType mapped = decisionFromConsent(consent.decision);
            const bool shouldPersist = persistentEligible
                                       && (consent.persist
                                           || consent.decision == UserDecision::AllowAlways
                                           || consent.decision == UserDecision::DenyAlways);
            if (shouldPersist && m_storeAdapter && !caller.appId.isEmpty()) {
                m_storeAdapter->saveDecision(caller.appId,
                                             spec.capability,
                                             context.resourceType,
                                             context.resourceId,
                                             mapped,
                                             consent.scope.isEmpty() ? QStringLiteral("persistent")
                                                                     : consent.scope);
            }
            if (consent.decision == UserDecision::AllowOnce
                || consent.decision == UserDecision::AllowAlways) {
                request->respondSuccess(createActiveSession());
            } else if (consent.decision == UserDecision::Cancelled) {
                request->respondCancelled(consent.reason.isEmpty() ? QStringLiteral("cancelled-by-user")
                                                                   : consent.reason);
            } else {
                request->respondDenied(consent.reason.isEmpty() ? QStringLiteral("denied-by-user")
                                                                : consent.reason);
            }
        });

    return successWithHandle(requestPath, {{QStringLiteral("pending"), true}});
}

QVariantMap PortalAccessMediator::handlePortalSessionOperation(const QString &portalMethod,
                                                               const QDBusMessage &message,
                                                               const QString &sessionPath,
                                                               const QVariantMap &parameters,
                                                               bool requireActive)
{
    if (!m_mapper || !m_trustResolver || !m_permissionBroker || !m_requestManager || !m_sessionManager) {
        return PortalResponseSerializer::error(PortalError::BackendUnavailable,
                                               QStringLiteral("session-operation-uninitialized"));
    }
    if (sessionPath.trimmed().isEmpty()) {
        return PortalResponseSerializer::error(PortalError::InvalidRequest,
                                               QStringLiteral("missing-session-path"));
    }

    PortalSessionObject *session = m_sessionManager->session(sessionPath);
    if (!session) {
        return PortalResponseSerializer::error(PortalError::SessionNotActive,
                                               QStringLiteral("session-not-found"),
                                               {{QStringLiteral("sessionPath"), sessionPath}});
    }
    if (requireActive && !session->isActive()) {
        return PortalResponseSerializer::error(PortalError::SessionNotActive,
                                               QStringLiteral("session-not-active"),
                                               {{QStringLiteral("sessionPath"), sessionPath},
                                                {QStringLiteral("state"), session->stateString()}});
    }

    Slm::Permissions::CallerIdentity caller = Slm::Permissions::resolveCallerIdentityFromDbus(message);
    caller = m_trustResolver->resolveTrust(caller);
    if (!session->callerIdentity().busName.isEmpty()
        && caller.busName.compare(session->callerIdentity().busName, Qt::CaseInsensitive) != 0) {
        return PortalResponseSerializer::error(PortalError::PermissionDenied,
                                               QStringLiteral("session-owner-mismatch"),
                                               {{QStringLiteral("sessionPath"), sessionPath}});
    }

    const PortalMethodSpec spec = m_mapper->mapMethod(portalMethod);
    if (!spec.valid) {
        return PortalResponseSerializer::error(PortalError::CapabilityNotMapped,
                                               QStringLiteral("portal-method-not-mapped"),
                                               {{QStringLiteral("method"), portalMethod}});
    }
    if (!isSessionCapabilityCompatible(session->capability(), spec.capability)) {
        return PortalResponseSerializer::error(PortalError::InvalidRequest,
                                               QStringLiteral("session-capability-mismatch"),
                                                {{QStringLiteral("sessionPath"), sessionPath},
                                                {QStringLiteral("sessionCapability"),
                                                 Slm::Permissions::capabilityToString(session->capability())},
                                                {QStringLiteral("requestedCapability"),
                                                 Slm::Permissions::capabilityToString(spec.capability)}});
    }

    QVariantMap localParams = parameters;
    localParams.insert(QStringLiteral("resourceType"), QStringLiteral("portal-session"));
    localParams.insert(QStringLiteral("resourceId"), sessionPath);
    const Slm::Permissions::AccessContext context = buildAccessContext(spec, caller, localParams);
    const Slm::Permissions::PolicyDecision policy = m_permissionBroker->requestAccess(caller, context);

    PortalRequestObject *request = nullptr;
    const QString requestPath =
        m_requestManager->createRequest(caller, spec.capability, context, &request);
    if (!request) {
        return PortalResponseSerializer::error(PortalError::BackendUnavailable,
                                               QStringLiteral("request-creation-failed"));
    }

    auto successPayload = [requestPath, sessionPath, portalMethod, session, parameters]() {
        QVariantMap results;
        results.insert(QStringLiteral("requestPath"), requestPath);
        results.insert(QStringLiteral("requestHandle"), requestPath);
        results.insert(QStringLiteral("sessionPath"), sessionPath);
        results.insert(QStringLiteral("sessionHandle"), sessionPath);
        results.insert(QStringLiteral("session_handle"), sessionPath);
        results.insert(QStringLiteral("method"), portalMethod);
        results.insert(QStringLiteral("active"), true);

        if (portalMethod.compare(QStringLiteral("ScreencastSelectSources"), Qt::CaseInsensitive) == 0) {
            const QVariantMap selected = normalizeScreencastSelection(parameters);
            QVariantMap metadata = session->metadata();
            metadata.insert(QStringLiteral("screencast.selected_sources"), selected);
            metadata.insert(QStringLiteral("screencast.sources_selected"), true);
            session->setMetadata(metadata);
            results.insert(QStringLiteral("sources_selected"), true);
            results.insert(QStringLiteral("selected_sources"), selected);
        } else if (portalMethod.compare(QStringLiteral("ScreencastStart"), Qt::CaseInsensitive) == 0) {
            QVariantMap metadata = session->metadata();
            QVariantMap selected =
                metadata.value(QStringLiteral("screencast.selected_sources")).toMap();
            if (selected.isEmpty()) {
                selected = normalizeScreencastSelection(parameters);
            }
            QVariantList streams = normalizeProvidedStreams(parameters);
            if (streams.isEmpty()) {
                streams = QVariantList{makeSyntheticStream(sessionPath, selected)};
            }
            metadata.insert(QStringLiteral("screencast.streams"), streams);
            metadata.insert(QStringLiteral("screencast.stream_count"), streams.size());
            session->setMetadata(metadata);
            results.insert(QStringLiteral("streams"), streams);
            results.insert(QStringLiteral("sources_selected"),
                           metadata.value(QStringLiteral("screencast.sources_selected"), true).toBool());
            results.insert(QStringLiteral("selected_sources"), selected);
        } else if (portalMethod.compare(QStringLiteral("ScreencastStop"), Qt::CaseInsensitive) == 0) {
            results.insert(QStringLiteral("stopped"), true);
            const bool closeSession = parameters.value(QStringLiteral("closeSession"), true).toBool();
            results.insert(QStringLiteral("session_closed"), closeSession);
            if (closeSession) {
                session->Close();
            }
        } else if (portalMethod.compare(QStringLiteral("InputCaptureSetPointerBarriers"), Qt::CaseInsensitive) == 0) {
            QVariantMap metadata = session->metadata();
            const QVariantList barriers = parameters.value(QStringLiteral("barriers")).toList();
            metadata.insert(QStringLiteral("inputcapture.pointer_barriers"), barriers);
            metadata.insert(QStringLiteral("inputcapture.barrier_count"), barriers.size());
            session->setMetadata(metadata);
            results.insert(QStringLiteral("pointer_barriers"), barriers);
            results.insert(QStringLiteral("barriers_set"), true);
        } else if (portalMethod.compare(QStringLiteral("InputCaptureEnable"), Qt::CaseInsensitive) == 0) {
            QVariantMap metadata = session->metadata();
            metadata.insert(QStringLiteral("inputcapture.enabled"), true);
            session->setMetadata(metadata);
            results.insert(QStringLiteral("enabled"), true);
        } else if (portalMethod.compare(QStringLiteral("InputCaptureDisable"), Qt::CaseInsensitive) == 0) {
            QVariantMap metadata = session->metadata();
            metadata.insert(QStringLiteral("inputcapture.enabled"), false);
            session->setMetadata(metadata);
            results.insert(QStringLiteral("disabled"), true);
        } else if (portalMethod.compare(QStringLiteral("InputCaptureRelease"), Qt::CaseInsensitive) == 0) {
            QVariantMap metadata = session->metadata();
            metadata.insert(QStringLiteral("inputcapture.enabled"), false);
            metadata.insert(QStringLiteral("inputcapture.released"), true);
            session->setMetadata(metadata);
            const bool closeSession = parameters.value(QStringLiteral("closeSession"), true).toBool();
            results.insert(QStringLiteral("released"), true);
            results.insert(QStringLiteral("session_closed"), closeSession);
            if (closeSession) {
                session->Close();
            }
        }

        return PortalResponseSerializer::success({
            {QStringLiteral("requestPath"), requestPath},
            {QStringLiteral("requestHandle"), requestPath},
            {QStringLiteral("sessionPath"), sessionPath},
            {QStringLiteral("sessionHandle"), sessionPath},
            {QStringLiteral("method"), portalMethod},
            {QStringLiteral("active"), true},
            {QStringLiteral("results"), results},
        });
    };

    if (policy.type == Slm::Permissions::DecisionType::Allow
        || policy.type == Slm::Permissions::DecisionType::AllowOnce
        || policy.type == Slm::Permissions::DecisionType::AllowAlways) {
        request->respondSuccess(successPayload());
        return successWithHandle(requestPath, successPayload());
    }
    if (policy.type == Slm::Permissions::DecisionType::Deny
        || policy.type == Slm::Permissions::DecisionType::DenyAlways) {
        request->respondDenied(policy.reason.isEmpty() ? QStringLiteral("denied-by-policy") : policy.reason);
        return successWithHandle(requestPath, {{QStringLiteral("pending"), false}});
    }

    request->setAwaitingUser();
    if (!m_dialogBridge) {
        request->respondDenied(QStringLiteral("missing-consent-ui"));
        return successWithHandle(requestPath, {{QStringLiteral("pending"), false}});
    }
    const bool persistentEligible = persistenceAllowedForConsent(spec, policy);
    m_dialogBridge->requestConsent(
        requestPath,
        caller,
        spec.capability,
        context,
        persistentEligible,
        [this, request, requestPath, spec, caller, context, sessionPath, successPayload, persistentEligible](const ConsentResult &consent) {
            if (!request || request->isCompleted()) {
                return;
            }
            const Slm::Permissions::DecisionType mapped = decisionFromConsent(consent.decision);
            const bool shouldPersist = persistentEligible
                                       && (consent.persist
                                           || consent.decision == UserDecision::AllowAlways
                                           || consent.decision == UserDecision::DenyAlways);
            if (shouldPersist && m_storeAdapter && !caller.appId.isEmpty()) {
                m_storeAdapter->saveDecision(caller.appId,
                                             spec.capability,
                                             context.resourceType,
                                             sessionPath,
                                             mapped,
                                             consent.scope.isEmpty() ? QStringLiteral("persistent")
                                                                     : consent.scope);
            }
            if (consent.decision == UserDecision::AllowOnce
                || consent.decision == UserDecision::AllowAlways) {
                request->respondSuccess(successPayload());
            } else if (consent.decision == UserDecision::Cancelled) {
                request->respondCancelled(consent.reason.isEmpty() ? QStringLiteral("cancelled-by-user")
                                                                   : consent.reason);
            } else {
                request->respondDenied(consent.reason.isEmpty() ? QStringLiteral("denied-by-user")
                                                                : consent.reason);
            }
        });

    return successWithHandle(requestPath, {{QStringLiteral("pending"), true}});
}

Slm::Permissions::AccessContext PortalAccessMediator::buildAccessContext(
    const PortalMethodSpec &spec,
    const Slm::Permissions::CallerIdentity &caller,
    const QVariantMap &parameters) const
{
    Slm::Permissions::AccessContext context;
    context.capability = spec.capability;
    context.resourceType = parameters.value(QStringLiteral("resourceType")).toString().trimmed();
    context.resourceId = parameters.value(QStringLiteral("resourceId")).toString().trimmed();
    context.initiatedByUserGesture = parameters.value(QStringLiteral("initiatedByUserGesture")).toBool();
    const bool callerCanAssertOfficialUi =
        caller.isOfficialComponent
        || caller.trustLevel == Slm::Permissions::TrustLevel::PrivilegedDesktopService;
    context.initiatedFromOfficialUI = caller.isOfficialComponent
                                      || (callerCanAssertOfficialUi
                                          && parameters.value(QStringLiteral("initiatedFromOfficialUI")).toBool());
    context.sensitivityLevel = spec.sensitivity;
    context.timestamp = QDateTime::currentMSecsSinceEpoch();
    context.sessionOnly = (spec.requestKind == PortalRequestKind::Session);
    return context;
}

QVariantMap PortalAccessMediator::successWithHandle(const QString &requestPath,
                                                    const QVariantMap &extra) const
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

Slm::Permissions::DecisionType PortalAccessMediator::decisionFromConsent(UserDecision decision)
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

} // namespace Slm::PortalAdapter
