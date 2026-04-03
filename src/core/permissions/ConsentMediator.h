#pragma once

#include "Capability.h"
#include "PermissionStore.h"
#include "PolicyDecision.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QVariantMap>

class QTimer;

namespace Slm::Permissions {

// ConsentMediator bridges the gap between AskUser policy decisions and actual
// user consent dialogs for non-portal D-Bus services (clipboard, desktop daemon,
// file actions, etc.).
//
// Usage:
//   1. Service receives PolicyDecision::AskUser from DBusSecurityGuard.
//   2. Service calls mediator.requestConsent(req) → returns requestId.
//   3. mediator emits consentRequested(requestId, payload) → UI shows dialog.
//   4. UI calls mediator.fulfillConsent(requestId, response).
//   5. mediator emits consentResolved(requestId, response, decision).
//   6. AllowAlways / DenyAlways responses are auto-persisted to PermissionStore.
//   7. On timeout (default 30 s) mediator auto-resolves with Timeout (→ Deny).

class ConsentMediator : public QObject
{
    Q_OBJECT
public:
    // User responses, mirroring portal consent decisions.
    enum class UserResponse {
        AllowOnce,
        AllowAlways,
        DenyOnce,
        DenyAlways,
        Timeout,
    };
    Q_ENUM(UserResponse)

    struct ConsentRequest {
        QString     requestId;      // auto-filled by requestConsent if empty
        QString     appId;
        Capability  capability = Capability::Unknown;
        QString     resourceType;
        QString     resourceId;
        QString     description;    // human-readable, shown in consent dialog
        int         timeoutMs = 30000;
    };

    explicit ConsentMediator(QObject *parent = nullptr);

    void setPermissionStore(PermissionStore *store);

    // Initiates an async consent request.
    // Returns the requestId (generated if req.requestId is empty).
    // Emits consentRequested so the UI layer can display a dialog.
    QString requestConsent(const ConsentRequest &request);

    // Called by the UI to resolve a pending request.
    // No-op if requestId is unknown or already resolved.
    void fulfillConsent(const QString &requestId, UserResponse response);

    // Cancels a pending request (resolves as DenyOnce, no persistence).
    void cancelConsent(const QString &requestId);

    // Returns the set of currently pending request IDs.
    QStringList pendingRequestIds() const;

signals:
    // Emitted when UI should show a consent dialog.
    void consentRequested(const QString &requestId, const QVariantMap &payload);

    // Emitted when the request is resolved (via fulfillConsent, cancel, or timeout).
    void consentResolved(const QString &requestId,
                         ConsentMediator::UserResponse response,
                         const Slm::Permissions::PolicyDecision &decision);

private slots:
    void onTimeout(const QString &requestId);

private:
    struct PendingRequest {
        ConsentRequest request;
        QTimer        *timer = nullptr;
    };

    void resolveRequest(const QString &requestId, UserResponse response);
    PolicyDecision responseToDecision(const ConsentRequest &req,
                                      UserResponse response) const;
    QVariantMap requestPayload(const ConsentRequest &req) const;

    QHash<QString, PendingRequest> m_pending;
    PermissionStore               *m_store = nullptr;
};

} // namespace Slm::Permissions
