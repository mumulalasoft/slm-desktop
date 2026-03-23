#pragma once

#include <QObject>
#include <QDBusContext>
#include <QVariantMap>
#include <QHash>
#include "src/core/permissions/AuditLogger.h"
#include "src/core/permissions/DBusSecurityGuard.h"
#include "src/core/permissions/PermissionBroker.h"
#include "src/core/permissions/PermissionStore.h"
#include "src/core/permissions/PolicyEngine.h"
#include "src/core/permissions/TrustResolver.h"

#include <memory>

class ScreencastStreamBackend;

class ScreencastService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.Screencast")
    Q_PROPERTY(bool serviceRegistered READ serviceRegistered NOTIFY serviceRegisteredChanged)
    Q_PROPERTY(QString apiVersion READ apiVersion CONSTANT)

public:
    explicit ScreencastService(QObject *parent = nullptr);
    ~ScreencastService() override;

    bool serviceRegistered() const;
    QString apiVersion() const;

public slots:
    QVariantMap Ping() const;
    QVariantMap GetState() const;
    QVariantMap GetSessionStreams(const QString &sessionPath) const;
    QVariantMap UpdateSessionStreams(const QString &sessionPath, const QVariantList &streams);
    QVariantMap EndSession(const QString &sessionPath);
    QVariantMap RevokeSession(const QString &sessionPath, const QString &reason);

signals:
    void serviceRegisteredChanged();
    void SessionStreamsChanged(const QString &sessionPath, const QVariantList &streams);
    void SessionEnded(const QString &sessionPath);
    void SessionRevoked(const QString &sessionPath, const QString &reason);

private:
    void registerDbusService();
    void setupSecurity();
    bool checkPermission(Slm::Permissions::Capability capability, const QString &methodName);

    static QVariantMap invalidArgument(const QString &reason);
    static QVariantMap success(const QVariantMap &results);
    static QVariantList normalizeStreams(const QVariantList &streams);
    void attachStreamBackend();
    void onBackendSessionStreamsChanged(const QString &sessionPath, const QVariantList &streams);
    void onBackendSessionEnded(const QString &sessionPath);
    void onBackendSessionRevoked(const QString &sessionPath, const QString &reason);

private:
    bool m_serviceRegistered = false;
    QHash<QString, QVariantList> m_sessionStreams;
    std::unique_ptr<ScreencastStreamBackend> m_streamBackend;
    QString m_streamBackendRequestedMode = QStringLiteral("portal-mirror");
    QString m_streamBackendMode = QStringLiteral("none");
    QString m_streamBackendFallbackReason;

    mutable Slm::Permissions::PermissionStore m_permissionStore;
    mutable Slm::Permissions::TrustResolver m_trustResolver;
    mutable Slm::Permissions::PolicyEngine m_policyEngine;
    mutable Slm::Permissions::AuditLogger m_auditLogger;
    mutable Slm::Permissions::PermissionBroker m_permissionBroker;
    mutable Slm::Permissions::DBusSecurityGuard m_securityGuard;
};
