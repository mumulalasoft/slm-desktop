#include "ConsentMediator.h"

#include <QTimer>
#include <QUuid>

namespace Slm::Permissions {

ConsentMediator::ConsentMediator(QObject *parent)
    : QObject(parent)
{
}

void ConsentMediator::setPermissionStore(PermissionStore *store)
{
    m_store = store;
}

QString ConsentMediator::requestConsent(const ConsentRequest &request)
{
    ConsentRequest req = request;
    if (req.requestId.isEmpty()) {
        req.requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    // Set up timeout timer.
    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setInterval(req.timeoutMs > 0 ? req.timeoutMs : 30000);

    const QString reqId = req.requestId;
    connect(timer, &QTimer::timeout, this, [this, reqId]() {
        onTimeout(reqId);
    });

    m_pending.insert(reqId, PendingRequest{std::move(req), timer});
    timer->start();

    emit consentRequested(reqId, requestPayload(m_pending[reqId].request));
    return reqId;
}

void ConsentMediator::fulfillConsent(const QString &requestId, UserResponse response)
{
    if (!m_pending.contains(requestId)) {
        return;
    }
    resolveRequest(requestId, response);
}

void ConsentMediator::cancelConsent(const QString &requestId)
{
    if (!m_pending.contains(requestId)) {
        return;
    }
    resolveRequest(requestId, UserResponse::DenyOnce);
}

QStringList ConsentMediator::pendingRequestIds() const
{
    return m_pending.keys();
}

// ── Private ───────────────────────────────────────────────────────────────────

void ConsentMediator::onTimeout(const QString &requestId)
{
    if (!m_pending.contains(requestId)) {
        return;
    }
    qWarning("ConsentMediator: request '%s' timed out — denying",
             qUtf8Printable(requestId));
    resolveRequest(requestId, UserResponse::Timeout);
}

void ConsentMediator::resolveRequest(const QString &requestId, UserResponse response)
{
    auto it = m_pending.find(requestId);
    if (it == m_pending.end()) {
        return;
    }

    PendingRequest pending = std::move(it.value());
    m_pending.erase(it);

    if (pending.timer) {
        pending.timer->stop();
        pending.timer->deleteLater();
        pending.timer = nullptr;
    }

    const PolicyDecision decision = responseToDecision(pending.request, response);

    // Auto-persist AllowAlways / DenyAlways decisions.
    if (m_store && m_store->isOpen()) {
        if (response == UserResponse::AllowAlways) {
            m_store->savePermission(pending.request.appId,
                                    pending.request.capability,
                                    DecisionType::AllowAlways,
                                    QStringLiteral("persistent"),
                                    pending.request.resourceType,
                                    pending.request.resourceId);
        } else if (response == UserResponse::DenyAlways) {
            m_store->savePermission(pending.request.appId,
                                    pending.request.capability,
                                    DecisionType::DenyAlways,
                                    QStringLiteral("persistent"),
                                    pending.request.resourceType,
                                    pending.request.resourceId);
        }
    }

    emit consentResolved(requestId, response, decision);
}

PolicyDecision ConsentMediator::responseToDecision(const ConsentRequest &req,
                                                    UserResponse response) const
{
    PolicyDecision out;
    out.capability = req.capability;
    out.persistentEligible = (response == UserResponse::AllowAlways
                               || response == UserResponse::DenyAlways);
    switch (response) {
    case UserResponse::AllowOnce:
        out.type   = DecisionType::Allow;
        out.reason = QStringLiteral("consent-allow-once");
        break;
    case UserResponse::AllowAlways:
        out.type   = DecisionType::AllowAlways;
        out.reason = QStringLiteral("consent-allow-always");
        break;
    case UserResponse::DenyOnce:
        out.type   = DecisionType::Deny;
        out.reason = QStringLiteral("consent-deny-once");
        break;
    case UserResponse::DenyAlways:
        out.type   = DecisionType::DenyAlways;
        out.reason = QStringLiteral("consent-deny-always");
        break;
    case UserResponse::Timeout:
        out.type   = DecisionType::Deny;
        out.reason = QStringLiteral("consent-timeout");
        break;
    }
    return out;
}

QVariantMap ConsentMediator::requestPayload(const ConsentRequest &req) const
{
    return {
        {QStringLiteral("requestId"),    req.requestId},
        {QStringLiteral("appId"),        req.appId},
        {QStringLiteral("capability"),   capabilityToString(req.capability)},
        {QStringLiteral("resourceType"), req.resourceType},
        {QStringLiteral("resourceId"),   req.resourceId},
        {QStringLiteral("description"),  req.description},
        {QStringLiteral("timeoutMs"),    req.timeoutMs},
    };
}

} // namespace Slm::Permissions
