#include "PortalPermissionStoreAdapter.h"

#include "../../../core/permissions/PermissionStore.h"

namespace Slm::PortalAdapter {

PortalPermissionStoreAdapter::PortalPermissionStoreAdapter(QObject *parent)
    : QObject(parent)
{
}

void PortalPermissionStoreAdapter::setStore(Slm::Permissions::PermissionStore *store)
{
    m_store = store;
}

PortalStoredDecision PortalPermissionStoreAdapter::loadDecision(
    const QString &appId,
    Slm::Permissions::Capability capability,
    const QString &resourceType,
    const QString &resourceId) const
{
    PortalStoredDecision out;
    if (!m_store || appId.trimmed().isEmpty()) {
        return out;
    }
    const Slm::Permissions::StoredPermission stored =
        m_store->findPermission(appId, capability, resourceType, resourceId);
    if (!stored.valid) {
        return out;
    }
    out.found = true;
    out.decision = stored.decision;
    out.scope = stored.scope;
    out.resourceType = stored.resourceType;
    out.resourceId = stored.resourceId;
    return out;
}

bool PortalPermissionStoreAdapter::saveDecision(const QString &appId,
                                                Slm::Permissions::Capability capability,
                                                const QString &resourceType,
                                                const QString &resourceId,
                                                Slm::Permissions::DecisionType decision,
                                                const QString &scope)
{
    if (!m_store || appId.trimmed().isEmpty()) {
        return false;
    }
    return m_store->savePermission(appId,
                                   capability,
                                   decision,
                                   scope,
                                   resourceType,
                                   resourceId);
}

bool PortalPermissionStoreAdapter::removeDecision(const QString &appId,
                                                  Slm::Permissions::Capability capability,
                                                  const QString &resourceType,
                                                  const QString &resourceId)
{
    if (!m_store || appId.trimmed().isEmpty()) {
        return false;
    }
    return m_store->removePermission(appId,
                                     capability,
                                     resourceType,
                                     resourceId);
}

QVariantList PortalPermissionStoreAdapter::listDecisions(const QString &appId) const
{
    QVariantList out;
    if (!m_store) {
        return out;
    }
    const QVariantList all = m_store->listPermissions();
    if (appId.trimmed().isEmpty()) {
        return all;
    }
    for (const QVariant &row : all) {
        const QVariantMap map = row.toMap();
        if (map.value(QStringLiteral("appId")).toString().compare(appId, Qt::CaseInsensitive) == 0) {
            out.push_back(map);
        }
    }
    return out;
}

} // namespace Slm::PortalAdapter
