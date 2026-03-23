#pragma once

#include "../../../core/permissions/Capability.h"
#include "../../../core/permissions/PolicyDecision.h"

#include <QObject>
#include <QString>
#include <QVariantList>

namespace Slm::Permissions {
class PermissionStore;
}

namespace Slm::PortalAdapter {

struct PortalStoredDecision {
    bool found = false;
    Slm::Permissions::DecisionType decision = Slm::Permissions::DecisionType::Deny;
    QString scope;
    QString resourceType;
    QString resourceId;
};

class PortalPermissionStoreAdapter : public QObject
{
    Q_OBJECT

public:
    explicit PortalPermissionStoreAdapter(QObject *parent = nullptr);

    void setStore(Slm::Permissions::PermissionStore *store);

    PortalStoredDecision loadDecision(const QString &appId,
                                      Slm::Permissions::Capability capability,
                                      const QString &resourceType = QString(),
                                      const QString &resourceId = QString()) const;
    bool saveDecision(const QString &appId,
                      Slm::Permissions::Capability capability,
                      const QString &resourceType,
                      const QString &resourceId,
                      Slm::Permissions::DecisionType decision,
                      const QString &scope);
    bool removeDecision(const QString &appId,
                        Slm::Permissions::Capability capability,
                        const QString &resourceType = QString(),
                        const QString &resourceId = QString());
    QVariantList listDecisions(const QString &appId) const;

private:
    Slm::Permissions::PermissionStore *m_store = nullptr;
};

} // namespace Slm::PortalAdapter
