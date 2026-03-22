#pragma once

#include <QObject>
#include <QDBusContext>
#include <QStringList>
#include <QVariantList>
#include "src/core/permissions/AuditLogger.h"
#include "src/core/permissions/DBusSecurityGuard.h"
#include "src/core/permissions/PermissionBroker.h"
#include "src/core/permissions/PermissionStore.h"
#include "src/core/permissions/PolicyEngine.h"
#include "src/core/permissions/TrustResolver.h"

class FileOperationsManager;

class FileOperationsService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.FileOperations")
    Q_PROPERTY(bool serviceRegistered READ serviceRegistered NOTIFY serviceRegisteredChanged)
    Q_PROPERTY(QString apiVersion READ apiVersion CONSTANT)

public:
    explicit FileOperationsService(FileOperationsManager *manager, QObject *parent = nullptr);
    ~FileOperationsService() override;

    bool serviceRegistered() const;
    QString apiVersion() const;

public slots:
    QVariantMap Ping() const;
    QVariantMap GetCapabilities() const;
    QString Copy(const QStringList &uris, const QString &destination);
    QString Move(const QStringList &uris, const QString &destination);
    QString Delete(const QStringList &uris);
    QString Trash(const QStringList &uris);
    QString EmptyTrash();
    bool Pause(const QString &id);
    bool Resume(const QString &id);
    bool Cancel(const QString &id);
    QVariantMap GetJob(const QString &id);
    QVariantList ListJobs();

signals:
    void serviceRegisteredChanged();
    void JobsChanged();
    void Progress(const QString &id, int percent);
    void ProgressDetail(const QString &id, int current, int total);
    void Finished(const QString &id);
    void Error(const QString &id);
    void ErrorDetail(const QString &id, const QString &code, const QString &message);

private:
    void registerDbusService();
    void setupSecurity();
    bool checkPermission(Slm::Permissions::Capability capability, const QString &methodName);

    FileOperationsManager *m_manager = nullptr;
    bool m_serviceRegistered = false;

    mutable Slm::Permissions::PermissionStore m_permissionStore;
    mutable Slm::Permissions::TrustResolver m_trustResolver;
    mutable Slm::Permissions::PolicyEngine m_policyEngine;
    mutable Slm::Permissions::AuditLogger m_auditLogger;
    mutable Slm::Permissions::PermissionBroker m_permissionBroker;
    mutable Slm::Permissions::DBusSecurityGuard m_securityGuard;
};
