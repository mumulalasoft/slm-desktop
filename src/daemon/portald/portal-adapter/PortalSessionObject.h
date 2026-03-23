#pragma once

#include "../../../core/permissions/CallerIdentity.h"
#include "../../../core/permissions/Capability.h"

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVariantMap>

namespace Slm::PortalAdapter {

enum class PortalSessionState {
    Created = 0,
    Active,
    Suspended,
    Revoked,
    Closed,
    Expired
};

class PortalSessionObject : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.portal.Session")
    Q_PROPERTY(QString objectPath READ objectPath CONSTANT)
    Q_PROPERTY(QString sessionId READ sessionId CONSTANT)
    Q_PROPERTY(QString state READ stateString NOTIFY stateChanged)

public:
    PortalSessionObject(const QString &sessionId,
                        const QString &objectPath,
                        const Slm::Permissions::CallerIdentity &caller,
                        Slm::Permissions::Capability capability,
                        bool revocable,
                        QObject *parent = nullptr);

    QString objectPath() const;
    QString sessionId() const;
    QString stateString() const;
    Slm::Permissions::CallerIdentity callerIdentity() const;
    Slm::Permissions::Capability capability() const;
    bool isRevocable() const;
    bool isActive() const;
    QDateTime createdAt() const;
    QDateTime expiresAt() const;
    QVariantMap metadata() const;
    void setMetadata(const QVariantMap &metadata);
    void setExpiresAt(const QDateTime &expiresAt);

    void activate();
    void revoke(const QString &reason);

public slots:
    Q_NOREPLY void Close();

signals:
    void stateChanged();
    void sessionFinished(const QString &sessionPath);
    void Closed(const QVariantMap &details);

private:
    void setState(PortalSessionState state);

    QString m_sessionId;
    QString m_objectPath;
    Slm::Permissions::CallerIdentity m_caller;
    Slm::Permissions::Capability m_capability = Slm::Permissions::Capability::Unknown;
    PortalSessionState m_state = PortalSessionState::Created;
    QDateTime m_createdAt;
    QDateTime m_expiresAt;
    QVariantMap m_metadata;
    bool m_revocable = true;
};

} // namespace Slm::PortalAdapter

