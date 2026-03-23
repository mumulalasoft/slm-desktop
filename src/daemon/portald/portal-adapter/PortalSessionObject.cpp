#include "PortalSessionObject.h"

namespace Slm::PortalAdapter {

namespace {

QString stateToString(PortalSessionState state)
{
    switch (state) {
    case PortalSessionState::Created: return QStringLiteral("Created");
    case PortalSessionState::Active: return QStringLiteral("Active");
    case PortalSessionState::Suspended: return QStringLiteral("Suspended");
    case PortalSessionState::Revoked: return QStringLiteral("Revoked");
    case PortalSessionState::Closed: return QStringLiteral("Closed");
    case PortalSessionState::Expired: return QStringLiteral("Expired");
    }
    return QStringLiteral("Created");
}

} // namespace

PortalSessionObject::PortalSessionObject(const QString &sessionId,
                                         const QString &objectPath,
                                         const Slm::Permissions::CallerIdentity &caller,
                                         Slm::Permissions::Capability capability,
                                         bool revocable,
                                         QObject *parent)
    : QObject(parent)
    , m_sessionId(sessionId)
    , m_objectPath(objectPath)
    , m_caller(caller)
    , m_capability(capability)
    , m_createdAt(QDateTime::currentDateTimeUtc())
    , m_revocable(revocable)
{
}

QString PortalSessionObject::objectPath() const
{
    return m_objectPath;
}

QString PortalSessionObject::sessionId() const
{
    return m_sessionId;
}

QString PortalSessionObject::stateString() const
{
    return stateToString(m_state);
}

Slm::Permissions::CallerIdentity PortalSessionObject::callerIdentity() const
{
    return m_caller;
}

Slm::Permissions::Capability PortalSessionObject::capability() const
{
    return m_capability;
}

bool PortalSessionObject::isRevocable() const
{
    return m_revocable;
}

bool PortalSessionObject::isActive() const
{
    return m_state == PortalSessionState::Active;
}

QDateTime PortalSessionObject::createdAt() const
{
    return m_createdAt;
}

QDateTime PortalSessionObject::expiresAt() const
{
    return m_expiresAt;
}

QVariantMap PortalSessionObject::metadata() const
{
    return m_metadata;
}

void PortalSessionObject::setMetadata(const QVariantMap &metadata)
{
    m_metadata = metadata;
}

void PortalSessionObject::setExpiresAt(const QDateTime &expiresAt)
{
    m_expiresAt = expiresAt;
}

void PortalSessionObject::activate()
{
    setState(PortalSessionState::Active);
}

void PortalSessionObject::revoke(const QString &reason)
{
    if (!m_revocable) {
        return;
    }
    setState(PortalSessionState::Revoked);
    emit Closed({{QStringLiteral("reason"), reason},
                 {QStringLiteral("state"), stateString()},
                 {QStringLiteral("revoked"), true}});
    emit sessionFinished(m_objectPath);
}

void PortalSessionObject::Close()
{
    setState(PortalSessionState::Closed);
    emit Closed({{QStringLiteral("reason"), QStringLiteral("closed")},
                 {QStringLiteral("state"), stateString()}});
    emit sessionFinished(m_objectPath);
}

void PortalSessionObject::setState(PortalSessionState state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    emit stateChanged();
}

} // namespace Slm::PortalAdapter

