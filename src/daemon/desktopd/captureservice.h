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

class CaptureService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.Capture")
    Q_PROPERTY(bool serviceRegistered READ serviceRegistered NOTIFY serviceRegisteredChanged)
    Q_PROPERTY(QString apiVersion READ apiVersion CONSTANT)

public:
    explicit CaptureService(QObject *parent = nullptr);
    ~CaptureService() override;

    bool serviceRegistered() const;
    QString apiVersion() const;

public slots:
    QVariantMap Ping() const;
    QVariantMap GetCapabilities() const;
    QVariantMap GetScreencastStreams(const QString &sessionPath,
                                     const QString &appId,
                                     const QVariantMap &options);
    QVariantMap SetScreencastSessionStreams(const QString &sessionPath,
                                            const QVariantList &streams,
                                            const QVariantMap &options);
    QVariantMap ClearScreencastSession(const QString &sessionPath);
    QVariantMap RevokeScreencastSession(const QString &sessionPath,
                                        const QString &reason);

signals:
    void serviceRegisteredChanged();
    void ScreencastSessionStreamsChanged(const QString &sessionPath, int streamCount);
    void ScreencastSessionCleared(const QString &sessionPath, bool revoked, const QString &reason);

private:
    void connectPortalSignals();
    void registerDbusService();
    void setupSecurity();
    bool checkPermission(Slm::Permissions::Capability capability, const QString &methodName);

    static QVariantMap invalidArgument(const QString &reason);
    static QVariantMap success(const QVariantMap &results);
    static QVariantList normalizeStreamsFromOptions(const QVariantMap &options);
    static QVariantList queryStreamsFromScreencastService(const QString &sessionPath);
    static QVariantList queryStreamsFromPortalService(const QString &sessionPath);

private slots:
    void onPortalSessionStateChanged(const QString &sessionHandle,
                                     const QString &appId,
                                     bool active,
                                     int activeCount);
    void onPortalSessionRevoked(const QString &sessionHandle,
                                const QString &appId,
                                const QString &reason,
                                int activeCount);

private:
    bool m_serviceRegistered = false;
    QHash<QString, QVariantList> m_sessionStreams;

    mutable Slm::Permissions::PermissionStore m_permissionStore;
    mutable Slm::Permissions::TrustResolver m_trustResolver;
    mutable Slm::Permissions::PolicyEngine m_policyEngine;
    mutable Slm::Permissions::AuditLogger m_auditLogger;
    mutable Slm::Permissions::PermissionBroker m_permissionBroker;
    mutable Slm::Permissions::DBusSecurityGuard m_securityGuard;
};
