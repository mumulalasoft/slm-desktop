#pragma once

#include "../../../core/permissions/AccessContext.h"
#include "../../../core/permissions/CallerIdentity.h"
#include "../../../core/permissions/Capability.h"

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVariantMap>

namespace Slm::PortalAdapter {

enum class PortalRequestState {
    Pending = 0,
    AwaitingUser,
    Allowed,
    Denied,
    Cancelled,
    Failed
};

class PortalRequestObject : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.portal.Request")
    Q_PROPERTY(QString objectPath READ objectPath CONSTANT)
    Q_PROPERTY(QString requestId READ requestId CONSTANT)
    Q_PROPERTY(QString state READ stateString NOTIFY stateChanged)

public:
    PortalRequestObject(const QString &requestId,
                        const QString &objectPath,
                        const Slm::Permissions::CallerIdentity &caller,
                        Slm::Permissions::Capability capability,
                        const Slm::Permissions::AccessContext &context,
                        QObject *parent = nullptr);

    QString requestId() const;
    QString objectPath() const;
    QString stateString() const;
    Slm::Permissions::CallerIdentity callerIdentity() const;
    Slm::Permissions::Capability capability() const;
    Slm::Permissions::AccessContext accessContext() const;
    QDateTime createdAt() const;
    QDateTime completedAt() const;
    bool isCompleted() const;

    void setAwaitingUser();
    void respondSuccess(const QVariantMap &resultMap);
    void respondDenied(const QString &reason);
    void respondCancelled(const QString &reason = QStringLiteral("cancelled"));
    void respondFailed(const QString &reason);

public slots:
    Q_NOREPLY void Close();

signals:
    void stateChanged();
    void finished(const QString &requestPath);
    void Response(uint response, const QVariantMap &results);

private:
    void finish(uint responseCode,
                const QVariantMap &results,
                PortalRequestState finalState);
    void updateState(PortalRequestState state);

    QString m_requestId;
    QString m_objectPath;
    Slm::Permissions::CallerIdentity m_caller;
    Slm::Permissions::Capability m_capability = Slm::Permissions::Capability::Unknown;
    Slm::Permissions::AccessContext m_context;
    PortalRequestState m_state = PortalRequestState::Pending;
    QDateTime m_createdAt;
    QDateTime m_completedAt;
    bool m_responded = false;
};

} // namespace Slm::PortalAdapter

