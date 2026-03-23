#include <QCoreApplication>
#include <QDebug>

#include "../../core/permissions/AuditLogger.h"
#include "../../core/permissions/PermissionBroker.h"
#include "../../core/permissions/PermissionStore.h"
#include "../../core/permissions/PolicyEngine.h"
#include "../../core/permissions/TrustResolver.h"
#include "portal-adapter/PortalBackendService.h"
#include "implportalservice.h"
#include "portalmanager.h"
#include "portalservice.h"

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("slm-portald"));
    QCoreApplication::setOrganizationName(QStringLiteral("SLM"));

    Slm::Permissions::PermissionStore permissionStore;
    permissionStore.open();
    Slm::Permissions::TrustResolver trustResolver;
    Slm::Permissions::PolicyEngine policyEngine;
    policyEngine.setStore(&permissionStore);
    Slm::Permissions::AuditLogger auditLogger(&permissionStore);
    Slm::Permissions::PermissionBroker permissionBroker;
    permissionBroker.setStore(&permissionStore);
    permissionBroker.setPolicyEngine(&policyEngine);
    permissionBroker.setAuditLogger(&auditLogger);

    PortalManager manager;
    Slm::PortalAdapter::PortalBackendService backend;
    backend.configurePermissions(&trustResolver, &permissionBroker, &auditLogger, &permissionStore);

    PortalService service(&manager, &backend);
    ImplPortalService implService(&manager, &backend);

    qInfo().noquote() << "[slm-portald] serviceRegistered=" << service.serviceRegistered();
    qInfo().noquote() << "[slm-portald] implServiceRegistered=" << implService.serviceRegistered();
    qInfo().noquote() << "[slm-portald] permission-db=" << permissionStore.databasePath();

    return app.exec();
}
