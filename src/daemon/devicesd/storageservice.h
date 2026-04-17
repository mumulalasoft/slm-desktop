#pragma once

#include "src/core/permissions/DBusSecurityGuard.h"
#include "src/core/permissions/PermissionBroker.h"
#include "src/core/permissions/TrustResolver.h"
#include "src/core/permissions/PermissionStore.h"
#include "src/core/permissions/PolicyEngine.h"
#include "src/core/permissions/AuditLogger.h"

#include <QObject>
#include <QDBusContext>
#include <QVariantMap>

class DevicesManager;

class StorageService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.Storage")
    Q_PROPERTY(bool serviceRegistered READ serviceRegistered NOTIFY serviceRegisteredChanged)
    Q_PROPERTY(QString apiVersion READ apiVersion CONSTANT)

public:
    explicit StorageService(DevicesManager *manager, QObject *parent = nullptr);
    ~StorageService() override;

    bool serviceRegistered() const;
    QString apiVersion() const;

public slots:
    QVariantMap Ping() const;
    QVariantMap GetCapabilities() const;
    QVariantMap GetStorageLocations();
    QVariantMap StoragePolicyForPath(const QString &path);
    QVariantMap SetStoragePolicyForPath(const QString &path,
                                        const QVariantMap &policyPatch,
                                        const QString &scope);
    QVariantMap Mount(const QString &target);
    QVariantMap Eject(const QString &target);
    QVariantMap ConnectServer(const QString &serverUri);

signals:
    void serviceRegisteredChanged();
    void StorageLocationsChanged();

private:
    void registerDbusService();

    DevicesManager *m_manager = nullptr;
    bool m_serviceRegistered = false;
    Slm::Permissions::PermissionStore m_store;
    Slm::Permissions::PolicyEngine m_engine;
    Slm::Permissions::AuditLogger m_audit;
    Slm::Permissions::TrustResolver m_resolver;
    Slm::Permissions::PermissionBroker m_broker;
    Slm::Permissions::DBusSecurityGuard m_guard;
};
