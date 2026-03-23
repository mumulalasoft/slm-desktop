#pragma once

#include "../../core/actions/framework/slmactionframework.h"
#include "../../core/permissions/AuditLogger.h"
#include "../../core/permissions/DBusSecurityGuard.h"
#include "../../core/permissions/PermissionBroker.h"
#include "../../core/permissions/PermissionStore.h"
#include "../../core/permissions/PolicyEngine.h"
#include "../../core/permissions/TrustResolver.h"

#include <QDBusContext>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class SlmCapabilityService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.SLMCapabilities")
    Q_PROPERTY(bool serviceRegistered READ serviceRegistered NOTIFY serviceRegisteredChanged)

public:
    explicit SlmCapabilityService(Slm::Actions::Framework::SlmActionFramework *framework,
                                  QObject *parent = nullptr);
    ~SlmCapabilityService() override;

    bool serviceRegistered() const;

public slots:
    QVariantMap Ping() const;
    QVariantList ListActions(const QString &capability,
                             const QStringList &uris,
                             const QVariantMap &options = QVariantMap());
    QVariantList ListActionsWithContext(const QString &capability,
                                        const QVariantMap &context);
    QVariantMap InvokeAction(const QString &actionId,
                             const QStringList &uris,
                             const QVariantMap &options = QVariantMap());
    QVariantMap InvokeActionWithContext(const QString &actionId,
                                        const QVariantMap &context);
    QVariantMap ResolveDefaultAction(const QString &capability,
                                     const QVariantMap &context);
    QVariantList SearchActions(const QString &query,
                               const QVariantMap &context);
    QVariantList ListPermissionGrants();
    QVariantList ListPermissionAudit(int limit = 200);
    QVariantMap ResetPermissionsForApp(const QString &appId);
    QVariantMap ResetAllPermissions();
    void Reload();

signals:
    void serviceRegisteredChanged();
    void IndexingStarted();
    void IndexingFinished();
    void ProviderRegistered(const QString &provider_id);

private:
    void registerDbus();
    void setupSecurity();
    Slm::Permissions::Capability permissionCapabilityForList(const QString &capability) const;
    Slm::Permissions::Capability permissionCapabilityForInvoke(const QVariantMap &context) const;
    bool allowedByPolicy(const Slm::Permissions::Capability capability,
                         const QVariantMap &context,
                         const QString &methodName,
                         const QString &requestId) const;
    bool adminAllowed(const QString &methodName,
                      const QString &requestId) const;

    Slm::Actions::Framework::SlmActionFramework *m_framework = nullptr;
    bool m_serviceRegistered = false;
    mutable Slm::Permissions::PermissionStore m_permissionStore;
    mutable Slm::Permissions::TrustResolver m_trustResolver;
    mutable Slm::Permissions::PolicyEngine m_policyEngine;
    mutable Slm::Permissions::AuditLogger m_auditLogger;
    mutable Slm::Permissions::PermissionBroker m_permissionBroker;
    mutable Slm::Permissions::DBusSecurityGuard m_securityGuard;
};
