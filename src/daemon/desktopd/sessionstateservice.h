#pragma once

#include <QObject>
#include <QDBusContext>
#include <QVariantMap>
#include <QHash>
#include "sessionunlockthrottle.h"
#include "../../core/permissions/AuditLogger.h"
#include "../../core/permissions/DBusSecurityGuard.h"
#include "../../core/permissions/PermissionBroker.h"
#include "../../core/permissions/PermissionStore.h"
#include "../../core/permissions/PolicyEngine.h"
#include "../../core/permissions/TrustResolver.h"

class SessionStateManager;

class SessionStateService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.SessionState1")
    Q_PROPERTY(bool serviceRegistered READ serviceRegistered NOTIFY serviceRegisteredChanged)
    Q_PROPERTY(QString apiVersion READ apiVersion CONSTANT)
    Q_PROPERTY(QString lockState READ lockState NOTIFY LockStateChanged)

public:
    explicit SessionStateService(SessionStateManager *manager, QObject *parent = nullptr);
    ~SessionStateService() override;

    bool serviceRegistered() const;
    QString apiVersion() const;
    QString lockState() const;

public slots:
    QVariantMap Ping() const;
    QVariantMap GetCapabilities() const;
    uint Inhibit(const QString &reason, uint flags);
    void Logout();
    void Shutdown();
    void Reboot();
    void Lock();
    QVariantMap Unlock(const QString &password);
    // Force the session into Locked state without going through the normal
    // user-driven Lock() flow. Intended for slm-lockd to invoke when the
    // shell process disappears unexpectedly. Idempotent.
    QVariantMap ForceLocked(const QString &reason);

signals:
    void serviceRegisteredChanged();
    void SessionLocked();
    void SessionUnlocked();
    void LockStateChanged(const QString &state);
    void IdleChanged(bool idle);
    void ActiveAppChanged(const QString &app_id);
    // Forwarded from SessionStateManager.onPrepareForSleep(false). Shell
    // listens for this and re-attaches the LayerShell security overlay on
    // each output after a wake.
    void Resumed();

private:
    QString throttleKey() const;
    QString throttleStatePath() const;
    int lockoutSecondsForLevel(int level) const;
    int decayWindowSeconds() const;
    bool applyThrottleDecay(Slm::Desktopd::SessionUnlockThrottleState &state, qint64 nowMs) const;
    QVariantMap rateLimitedResult(const QString &key, qint64 nowMs);
    void recordUnlockFailure(const QString &key, qint64 nowMs);
    void recordUnlockSuccess(const QString &key);
    void loadUnlockThrottleState();
    void saveUnlockThrottleState() const;

    void registerDbusService();
    void setupSecurity();
    bool checkPermission(Slm::Permissions::Capability capability, const QString &methodName);
    void setLockState(const QString &state, const QString &reason);
    void onManagerSessionLocked();
    void onManagerSessionUnlocked();

    SessionStateManager *m_manager = nullptr;
    bool m_serviceRegistered = false;
    QString m_lockState = QStringLiteral("Active");

    mutable Slm::Permissions::PermissionStore m_permissionStore;
    mutable Slm::Permissions::TrustResolver m_trustResolver;
    mutable Slm::Permissions::PolicyEngine m_policyEngine;
    mutable Slm::Permissions::AuditLogger m_auditLogger;
    mutable Slm::Permissions::PermissionBroker m_permissionBroker;
    mutable Slm::Permissions::DBusSecurityGuard m_securityGuard;
    QHash<QString, Slm::Desktopd::SessionUnlockThrottleState> m_unlockThrottle;
};
