#pragma once

#include <QDBusArgument>
#include <QDBusContext>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QElapsedTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include "src/core/permissions/AuditLogger.h"
#include "src/core/permissions/DBusSecurityGuard.h"
#include "src/core/permissions/PermissionBroker.h"
#include "src/core/permissions/PermissionStore.h"
#include "src/core/permissions/PolicyEngine.h"
#include "src/core/permissions/TrustResolver.h"

class DesktopAppModel;
class WorkspaceManager;

struct SearchResultEntry
{
    QString id;
    QVariantMap metadata;
};

Q_DECLARE_METATYPE(SearchResultEntry)
using SearchResultList = QList<SearchResultEntry>;
Q_DECLARE_METATYPE(SearchResultList)

inline QDBusArgument &operator<<(QDBusArgument &argument, const SearchResultEntry &entry)
{
    argument.beginStructure();
    argument << entry.id << entry.metadata;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, SearchResultEntry &entry)
{
    argument.beginStructure();
    argument >> entry.id >> entry.metadata;
    argument.endStructure();
    return argument;
}

class GlobalSearchService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.Search.v1")
    Q_PROPERTY(bool serviceRegistered READ serviceRegistered NOTIFY serviceRegisteredChanged)
    Q_PROPERTY(QString apiVersion READ apiVersion CONSTANT)

public:
    explicit GlobalSearchService(DesktopAppModel *appModel,
                                 WorkspaceManager *workspaceManager,
                                 QObject *desktopSettings = nullptr,
                                 QObject *parent = nullptr);
    ~GlobalSearchService() override;

    bool serviceRegistered() const;
    QString apiVersion() const;

public slots:
    SearchResultList Query(const QString &text, const QVariantMap &options);
    void ActivateResult(const QString &id, const QVariantMap &activate_data);
    QVariantMap PreviewResult(const QString &id);
    QVariantMap ResolveClipboardResult(const QString &id, const QVariantMap &resolve_data);
    QVariantMap ConfigureTrackerPreset(const QVariantMap &preset = QVariantMap());
    QVariantMap ResetTrackerPreset();
    QVariantMap TrackerPresetStatus() const;
    QVariantList GetSearchProfiles() const;
    QString GetActiveSearchProfile() const;
    QVariantMap GetActiveSearchProfileMeta() const;
    bool SetActiveSearchProfile(const QString &profileId);
    QVariantMap GetTelemetryMeta() const;
    QVariantList GetActivationTelemetry(int limit = 100) const;
    bool ResetActivationTelemetry();

signals:
    void serviceRegisteredChanged();
    void IndexingStarted();
    void IndexingFinished();
    void ProviderRegistered(const QString &provider_id);
    void SearchProfileChanged(const QString &profile_id);

private:
    struct TrackerPresetPolicy {
        int initialDelaySec = 120;
        int cpuLimit = 15;
        bool idleOnly = true;
        bool chargingOnly = true;
        QStringList ignoredDirectories;
    };

    void registerDbusService();
    void registerProvider(const QString &providerId);
    QVariantList queryRecentFiles(const QString &text, int limit) const;
    QVariantList queryClipboard(const QString &text,
                                int limit,
                                const QVariantMap &options) const;
    QVariantList querySettings(const QString &text,
                               int limit,
                               const QVariantMap &options) const;
    QVariantList queryTracker(const QString &text, int limit) const;
    QVariantList querySlmSearchActions(const QString &text,
                                       const QVariantMap &options,
                                       int limit) const;
    QVariantMap applyTrackerPresetInternal(const QVariantMap &preset, bool resetOnly) const;
    bool canUseTrackerNow(QVariantMap *reason) const;
    bool isSessionIdle() const;
    bool isOnACPower() const;
    QString trackerConfigPath() const;
    QString trackerStatePath() const;
    static QStringList defaultIgnoredTrackerDirs();
    static QString normalizePathFromUriOrText(const QString &value);
    static QString canonicalResultId(const QVariantMap &row);
    int sourceBoost(const QVariantMap &options, const QString &provider, int fallback) const;
    void pushActivationTelemetry(const QVariantMap &entry);
    void setupSecurity();
    bool allowSearchCapability(Slm::Permissions::Capability capability,
                               const QVariantMap &context,
                               const QString &methodName,
                               const QString &requestId) const;

    DesktopAppModel *m_appModel = nullptr;
    WorkspaceManager *m_workspaceManager = nullptr;
    QObject *m_desktopSettings = nullptr;
    bool m_serviceRegistered = false;
    QHash<QString, QVariantMap> m_resultIndex;
    QSet<QString> m_providers;
    QDateTime m_startedAtUtc;
    TrackerPresetPolicy m_trackerPolicy;
    mutable QDateTime m_searchProfileUpdatedAtUtc;
    struct TrackerCacheEntry {
        QElapsedTimer timer;
        QVariantList rows;
    };
    mutable QHash<QString, TrackerCacheEntry> m_trackerCache;
    QVector<QVariantMap> m_activationTelemetry;
    int m_activationTelemetryCapacity = 256;
    mutable Slm::Permissions::PermissionStore m_permissionStore;
    mutable Slm::Permissions::TrustResolver m_trustResolver;
    mutable Slm::Permissions::PolicyEngine m_policyEngine;
    mutable Slm::Permissions::AuditLogger m_auditLogger;
    mutable Slm::Permissions::PermissionBroker m_permissionBroker;
    mutable Slm::Permissions::DBusSecurityGuard m_securityGuard;
};
