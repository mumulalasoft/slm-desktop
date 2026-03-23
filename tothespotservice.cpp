#include "tothespotservice.h"

#include "globalsearchservice.h"
#include "resourcepaths.h"

#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusReply>
#include <QDBusConnection>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>
#include <QtGlobal>

#include "src/core/actions/slmactionregistry.h"
#include "src/core/actions/framework/slmactionframework.h"

#ifdef signals
#undef signals
#define DESKTOP_SHELL_RESTORE_QT_SIGNALS_MACRO
#endif
extern "C" {
#include <gio/gio.h>
}
#ifdef DESKTOP_SHELL_RESTORE_QT_SIGNALS_MACRO
#define signals Q_SIGNALS
#undef DESKTOP_SHELL_RESTORE_QT_SIGNALS_MACRO
#endif

namespace {
constexpr const char kSearchService[] = "org.slm.Desktop.Search.v1";
constexpr const char kSearchPath[] = "/org/slm/Desktop/Search";
constexpr const char kSearchIface[] = "org.slm.Desktop.Search.v1";
constexpr const char kCapabilityService[] = "org.freedesktop.SLMCapabilities";
constexpr const char kCapabilityPath[] = "/org/freedesktop/SLMCapabilities";
constexpr const char kCapabilityIface[] = "org.freedesktop.SLMCapabilities";
constexpr int kSearchTimeoutMs = 1200;

QStringList splitSemicolonList(const QString &value)
{
    QStringList out;
    const QStringList parts = value.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    out.reserve(parts.size());
    for (const QString &part : parts) {
        const QString token = part.trimmed();
        if (!token.isEmpty()) {
            out.push_back(token);
        }
    }
    return out;
}

bool tokenListContains(const QStringList &list, const QString &needle)
{
    const QString n = needle.trimmed().toLower();
    if (n.isEmpty()) {
        return false;
    }
    for (const QString &item : list) {
        if (item.trimmed().toLower() == n) {
            return true;
        }
    }
    return false;
}

int keywordMatchBonus(const QStringList &keywords, const QString &query)
{
    const QString q = query.trimmed().toLower();
    if (q.isEmpty()) {
        return 0;
    }
    int bonus = 0;
    for (const QString &raw : keywords) {
        const QString k = raw.trimmed().toLower();
        if (k.isEmpty()) {
            continue;
        }
        if (k == q) {
            bonus = qMax(bonus, 220);
        } else if (k.startsWith(q)) {
            bonus = qMax(bonus, 140);
        } else if (k.contains(q)) {
            bonus = qMax(bonus, 80);
        }
    }
    return bonus;
}

QVariantList queryCapabilitySearchActions(const QString &needle, int limit)
{
    QDBusInterface iface(QString::fromLatin1(kCapabilityService),
                         QString::fromLatin1(kCapabilityPath),
                         QString::fromLatin1(kCapabilityIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return {};
    }
    QVariantMap context{
        {QStringLiteral("scope"), QStringLiteral("tothespot")},
        {QStringLiteral("selection_count"), 0},
        {QStringLiteral("source_app"), QStringLiteral("org.slm.tothespot")},
        {QStringLiteral("text"), needle},
    };
    QDBusReply<QVariantList> reply = iface.call(QStringLiteral("SearchActions"), needle, context);
    if (!reply.isValid()) {
        return {};
    }
    QVariantList rows = reply.value();
    if (rows.size() > limit) {
        rows = rows.mid(0, limit);
    }
    return rows;
}

QVariantList localSearchActionsFromDesktopEntries(const QString &needle, int limit)
{
    const QString query = needle.trimmed();
    if (query.isEmpty()) {
        return {};
    }
    
    Slm::Actions::ActionRegistry localRegistry;
    localRegistry.reload();
    Slm::Actions::Framework::SlmActionFramework framework(&localRegistry);
    
    QVariantMap context{
        {QStringLiteral("scope"), QStringLiteral("tothespot")},
        {QStringLiteral("selection_count"), 0},
        {QStringLiteral("source_app"), QStringLiteral("org.slm.tothespot")},
        {QStringLiteral("text"), needle},
    };
    
    QVariantList rows = framework.searchActions(needle, context);
    if (rows.size() > limit) {
        rows = rows.mid(0, limit);
    }
    return rows;
}
}

TothespotService::TothespotService(QObject *parent)
    : QObject(parent)
    , m_auditLogger(&m_permissionStore)
{
    setupSecurity();
    qRegisterMetaType<SearchResultEntry>("SearchResultEntry");
    qRegisterMetaType<SearchResultList>("SearchResultList");
    qDBusRegisterMetaType<SearchResultEntry>();
    qDBusRegisterMetaType<SearchResultList>();

    QDBusConnection::sessionBus().connect(QString::fromLatin1(kSearchService),
                                          QString::fromLatin1(kSearchPath),
                                          QString::fromLatin1(kSearchIface),
                                          QStringLiteral("SearchProfileChanged"),
                                          this,
                                          SLOT(handleSearchProfileChanged(QString)));
}

bool TothespotService::ready() const
{
    return true;
}

QVariantList TothespotService::query(const QString &text, const QVariantMap &options, int limit)
{
    if (!checkPermission(Slm::Permissions::Capability::SearchQueryApps, QStringLiteral("Query"))) {
        return {};
    }
    const QString needle = text.trimmed();

    // Primary source: global search service.
    QDBusInterface iface(QString::fromLatin1(kSearchService),
                         QString::fromLatin1(kSearchPath),
                         QString::fromLatin1(kSearchIface),
                         QDBusConnection::sessionBus());
    if (iface.isValid()) {
        iface.setTimeout(kSearchTimeoutMs);
        QVariantMap queryOpts = options;
        queryOpts.insert(QStringLiteral("limit"), qBound(1, limit, 256));
        queryOpts.insert(QStringLiteral("includeApps"), true);
        queryOpts.insert(QStringLiteral("includeRecent"), true);
        queryOpts.insert(QStringLiteral("includeTracker"), true);
        queryOpts.insert(QStringLiteral("includeActions"), true);
        queryOpts.insert(QStringLiteral("includeSettings"), true);
        queryOpts.insert(QStringLiteral("includePreview"), true);
        queryOpts.insert(QStringLiteral("scope"), QStringLiteral("tothespot"));
        queryOpts.insert(QStringLiteral("source_app"), QStringLiteral("org.slm.tothespot"));
        QDBusReply<SearchResultList> reply = iface.call(QStringLiteral("Query"), needle, queryOpts);
        if (reply.isValid()) {
            QVariantList out;
            const SearchResultList rows = reply.value();
            out.reserve(rows.size());
            for (const SearchResultEntry &entry : rows) {
                const QString resultId = entry.id;
                const QVariantMap meta = entry.metadata;
                if (resultId.isEmpty() || meta.isEmpty()) {
                    continue;
                }
                const QString provider = meta.value(QStringLiteral("provider")).toString();
                const QString title = meta.value(QStringLiteral("title")).toString();
                const QString subtitle = meta.value(QStringLiteral("subtitle")).toString();
                const QString path = meta.value(QStringLiteral("path")).toString();
                const QString iconRaw = meta.value(QStringLiteral("icon")).toString().trimmed();
                const bool iconIsUrlOrPath = ResourcePaths::Url::isExplicitIconSource(iconRaw);
                QVariantMap row;
                row.insert(QStringLiteral("resultId"), resultId);
                row.insert(QStringLiteral("provider"), provider);
                QString resultType = QStringLiteral("path");
                if (provider == QStringLiteral("apps")) {
                    resultType = QStringLiteral("app");
                } else if (provider == QStringLiteral("slm_actions")) {
                    resultType = QStringLiteral("action");
                } else if (provider == QStringLiteral("settings")) {
                    resultType = QStringLiteral("settings");
                }
                row.insert(QStringLiteral("type"), resultType);
                row.insert(QStringLiteral("isAction"), resultType == QStringLiteral("action"));
                row.insert(QStringLiteral("resultKind"),
                           resultType == QStringLiteral("action")
                               ? QStringLiteral("action")
                               : (resultType == QStringLiteral("app")
                                      ? QStringLiteral("app")
                                      : (resultType == QStringLiteral("settings")
                                             ? QStringLiteral("settings")
                                      : (meta.value(QStringLiteral("isDir")).toBool()
                                             ? QStringLiteral("folder")
                                             : QStringLiteral("file")))));
                row.insert(QStringLiteral("name"), title);
                row.insert(QStringLiteral("path"), subtitle.isEmpty() ? path : subtitle);
                row.insert(QStringLiteral("absolutePath"), path);
                row.insert(QStringLiteral("isDir"), meta.value(QStringLiteral("isDir")).toBool());
                row.insert(QStringLiteral("desktopId"), meta.value(QStringLiteral("desktopId")).toString());
                row.insert(QStringLiteral("desktopFile"), meta.value(QStringLiteral("desktopFile")).toString());
                row.insert(QStringLiteral("executable"), meta.value(QStringLiteral("executable")).toString());
                row.insert(QStringLiteral("deepLink"), meta.value(QStringLiteral("deepLink")).toString());
                row.insert(QStringLiteral("intent"), meta.value(QStringLiteral("subtitle")).toString());
                row.insert(QStringLiteral("actionId"), meta.value(QStringLiteral("actionId")).toString());
                row.insert(QStringLiteral("iconName"), iconIsUrlOrPath ? QString() : iconRaw);
                row.insert(QStringLiteral("iconSource"), iconIsUrlOrPath ? iconRaw : QString());
                row.insert(QStringLiteral("score"), meta.value(QStringLiteral("score")).toInt());
                out.push_back(row);
            }
            if (!out.isEmpty()) {
                return out;
            }
            qInfo().noquote() << "[slm-search] global-query-empty query=" << needle
                              << "fallback=enabled";
        }
    }

    const auto materializeActionRows = [&](const QVariantList &rows, const QString &tag) -> QVariantList {
        QVariantList out;
        out.reserve(rows.size());
        int invalidRows = 0;
        for (const QVariant &entryVar : rows) {
            const QVariantMap meta = entryVar.toMap();
            if (meta.isEmpty()) {
                ++invalidRows;
                continue;
            }
            QVariantMap row;
            row.insert(QStringLiteral("resultId"),
                       QStringLiteral("slm-action:") + meta.value(QStringLiteral("id")).toString());
            row.insert(QStringLiteral("actionId"), meta.value(QStringLiteral("id")).toString());
            row.insert(QStringLiteral("desktopId"), meta.value(QStringLiteral("desktopId")).toString());
            row.insert(QStringLiteral("provider"), QStringLiteral("slm_actions"));
            row.insert(QStringLiteral("type"), QStringLiteral("action"));
            row.insert(QStringLiteral("name"), meta.value(QStringLiteral("name")).toString());
            row.insert(QStringLiteral("path"),
                       meta.value(QStringLiteral("searchAction")).toMap()
                           .value(QStringLiteral("intent")).toString());
            row.insert(QStringLiteral("absolutePath"), QString());
            const QString iconRaw = meta.value(QStringLiteral("icon")).toString().trimmed();
            const bool iconIsUrlOrPath = ResourcePaths::Url::isExplicitIconSource(iconRaw);
            row.insert(QStringLiteral("iconName"), iconIsUrlOrPath ? QString() : iconRaw);
            row.insert(QStringLiteral("iconSource"), iconIsUrlOrPath ? iconRaw : QString());
            row.insert(QStringLiteral("score"), meta.value(QStringLiteral("score")).toInt());
            out.push_back(row);
        }
        if (!out.isEmpty()) {
            const QVariantMap first = out.first().toMap();
            qInfo().noquote() << "[slm-search] tothespot-fallback-return source=" << tag
                              << "query=" << needle
                              << "count=" << out.size()
                              << "firstName=" << first.value(QStringLiteral("name")).toString()
                              << "firstType=" << first.value(QStringLiteral("type")).toString()
                              << "firstProvider=" << first.value(QStringLiteral("provider")).toString();
        }
        if (invalidRows > 0) {
            qInfo().noquote() << "[slm-search] tothespot-fallback-invalid source=" << tag
                              << "query=" << needle
                              << "invalidRows=" << invalidRows;
        }
        return out;
    };

    QVariantList actionRows = queryCapabilitySearchActions(needle, qBound(1, limit, 256));
    if (!actionRows.isEmpty()) {
        qInfo().noquote() << "[slm-search] tothespot-fallback query=" << needle
                          << "source=capability-service"
                          << "count=" << actionRows.size();
    }
    QVariantList out = materializeActionRows(actionRows, QStringLiteral("capability-service"));
    if (out.isEmpty()) {
        actionRows = localSearchActionsFromDesktopEntries(needle, qBound(1, limit, 256));
        if (!actionRows.isEmpty()) {
            qInfo().noquote() << "[slm-search] tothespot-fallback query=" << needle
                              << "source=local-desktop-scan"
                              << "count=" << actionRows.size();
        }
        out = materializeActionRows(actionRows, QStringLiteral("local-desktop-scan"));
    }
    if (!out.isEmpty()) {
        return out;
    }

    return {};
}

bool TothespotService::activateResult(const QString &id, const QVariantMap &activateData)
{
    if (!checkPermission(Slm::Permissions::Capability::QuickActionInvoke, QStringLiteral("ActivateResult"))) {
        return false;
    }
    const QString rid = id.trimmed();
    if (rid.isEmpty()) {
        return false;
    }
    
    QDBusInterface iface(QString::fromLatin1(kSearchService),
                         QString::fromLatin1(kSearchPath),
                         QString::fromLatin1(kSearchIface),
                         QDBusConnection::sessionBus());
    if (iface.isValid()) {
        iface.setTimeout(kSearchTimeoutMs);
        QDBusReply<void> reply = iface.call(QStringLiteral("ActivateResult"), rid, activateData);
        if (reply.isValid()) {
            return true;
        }
    }

    if (rid.startsWith(QStringLiteral("slm-action:"))) {
        const QString actionId = rid.mid(11); // len("slm-action:") == 11
        Slm::Actions::ActionRegistry localRegistry;
        localRegistry.reload();
        Slm::Actions::Framework::SlmActionFramework framework(&localRegistry);
        
        QVariantMap invokeCtx = activateData;
        if (!invokeCtx.contains(QStringLiteral("scope"))) {
            invokeCtx.insert(QStringLiteral("scope"), QStringLiteral("tothespot"));
        }
        
        QVariantMap reply = framework.invokeAction(actionId, invokeCtx);
        if (reply.value(QStringLiteral("ok")).toBool()) {
            qInfo().noquote() << "[slm-search] tothespot-fallback-invoke action=" << actionId << "success";
            return true;
        }
        qWarning().noquote() << "[slm-search] tothespot-fallback-invoke action=" << actionId << "failed" 
                             << reply.value(QStringLiteral("error")).toString();
    }
    
    return false;
}

QVariantMap TothespotService::previewResult(const QString &id)
{
    const QString rid = id.trimmed();
    if (rid.isEmpty()) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("invalid-id")}};
    }
    QDBusInterface iface(QString::fromLatin1(kSearchService),
                         QString::fromLatin1(kSearchPath),
                         QString::fromLatin1(kSearchIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("service-unavailable")}};
    }
    iface.setTimeout(kSearchTimeoutMs);
    QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("PreviewResult"), rid);
    if (!reply.isValid()) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("dbus-error")}};
    }
    return reply.value();
}

QVariantMap TothespotService::resolveClipboardResult(const QString &id,
                                                     const QVariantMap &resolveData)
{
    if (!checkPermission(Slm::Permissions::Capability::SearchResolveClipboardResult, QStringLiteral("ResolveClipboardResult"))) {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("permission-denied")}};
    }
    const QString rid = id.trimmed();
    if (rid.isEmpty()) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("invalid-id")}};
    }
    QDBusInterface iface(QString::fromLatin1(kSearchService),
                         QString::fromLatin1(kSearchPath),
                         QString::fromLatin1(kSearchIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("service-unavailable")}};
    }
    iface.setTimeout(kSearchTimeoutMs);
    QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("ResolveClipboardResult"),
                                               rid,
                                               resolveData);
    if (!reply.isValid()) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("dbus-error")}};
    }
    return reply.value();
}

QVariantMap TothespotService::configureTrackerPreset(const QVariantMap &preset)
{
    if (!checkPermission(Slm::Permissions::Capability::SearchConfigure, QStringLiteral("ConfigureTrackerPreset"))) {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("permission-denied")}};
    }
    QDBusInterface iface(QString::fromLatin1(kSearchService),
                         QString::fromLatin1(kSearchPath),
                         QString::fromLatin1(kSearchIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("service-unavailable")}};
    }
    iface.setTimeout(kSearchTimeoutMs);
    QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("ConfigureTrackerPreset"), preset);
    return reply.isValid() ? reply.value() : QVariantMap{
        {QStringLiteral("ok"), false},
        {QStringLiteral("error"), QStringLiteral("dbus-error")}
    };
}

QVariantMap TothespotService::resetTrackerPreset()
{
    if (!checkPermission(Slm::Permissions::Capability::SearchConfigure, QStringLiteral("ResetTrackerPreset"))) {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("permission-denied")}};
    }
    QDBusInterface iface(QString::fromLatin1(kSearchService),
                         QString::fromLatin1(kSearchPath),
                         QString::fromLatin1(kSearchIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("service-unavailable")}};
    }
    iface.setTimeout(kSearchTimeoutMs);
    QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("ResetTrackerPreset"));
    return reply.isValid() ? reply.value() : QVariantMap{
        {QStringLiteral("ok"), false},
        {QStringLiteral("error"), QStringLiteral("dbus-error")}
    };
}

QVariantMap TothespotService::trackerPresetStatus()
{
    QDBusInterface iface(QString::fromLatin1(kSearchService),
                         QString::fromLatin1(kSearchPath),
                         QString::fromLatin1(kSearchIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("service-unavailable")}};
    }
    iface.setTimeout(kSearchTimeoutMs);
    QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("TrackerPresetStatus"));
    return reply.isValid() ? reply.value() : QVariantMap{
        {QStringLiteral("ok"), false},
        {QStringLiteral("error"), QStringLiteral("dbus-error")}
    };
}

QVariantList TothespotService::searchProfiles()
{
    QDBusInterface iface(QString::fromLatin1(kSearchService),
                         QString::fromLatin1(kSearchPath),
                         QString::fromLatin1(kSearchIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return {};
    }
    iface.setTimeout(kSearchTimeoutMs);
    QDBusReply<QVariantList> reply = iface.call(QStringLiteral("GetSearchProfiles"));
    return reply.isValid() ? reply.value() : QVariantList{};
}

QString TothespotService::activeSearchProfile()
{
    QDBusInterface iface(QString::fromLatin1(kSearchService),
                         QString::fromLatin1(kSearchPath),
                         QString::fromLatin1(kSearchIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return QStringLiteral("balanced");
    }
    iface.setTimeout(kSearchTimeoutMs);
    QDBusReply<QString> reply = iface.call(QStringLiteral("GetActiveSearchProfile"));
    if (!reply.isValid()) {
        return QStringLiteral("balanced");
    }
    const QString profile = reply.value().trimmed().toLower();
    if (profile == QStringLiteral("apps-first") ||
        profile == QStringLiteral("files-first") ||
        profile == QStringLiteral("balanced")) {
        return profile;
    }
    return QStringLiteral("balanced");
}

QVariantMap TothespotService::activeSearchProfileMeta()
{
    QDBusInterface iface(QString::fromLatin1(kSearchService),
                         QString::fromLatin1(kSearchPath),
                         QString::fromLatin1(kSearchIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("service-unavailable")},
            {QStringLiteral("profileId"), QStringLiteral("balanced")},
        };
    }
    iface.setTimeout(kSearchTimeoutMs);
    QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("GetActiveSearchProfileMeta"));
    if (!reply.isValid()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("dbus-error")},
            {QStringLiteral("profileId"), QStringLiteral("balanced")},
        };
    }
    return reply.value();
}

bool TothespotService::setActiveSearchProfile(const QString &profileId)
{
    if (!checkPermission(Slm::Permissions::Capability::SearchConfigure, QStringLiteral("SetActiveSearchProfile"))) {
        return false;
    }
    QDBusInterface iface(QString::fromLatin1(kSearchService),
                         QString::fromLatin1(kSearchPath),
                         QString::fromLatin1(kSearchIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return false;
    }
    iface.setTimeout(kSearchTimeoutMs);
    QDBusReply<bool> reply = iface.call(QStringLiteral("SetActiveSearchProfile"), profileId);
    return reply.isValid() && reply.value();
}

QVariantMap TothespotService::telemetryMeta()
{
    if (!checkPermission(Slm::Permissions::Capability::SearchReadTelemetry, QStringLiteral("GetTelemetryMeta"))) {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("permission-denied")}};
    }
    QDBusInterface iface(QString::fromLatin1(kSearchService),
                         QString::fromLatin1(kSearchPath),
                         QString::fromLatin1(kSearchIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("service-unavailable")}
        };
    }
    iface.setTimeout(kSearchTimeoutMs);
    QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("GetTelemetryMeta"));
    return reply.isValid() ? reply.value() : QVariantMap{
        {QStringLiteral("ok"), false},
        {QStringLiteral("error"), QStringLiteral("dbus-error")}
    };
}

QVariantList TothespotService::activationTelemetry(int limit)
{
    if (!checkPermission(Slm::Permissions::Capability::SearchReadTelemetry, QStringLiteral("GetActivationTelemetry"))) {
        return {};
    }
    QDBusInterface iface(QString::fromLatin1(kSearchService),
                         QString::fromLatin1(kSearchPath),
                         QString::fromLatin1(kSearchIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return {};
    }
    iface.setTimeout(kSearchTimeoutMs);
    QDBusReply<QVariantList> reply = iface.call(QStringLiteral("GetActivationTelemetry"),
                                                qBound(1, limit, 1000));
    return reply.isValid() ? reply.value() : QVariantList{};
}

bool TothespotService::resetActivationTelemetry()
{
    if (!checkPermission(Slm::Permissions::Capability::SearchReadTelemetry, QStringLiteral("ResetActivationTelemetry"))) {
        return false;
    }
    QDBusInterface iface(QString::fromLatin1(kSearchService),
                         QString::fromLatin1(kSearchPath),
                         QString::fromLatin1(kSearchIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return false;
    }
    iface.setTimeout(kSearchTimeoutMs);
    QDBusReply<bool> reply = iface.call(QStringLiteral("ResetActivationTelemetry"));
    return reply.isValid() && reply.value();
}

void TothespotService::handleSearchProfileChanged(const QString &profileId)
{
    emit searchProfileChanged(profileId);
}

void TothespotService::setupSecurity()
{
    if (!m_permissionStore.open()) {
        qWarning() << "[tothespot] failed to open permission store";
    }
    m_policyEngine.setStore(&m_permissionStore);
    m_permissionBroker.setStore(&m_permissionStore);
    m_permissionBroker.setPolicyEngine(&m_policyEngine);
    m_permissionBroker.setAuditLogger(&m_auditLogger);
    m_securityGuard.setTrustResolver(&m_trustResolver);
    m_securityGuard.setPermissionBroker(&m_permissionBroker);

    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.Tothespot"), QStringLiteral("Query"), Slm::Permissions::Capability::SearchQueryApps);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.Tothespot"), QStringLiteral("ActivateResult"), Slm::Permissions::Capability::QuickActionInvoke);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.Tothespot"), QStringLiteral("ResolveClipboardResult"), Slm::Permissions::Capability::SearchResolveClipboardResult);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.Tothespot"), QStringLiteral("ConfigureTrackerPreset"), Slm::Permissions::Capability::SearchConfigure);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.Tothespot"), QStringLiteral("ResetTrackerPreset"), Slm::Permissions::Capability::SearchConfigure);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.Tothespot"), QStringLiteral("SetActiveSearchProfile"), Slm::Permissions::Capability::SearchConfigure);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.Tothespot"), QStringLiteral("GetTelemetryMeta"), Slm::Permissions::Capability::SearchReadTelemetry);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.Tothespot"), QStringLiteral("GetActivationTelemetry"), Slm::Permissions::Capability::SearchReadTelemetry);
    m_securityGuard.registerMethodCapability(QStringLiteral("org.slm.Tothespot"), QStringLiteral("ResetActivationTelemetry"), Slm::Permissions::Capability::SearchReadTelemetry);
}

bool TothespotService::checkPermission(Slm::Permissions::Capability capability, const QString &methodName)
{
    if (!calledFromDBus()) {
        return true;
    }

    QVariantMap ctx;
    ctx.insert(QStringLiteral("method"), methodName);
    ctx.insert(QStringLiteral("sensitivityLevel"), QStringLiteral("Low"));

    const Slm::Permissions::PolicyDecision d = m_securityGuard.check(message(), capability, ctx);
    if (d.type == Slm::Permissions::DecisionType::Allow) {
        return true;
    }

    sendErrorReply(QStringLiteral("org.slm.Desktop.Error.PermissionDenied"),
                   QStringLiteral("Permission denied for method %1: %2")
                       .arg(methodName)
                       .arg(d.reason));
    return false;
}
