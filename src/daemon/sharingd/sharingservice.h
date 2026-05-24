#pragma once

#include <QDBusContext>
#include <QObject>
#include <QVariantMap>

#include "../../core/permissions/AuditLogger.h"
#include "../../core/permissions/DBusSecurityGuard.h"
#include "../../core/permissions/PermissionBroker.h"
#include "../../core/permissions/PermissionStore.h"
#include "../../core/permissions/PolicyEngine.h"
#include "../../core/permissions/TrustResolver.h"

class SharingManager;

class SharingService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Sharing")
    Q_PROPERTY(bool serviceRegistered READ serviceRegistered NOTIFY serviceRegisteredChanged)
    Q_PROPERTY(QString apiVersion READ apiVersion CONSTANT)

public:
    explicit SharingService(SharingManager *manager, QObject *parent = nullptr);
    ~SharingService() override;

    bool serviceRegistered() const;
    QString apiVersion() const;

public slots:
    QVariantMap Ping() const;
    QVariantMap GetCapabilities() const;
    QVariantMap GetStatus() const;

    QVariantMap SetFeatureEnabled(const QString &feature, bool enabled);
    QVariantMap GetFeatureState(const QString &feature) const;

    QVariantMap AddSharedFolder(const QString &path, const QVariantMap &options);
    QVariantMap RemoveSharedFolder(const QString &path);
    QVariantMap UpdateSharedFolder(const QString &path, const QVariantMap &options);
    QVariantMap ListSharedFolders() const;

    QVariantMap SendFile(const QString &deviceId, const QString &path);
    QVariantMap ListActiveTransfers() const;
    QVariantMap CancelTransfer(const QString &transferId);
    QVariantMap AcceptIncomingTransfer(const QString &transferId, const QString &savePath);
    QVariantMap RejectIncomingTransfer(const QString &transferId);

    QVariantMap CheckEnvironment() const;
    QVariantMap TryAutoFix(const QString &issue);

    // Folder sharing compat API (replaces org.slm.Desktop.FolderSharing)
    QVariantMap ConfigureShare(const QString &path, const QVariantMap &options);
    QVariantMap DisableShare(const QString &path);
    QVariantMap ShareInfo(const QString &path) const;
    QVariantMap ListShares() const;
    QVariantMap CheckFileSharingEnvironment() const;
    QVariantMap TryAutoFixFileSharing();
    QVariantMap SetupSharingPrerequisites();

signals:
    void serviceRegisteredChanged();
    void SharedFolderAdded(const QString &path, const QVariantMap &info);
    void SharedFolderRemoved(const QString &path);
    void TransferStarted(const QString &transferId, const QVariantMap &info);
    void TransferProgress(const QString &transferId, qint64 bytesTransferred, qint64 totalBytes);
    void TransferCompleted(const QString &transferId, bool success, const QString &error);
    void TransferIncomingRequest(const QString &transferId, const QString &fromDeviceId, const QVariantMap &info);
    void FeatureStateChanged(const QString &feature, bool enabled);
    void ShareStateChanged(const QString &path, const QVariantMap &shareInfo);

private:
    void connectManagerSignals();
    bool checkPermission(Slm::Permissions::Capability capability, const QString &methodName) const;
    static QVariantMap makeResult(bool ok,
                                  const QString &error = QString(),
                                  const QVariantMap &extra = QVariantMap());

    bool m_serviceRegistered = false;
    SharingManager *m_manager = nullptr;

    mutable Slm::Permissions::PermissionStore m_permissionStore;
    mutable Slm::Permissions::TrustResolver m_trustResolver;
    mutable Slm::Permissions::PolicyEngine m_policyEngine;
    mutable Slm::Permissions::AuditLogger m_auditLogger;
    mutable Slm::Permissions::PermissionBroker m_permissionBroker;
    mutable Slm::Permissions::DBusSecurityGuard m_securityGuard;
};
