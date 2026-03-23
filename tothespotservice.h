#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QDBusContext>
#include "src/core/permissions/AuditLogger.h"
#include "src/core/permissions/DBusSecurityGuard.h"
#include "src/core/permissions/PermissionBroker.h"
#include "src/core/permissions/PermissionStore.h"
#include "src/core/permissions/PolicyEngine.h"
#include "src/core/permissions/TrustResolver.h"

class TothespotService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)

public:
    explicit TothespotService(QObject *parent = nullptr);

    bool ready() const;

    Q_INVOKABLE QVariantList query(const QString &text,
                                   const QVariantMap &options = QVariantMap(),
                                   int limit = 100);
    Q_INVOKABLE bool activateResult(const QString &id,
                                    const QVariantMap &activateData = QVariantMap());
    Q_INVOKABLE QVariantMap previewResult(const QString &id);
    Q_INVOKABLE QVariantMap resolveClipboardResult(const QString &id,
                                                   const QVariantMap &resolveData = QVariantMap());
    Q_INVOKABLE QVariantMap configureTrackerPreset(const QVariantMap &preset = QVariantMap());
    Q_INVOKABLE QVariantMap resetTrackerPreset();
    Q_INVOKABLE QVariantMap trackerPresetStatus();
    Q_INVOKABLE QVariantList searchProfiles();
    Q_INVOKABLE QString activeSearchProfile();
    Q_INVOKABLE QVariantMap activeSearchProfileMeta();
    Q_INVOKABLE bool setActiveSearchProfile(const QString &profileId);
    Q_INVOKABLE QVariantMap telemetryMeta();
    Q_INVOKABLE QVariantList activationTelemetry(int limit = 100);
    Q_INVOKABLE bool resetActivationTelemetry();

signals:
    void readyChanged();
    void searchProfileChanged(const QString &profileId);

private:
    void setupSecurity();
    bool checkPermission(Slm::Permissions::Capability capability, const QString &methodName);
    Q_SLOT void handleSearchProfileChanged(const QString &profileId);

    mutable Slm::Permissions::PermissionStore m_permissionStore;
    mutable Slm::Permissions::TrustResolver m_trustResolver;
    mutable Slm::Permissions::PolicyEngine m_policyEngine;
    mutable Slm::Permissions::AuditLogger m_auditLogger;
    mutable Slm::Permissions::PermissionBroker m_permissionBroker;
    mutable Slm::Permissions::DBusSecurityGuard m_securityGuard;
};
