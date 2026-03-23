#include "PortalRequestObject.h"

namespace Slm::PortalAdapter {

namespace {

QString stateToString(PortalRequestState state)
{
    switch (state) {
    case PortalRequestState::Pending: return QStringLiteral("Pending");
    case PortalRequestState::AwaitingUser: return QStringLiteral("AwaitingUser");
    case PortalRequestState::Allowed: return QStringLiteral("Allowed");
    case PortalRequestState::Denied: return QStringLiteral("Denied");
    case PortalRequestState::Cancelled: return QStringLiteral("Cancelled");
    case PortalRequestState::Failed: return QStringLiteral("Failed");
    }
    return QStringLiteral("Pending");
}

} // namespace

PortalRequestObject::PortalRequestObject(const QString &requestId,
                                         const QString &objectPath,
                                         const Slm::Permissions::CallerIdentity &caller,
                                         Slm::Permissions::Capability capability,
                                         const Slm::Permissions::AccessContext &context,
                                         QObject *parent)
    : QObject(parent)
    , m_requestId(requestId)
    , m_objectPath(objectPath)
    , m_caller(caller)
    , m_capability(capability)
    , m_context(context)
    , m_createdAt(QDateTime::currentDateTimeUtc())
{
}

QString PortalRequestObject::requestId() const
{
    return m_requestId;
}

QString PortalRequestObject::objectPath() const
{
    return m_objectPath;
}

QString PortalRequestObject::stateString() const
{
    return stateToString(m_state);
}

Slm::Permissions::CallerIdentity PortalRequestObject::callerIdentity() const
{
    return m_caller;
}

Slm::Permissions::Capability PortalRequestObject::capability() const
{
    return m_capability;
}

Slm::Permissions::AccessContext PortalRequestObject::accessContext() const
{
    return m_context;
}

QDateTime PortalRequestObject::createdAt() const
{
    return m_createdAt;
}

QDateTime PortalRequestObject::completedAt() const
{
    return m_completedAt;
}

bool PortalRequestObject::isCompleted() const
{
    return m_responded;
}

void PortalRequestObject::setAwaitingUser()
{
    if (m_responded) {
        return;
    }
    updateState(PortalRequestState::AwaitingUser);
}

void PortalRequestObject::respondSuccess(const QVariantMap &resultMap)
{
    finish(0, resultMap, PortalRequestState::Allowed);
}

void PortalRequestObject::respondDenied(const QString &reason)
{
    QVariantMap payload;
    payload.insert(QStringLiteral("ok"), false);
    payload.insert(QStringLiteral("reason"), reason);
    payload.insert(QStringLiteral("error"), QStringLiteral("PermissionDenied"));
    finish(1, payload, PortalRequestState::Denied);
}

void PortalRequestObject::respondCancelled(const QString &reason)
{
    QVariantMap payload;
    payload.insert(QStringLiteral("ok"), false);
    payload.insert(QStringLiteral("reason"), reason);
    payload.insert(QStringLiteral("error"), QStringLiteral("CancelledByUser"));
    finish(2, payload, PortalRequestState::Cancelled);
}

void PortalRequestObject::respondFailed(const QString &reason)
{
    QVariantMap payload;
    payload.insert(QStringLiteral("ok"), false);
    payload.insert(QStringLiteral("reason"), reason);
    payload.insert(QStringLiteral("error"), QStringLiteral("Failed"));
    finish(3, payload, PortalRequestState::Failed);
}

void PortalRequestObject::Close()
{
    if (!m_responded) {
        respondCancelled(QStringLiteral("request-closed"));
    }
}

void PortalRequestObject::finish(uint responseCode,
                                 const QVariantMap &results,
                                 PortalRequestState finalState)
{
    if (m_responded) {
        return;
    }
    m_responded = true;
    m_completedAt = QDateTime::currentDateTimeUtc();
    updateState(finalState);
    emit Response(responseCode, results);
    emit finished(m_objectPath);
}

void PortalRequestObject::updateState(PortalRequestState state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    emit stateChanged();
}

} // namespace Slm::PortalAdapter

