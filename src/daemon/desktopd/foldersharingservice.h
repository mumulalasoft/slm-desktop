#pragma once

#include <QDBusContext>
#include <QDBusConnection>
#include <QObject>
#include <QVariantMap>

#include "src/core/permissions/AuditLogger.h"
#include "src/core/permissions/DBusSecurityGuard.h"
#include "src/core/permissions/PermissionBroker.h"
#include "src/core/permissions/PermissionStore.h"
#include "src/core/permissions/PolicyEngine.h"
#include "src/core/permissions/TrustResolver.h"

class FolderSharingService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.FolderSharing")
    Q_PROPERTY(bool serviceRegistered READ serviceRegistered NOTIFY serviceRegisteredChanged)
    Q_PROPERTY(QString apiVersion READ apiVersion CONSTANT)

public:
    explicit FolderSharingService(QObject *parent = nullptr);
    ~FolderSharingService() override;

    bool serviceRegistered() const;
    QString apiVersion() const;

public slots:
    QVariantMap Ping() const;
    QVariantMap GetCapabilities() const;
    QVariantMap ConfigureShare(const QString &path, const QVariantMap &options);
    QVariantMap DisableShare(const QString &path);
    QVariantMap ShareInfo(const QString &path) const;
    QVariantMap ListShares() const;
    QVariantMap CheckEnvironment() const;
    QVariantMap TryAutoFix();
    QVariantMap SetupSharingPrerequisites();

signals:
    void serviceRegisteredChanged();
    void ShareStateChanged(const QString &path, const QVariantMap &shareInfo);

private:
    void registerDbusService();
    void setupSecurity();
    bool checkPermission(Slm::Permissions::Capability capability, const QString &methodName) const;
    static QVariantMap makeResult(bool ok,
                                  const QString &error = QString(),
                                  const QVariantMap &extra = QVariantMap());

    QString statePath() const;
    QVariantMap loadState() const;
    bool saveState(const QVariantMap &state, QString *error = nullptr) const;
    QString canonicalSharePath(const QString &path) const;
    QVariantMap recordForPath(const QString &path) const;
    QVariantMap buildAddressPayload(const QString &shareName) const;
    QString sanitizeShareName(QString name, const QString &fallback) const;
    QString normalizeAccessMode(const QString &value) const;
    QVariantList normalizeUserList(const QVariant &value) const;

    QVariantMap applyShareBackend(const QString &path, const QVariantMap &record) const;
    QVariantMap disableShareBackend(const QVariantMap &record) const;
    QString buildUsershareAcl(const QVariantMap &record) const;
    bool callerPidUid(uint *outPid, uint *outUid) const;
    bool authorizeSetupAction(uint callerPid) const;
    QVariantMap runPrivilegedSetup(uint callerUid) const;

private:
    bool m_serviceRegistered = false;

    mutable Slm::Permissions::PermissionStore m_permissionStore;
    mutable Slm::Permissions::TrustResolver m_trustResolver;
    mutable Slm::Permissions::PolicyEngine m_policyEngine;
    mutable Slm::Permissions::AuditLogger m_auditLogger;
    mutable Slm::Permissions::PermissionBroker m_permissionBroker;
    mutable Slm::Permissions::DBusSecurityGuard m_securityGuard;
};
