#pragma once

#include <QObject>
#include <QDBusContext>
#include <QHash>
#include <QVariantMap>
#include <memory>

#include "inputcapturebackend.h"
#include "src/core/permissions/AuditLogger.h"
#include "src/core/permissions/DBusSecurityGuard.h"
#include "src/core/permissions/PermissionBroker.h"
#include "src/core/permissions/PermissionStore.h"
#include "src/core/permissions/PolicyEngine.h"
#include "src/core/permissions/TrustResolver.h"

class InputCaptureService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.InputCapture")
    Q_PROPERTY(bool serviceRegistered READ serviceRegistered NOTIFY serviceRegisteredChanged)
    Q_PROPERTY(QString apiVersion READ apiVersion CONSTANT)

public:
    explicit InputCaptureService(QObject *parent = nullptr);
    ~InputCaptureService() override;

    bool serviceRegistered() const;
    QString apiVersion() const;

public slots:
    QVariantMap Ping() const;
    QVariantMap GetState() const;
    QVariantMap GetSessionState(const QString &sessionPath) const;
    QVariantMap CreateSession(const QString &sessionPath,
                              const QString &appId,
                              const QVariantMap &options);
    QVariantMap SetPointerBarriers(const QString &sessionPath,
                                   const QVariantList &barriers,
                                   const QVariantMap &options);
    QVariantMap EnableSession(const QString &sessionPath, const QVariantMap &options);
    QVariantMap DisableSession(const QString &sessionPath, const QVariantMap &options);
    QVariantMap ReleaseSession(const QString &sessionPath, const QString &reason);

signals:
    void serviceRegisteredChanged();
    void SessionStateChanged(const QString &sessionPath, bool enabled, int barrierCount);
    void SessionReleased(const QString &sessionPath, const QString &reason);

private:
    struct SessionRecord {
        QString sessionHandle;
        QString appId;
        QString ownerService;
        bool enabled = false;
        QVariantList barriers;
        qint64 updatedAtMs = 0;
    };

    void registerDbusService();
    void setupSecurity();
    bool checkPermission(Slm::Permissions::Capability capability, const QString &methodName) const;
    bool isOwnerAllowed(const SessionRecord &record) const;
    static QVariantMap invalidArgument(const QString &reason);
    static QVariantMap denied(const QString &reason);
    static QVariantMap denied(const QString &reason, const QVariantMap &results);
    static QVariantMap success(const QVariantMap &results);
    static QString normalizedSession(const QString &sessionPath);
    static bool validateBarriers(const QVariantList &barriers, QString *reasonOut);
    QVariantMap sessionToMap(const SessionRecord &record) const;
    QVariantMap withBackendResult(QVariantMap results, const QVariantMap &backend) const;

    bool m_serviceRegistered = false;
    QHash<QString, SessionRecord> m_sessions;
    std::unique_ptr<InputCaptureBackend> m_backend;

    mutable Slm::Permissions::PermissionStore m_permissionStore;
    mutable Slm::Permissions::TrustResolver m_trustResolver;
    mutable Slm::Permissions::PolicyEngine m_policyEngine;
    mutable Slm::Permissions::AuditLogger m_auditLogger;
    mutable Slm::Permissions::PermissionBroker m_permissionBroker;
    mutable Slm::Permissions::DBusSecurityGuard m_securityGuard;
};
