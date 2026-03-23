#pragma once

#include "../../../core/permissions/AccessContext.h"
#include "../../../core/permissions/CallerIdentity.h"
#include "../../../core/permissions/Capability.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <functional>

namespace Slm::PortalAdapter {

enum class UserDecision {
    AllowOnce = 0,
    AllowAlways,
    Deny,
    DenyAlways,
    Cancelled
};

struct ConsentResult {
    UserDecision decision = UserDecision::Cancelled;
    bool persist = false;
    QString scope;
    QString reason;
};

class PortalDialogBridge : public QObject
{
    Q_OBJECT

public:
    using ConsentCallback = std::function<void(const ConsentResult &)>;

    explicit PortalDialogBridge(QObject *parent = nullptr);

    void requestConsent(const QString &requestPath,
                        const Slm::Permissions::CallerIdentity &caller,
                        Slm::Permissions::Capability capability,
                        const Slm::Permissions::AccessContext &context,
                        const ConsentCallback &callback);

public slots:
    void submitConsent(const QString &requestPath,
                       const QString &decision,
                       bool persist,
                       const QString &scope,
                       const QString &reason);

signals:
    void consentRequested(const QString &requestPath, const QVariantMap &payload);

private:
    QHash<QString, ConsentCallback> m_pending;
};

} // namespace Slm::PortalAdapter

