#pragma once

#include "PortalAccessMediator.h"
#include "PortalCapabilityMapper.h"
#include "PortalDialogBridge.h"
#include "PortalPermissionStoreAdapter.h"
#include "PortalRequestManager.h"
#include "PortalSessionManager.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QObject>
#include <QVariantMap>

namespace Slm::Permissions {
class AuditLogger;
class PermissionBroker;
class PermissionStore;
class TrustResolver;
}

namespace Slm::PortalAdapter {

class PortalBackendService : public QObject
{
    Q_OBJECT

public:
    explicit PortalBackendService(QObject *parent = nullptr);

    void setBus(const QDBusConnection &bus);

    PortalRequestManager *requestManager() const;
    PortalSessionManager *sessionManager() const;
    PortalCapabilityMapper *capabilityMapper() const;
    PortalPermissionStoreAdapter *permissionStoreAdapter() const;
    PortalDialogBridge *dialogBridge() const;
    PortalAccessMediator *accessMediator() const;

    void configurePermissions(Slm::Permissions::TrustResolver *trustResolver,
                              Slm::Permissions::PermissionBroker *permissionBroker,
                              Slm::Permissions::AuditLogger *auditLogger,
                              Slm::Permissions::PermissionStore *permissionStore);

public slots:
    QVariantMap handleRequest(const QString &portalMethod,
                              const QDBusMessage &message,
                              const QVariantMap &parameters);
    QVariantMap handleDirect(const QString &portalMethod,
                             const QDBusMessage &message,
                             const QVariantMap &parameters);
    QVariantMap handleSessionRequest(const QString &portalMethod,
                                     const QDBusMessage &message,
                                     const QVariantMap &parameters);
    QVariantMap handleSessionOperation(const QString &portalMethod,
                                       const QDBusMessage &message,
                                       const QString &sessionPath,
                                       const QVariantMap &parameters,
                                       bool requireActive = true);
    QVariantMap closeSession(const QString &sessionPath);
    QVariantMap revokeSession(const QString &sessionPath, const QString &reason);
    QVariantMap getSessionMetadata(const QString &sessionPath) const;

private:
    QDBusConnection m_bus = QDBusConnection::sessionBus();
    PortalRequestManager m_requestManager;
    PortalSessionManager m_sessionManager;
    PortalCapabilityMapper m_capabilityMapper;
    PortalPermissionStoreAdapter m_storeAdapter;
    PortalDialogBridge m_dialogBridge;
    PortalAccessMediator m_accessMediator;
};

} // namespace Slm::PortalAdapter
