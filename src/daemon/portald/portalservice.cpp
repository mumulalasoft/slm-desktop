#include "portalservice.h"

#include "../../../dbuslogutils.h"
#include "portalmanager.h"
#include "portal-adapter/PortalBackendService.h"
#include "../../../portalmethodnames.h"
#include "../../../portalresponsebuilder.h"
#include "../../../portalvalidation.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QMetaObject>
#include <QRegularExpression>
#include <QSet>

namespace {
constexpr const char kService[] = "org.slm.Desktop.Portal";
constexpr const char kPath[] = "/org/slm/Desktop/Portal";
constexpr const char kApiVersion[] = "1.0";
constexpr const char kSearchService[] = "org.slm.Desktop.Search.v1";
constexpr const char kSearchPath[] = "/org/slm/Desktop/Search";
constexpr const char kSearchIface[] = "org.slm.Desktop.Search.v1";
constexpr const char kCapabilityService[] = "org.freedesktop.SLMCapabilities";
constexpr const char kCapabilityPath[] = "/org/freedesktop/SLMCapabilities";
constexpr const char kCapabilityIface[] = "org.freedesktop.SLMCapabilities";

QVariantList decodeSearchEntries(const QVariant &value)
{
    QVariantList rows;
    if (value.metaType().id() == qMetaTypeId<QDBusArgument>()) {
        const QDBusArgument arg = value.value<QDBusArgument>();
        arg.beginArray();
        while (!arg.atEnd()) {
            QString id;
            QVariantMap metadata;
            arg.beginStructure();
            arg >> id >> metadata;
            arg.endStructure();
            QVariantMap row;
            row.insert(QStringLiteral("id"), id);
            row.insert(QStringLiteral("metadata"), metadata);
            rows.push_back(row);
        }
        arg.endArray();
        return rows;
    }
    const QVariantList asList = value.toList();
    for (const QVariant &item : asList) {
        if (item.metaType().id() == qMetaTypeId<QDBusArgument>()) {
            const QDBusArgument entryArg = item.value<QDBusArgument>();
            QString id;
            QVariantMap metadata;
            entryArg.beginStructure();
            entryArg >> id >> metadata;
            entryArg.endStructure();
            QVariantMap row;
            row.insert(QStringLiteral("id"), id);
            row.insert(QStringLiteral("metadata"), metadata);
            rows.push_back(row);
        } else if (item.canConvert<QVariantMap>()) {
            rows.push_back(item.toMap());
        }
    }
    return rows;
}

QVariantMap toSummaryRow(const QVariantMap &entry)
{
    const QVariantMap metadata = entry.value(QStringLiteral("metadata")).toMap();
    QVariantMap row;
    row.insert(QStringLiteral("id"), entry.value(QStringLiteral("id")).toString());
    row.insert(QStringLiteral("provider"), metadata.value(QStringLiteral("provider")).toString());
    row.insert(QStringLiteral("title"), metadata.value(QStringLiteral("title")).toString());
    row.insert(QStringLiteral("subtitle"), metadata.value(QStringLiteral("subtitle")).toString());
    row.insert(QStringLiteral("icon"), metadata.value(QStringLiteral("icon")).toString());
    row.insert(QStringLiteral("resultType"), metadata.value(QStringLiteral("resultType")).toString());
    row.insert(QStringLiteral("timestamp"), metadata.value(QStringLiteral("timestamp")).toString());
    return row;
}

QString toOneLine(const QString &raw, int maxLen = 96)
{
    QString text = raw;
    text.replace(QRegularExpression(QStringLiteral("[\\r\\n\\t]+")), QStringLiteral(" "));
    text = text.simplified();
    if (maxLen > 3 && text.size() > maxLen) {
        text = text.left(maxLen - 3) + QStringLiteral("...");
    }
    return text;
}

QVariantMap toClipboardSummaryRow(const QVariantMap &entry)
{
    const QVariantMap metadata = entry.value(QStringLiteral("metadata")).toMap();
    const QVariantMap preview = metadata.value(QStringLiteral("preview")).toMap();

    const QString id = entry.value(QStringLiteral("id")).toString().trimmed();
    const QString itemType = preview.value(QStringLiteral("contentType")).toString().trimmed();
    const bool isSensitive = preview.value(QStringLiteral("isSensitive")).toBool()
                             || metadata.value(QStringLiteral("isSensitive")).toBool();
    QString previewText = preview.value(QStringLiteral("preview")).toString();
    if (previewText.trimmed().isEmpty()) {
        previewText = metadata.value(QStringLiteral("title")).toString();
    }
    if (isSensitive) {
        previewText = QStringLiteral("Sensitive item");
    }
    previewText = toOneLine(previewText, 80);
    const qlonglong tsBucket = preview.value(QStringLiteral("timestampBucket")).toLongLong();

    QVariantMap row;
    row.insert(QStringLiteral("id"), id);
    row.insert(QStringLiteral("provider"), QStringLiteral("clipboard"));
    row.insert(QStringLiteral("title"), previewText);
    row.insert(QStringLiteral("subtitle"), itemType.isEmpty() ? QStringLiteral("clipboard") : itemType);
    row.insert(QStringLiteral("icon"), metadata.value(QStringLiteral("icon")).toString());
    row.insert(QStringLiteral("itemType"), itemType);
    row.insert(QStringLiteral("previewText"), previewText);
    row.insert(QStringLiteral("isSensitive"), isSensitive);
    row.insert(QStringLiteral("timestampBucket"), tsBucket);
    return row;
}
}

PortalService::PortalService(PortalManager *manager,
                             QObject *backend,
                             QObject *parent)
    : QObject(parent)
    , m_manager(manager)
    , m_backend(backend)
{
    registerDbusService();
}

PortalService::~PortalService()
{
    if (!m_serviceRegistered) {
        return;
    }
    QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(kPath));
    QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(kService));
}

bool PortalService::serviceRegistered() const
{
    return m_serviceRegistered;
}

QString PortalService::apiVersion() const
{
    return QString::fromLatin1(kApiVersion);
}

QVariantMap PortalService::Ping() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("service"), QString::fromLatin1(kService)},
        {QStringLiteral("registered"), m_serviceRegistered},
        {QStringLiteral("api_version"), apiVersion()},
    };
}

QVariantMap PortalService::GetCapabilities() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("service"), QString::fromLatin1(kService)},
        {QStringLiteral("api_version"), apiVersion()},
        {QStringLiteral("capabilities"), QStringList{
             QStringLiteral("screenshot"),
             QStringLiteral("screencast"),
             QStringLiteral("share"),
             QStringLiteral("global_shortcuts"),
             QStringLiteral("portal_sessions"),
             QStringLiteral("file_chooser"),
             QStringLiteral("open_uri"),
             QStringLiteral("pick_color"),
             QStringLiteral("pick_folder"),
             QStringLiteral("wallpaper"),
        }},
    };
}

QVariantMap PortalService::Screenshot(const QVariantMap &options)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    const QVariantMap result = m_manager ? m_manager->Screenshot(options)
                                         : SlmPortalResponseBuilder::serviceUnavailable(
                                               QString::fromLatin1(SlmPortalMethod::kScreenshot));
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                         QString::fromLatin1(SlmPortalMethod::kScreenshot),
                         requestId,
                         caller,
                         QString(),
                         QStringLiteral("finish"),
                         {{QStringLiteral("ok"), result.value(QStringLiteral("ok")).toBool()},
                          {QStringLiteral("error"), result.value(QStringLiteral("error")).toString()}});
    return result;
}

QVariantMap PortalService::ScreenCast(const QVariantMap &options)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    const QVariantMap result = m_manager ? m_manager->ScreenCast(options)
                                         : SlmPortalResponseBuilder::serviceUnavailable(
                                               QString::fromLatin1(SlmPortalMethod::kScreenCast));
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                         QString::fromLatin1(SlmPortalMethod::kScreenCast),
                         requestId,
                         caller,
                         QString(),
                         QStringLiteral("finish"),
                         {{QStringLiteral("ok"), result.value(QStringLiteral("ok")).toBool()},
                          {QStringLiteral("error"), result.value(QStringLiteral("error")).toString()}});
    return result;
}

QVariantMap PortalService::FileChooser(const QVariantMap &options)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    const QString mode = options.value(QStringLiteral("mode")).toString().trimmed();
    QVariantMap result;
    if (!SlmPortalValidation::isValidFileChooserMode(mode)) {
        result = SlmPortalResponseBuilder::invalidArgument(
            QString::fromLatin1(SlmPortalMethod::kFileChooser),
                                                           {{QStringLiteral("options"), options}});
    } else {
        result = m_manager ? m_manager->FileChooser(options)
                           : SlmPortalResponseBuilder::serviceUnavailable(
                                 QString::fromLatin1(SlmPortalMethod::kFileChooser));
    }
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                         QString::fromLatin1(SlmPortalMethod::kFileChooser),
                         requestId,
                         caller,
                         QString(),
                         QStringLiteral("finish"),
                         {{QStringLiteral("ok"), result.value(QStringLiteral("ok")).toBool()},
                          {QStringLiteral("error"), result.value(QStringLiteral("error")).toString()}});
    return result;
}

QVariantMap PortalService::OpenURI(const QString &uri, const QVariantMap &options)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    QVariantMap result;
    if (uri.trimmed().isEmpty()) {
        result = SlmPortalResponseBuilder::invalidArgument(
            QString::fromLatin1(SlmPortalMethod::kOpenURI),
                                                           {{QStringLiteral("uri"), uri},
                                                            {QStringLiteral("options"), options}});
    } else {
        result = m_manager ? m_manager->OpenURI(uri, options)
                           : SlmPortalResponseBuilder::serviceUnavailable(
                                 QString::fromLatin1(SlmPortalMethod::kOpenURI));
    }
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                         QString::fromLatin1(SlmPortalMethod::kOpenURI),
                         requestId,
                         caller,
                         QString(),
                         QStringLiteral("finish"),
                         {{QStringLiteral("ok"), result.value(QStringLiteral("ok")).toBool()},
                          {QStringLiteral("error"), result.value(QStringLiteral("error")).toString()},
                          {QStringLiteral("uri"), uri}});
    return result;
}

QVariantMap PortalService::PickColor(const QVariantMap &options)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    const QVariantMap result = m_manager ? m_manager->PickColor(options)
                                         : SlmPortalResponseBuilder::serviceUnavailable(
                                               QString::fromLatin1(SlmPortalMethod::kPickColor));
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                         QString::fromLatin1(SlmPortalMethod::kPickColor),
                         requestId,
                         caller,
                         QString(),
                         QStringLiteral("finish"),
                         {{QStringLiteral("ok"), result.value(QStringLiteral("ok")).toBool()},
                          {QStringLiteral("error"), result.value(QStringLiteral("error")).toString()}});
    return result;
}

QVariantMap PortalService::PickFolder(const QVariantMap &options)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    QVariantMap result;
    if (SlmPortalValidation::hasPickFolderConflicts(options)) {
        result = SlmPortalResponseBuilder::invalidArgument(
            QString::fromLatin1(SlmPortalMethod::kPickFolder),
                                                           {{QStringLiteral("options"), options}});
    } else {
        result = m_manager ? m_manager->PickFolder(options)
                           : SlmPortalResponseBuilder::serviceUnavailable(
                                 QString::fromLatin1(SlmPortalMethod::kPickFolder));
    }
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                         QString::fromLatin1(SlmPortalMethod::kPickFolder),
                         requestId,
                         caller,
                         QString(),
                         QStringLiteral("finish"),
                         {{QStringLiteral("ok"), result.value(QStringLiteral("ok")).toBool()},
                          {QStringLiteral("error"), result.value(QStringLiteral("error")).toString()}});
    return result;
}

QVariantMap PortalService::Wallpaper(const QVariantMap &options)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    const QVariantMap result = m_manager ? m_manager->Wallpaper(options)
                                         : SlmPortalResponseBuilder::serviceUnavailable(
                                               QString::fromLatin1(SlmPortalMethod::kWallpaper));
    SlmDbusLog::logEvent(QString::fromLatin1(kService),
                         QString::fromLatin1(SlmPortalMethod::kWallpaper),
                         requestId,
                         caller,
                         QString(),
                         QStringLiteral("finish"),
                         {{QStringLiteral("ok"), result.value(QStringLiteral("ok")).toBool()},
                          {QStringLiteral("error"), result.value(QStringLiteral("error")).toString()}});
    return result;
}

QVariantMap PortalService::RequestClipboardPreview(const QVariantMap &options)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("RequestClipboardPreview"));
    }
    QVariantMap request = options;
    // Portal-facing clipboard preview path is summary-only by contract.
    request.insert(QStringLiteral("includeSensitive"), false);
    request.insert(QStringLiteral("initiatedFromOfficialUI"), false);
    request.insert(QStringLiteral("resourceType"), QStringLiteral("clipboard-preview"));
    if (!request.contains(QStringLiteral("resourceId"))) {
        request.insert(QStringLiteral("resourceId"), QStringLiteral("history"));
    }
    return callBackendRequest(QStringLiteral("RequestClipboardPreview"), request);
}

QVariantMap PortalService::RequestClipboardContent(const QVariantMap &options)
{
    const qlonglong clipboardId = options.value(QStringLiteral("clipboardId")).toLongLong();
    const QString resultId = options.value(QStringLiteral("resultId")).toString().trimmed();
    if (clipboardId <= 0 && resultId.isEmpty()) {
        return SlmPortalResponseBuilder::invalidArgument(
            QStringLiteral("RequestClipboardContent"),
            {{QStringLiteral("options"), options}});
    }
    if (!options.value(QStringLiteral("initiatedByUserGesture"), false).toBool()) {
        return SlmPortalResponseBuilder::permissionDenied(
            QStringLiteral("RequestClipboardContent"),
            {{QStringLiteral("reason"), QStringLiteral("user-gesture-required")}});
    }
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("RequestClipboardContent"));
    }
    QVariantMap request = options;
    request.insert(QStringLiteral("initiatedFromOfficialUI"), false);
    request.insert(QStringLiteral("resourceType"), QStringLiteral("clipboard-content"));
    if (!resultId.isEmpty()) {
        request.insert(QStringLiteral("resourceId"), resultId);
    } else {
        request.insert(QStringLiteral("resourceId"), QStringLiteral("clipboard:%1").arg(clipboardId));
    }
    return callBackendRequest(QStringLiteral("RequestClipboardContent"), request);
}

QVariantMap PortalService::ResolveClipboardResult(const QString &resultId, const QVariantMap &options)
{
    const QString id = resultId.trimmed();
    if (id.isEmpty()) {
        return SlmPortalResponseBuilder::invalidArgument(
            QStringLiteral("ResolveClipboardResult"),
            {{QStringLiteral("resultId"), resultId},
             {QStringLiteral("options"), options}});
    }
    if (!options.value(QStringLiteral("initiatedByUserGesture"), false).toBool()) {
        return SlmPortalResponseBuilder::permissionDenied(
            QStringLiteral("ResolveClipboardResult"),
            {{QStringLiteral("reason"), QStringLiteral("user-gesture-required")}});
    }
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("ResolveClipboardResult"));
    }
    QVariantMap request = options;
    request.insert(QStringLiteral("resultId"), id);
    request.insert(QStringLiteral("resourceType"), QStringLiteral("clipboard-search-resolve"));
    request.insert(QStringLiteral("resourceId"), id);
    request.insert(QStringLiteral("initiatedFromOfficialUI"), false);
    return callBackendRequest(QStringLiteral("ResolveClipboardResult"), request);
}

QVariantMap PortalService::QueryClipboardPreview(const QString &query, const QVariantMap &options)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("QueryClipboardPreview"));
    }
    QVariantMap authContext = options;
    authContext.insert(QStringLiteral("query"), query);
    authContext.insert(QStringLiteral("resourceType"), QStringLiteral("search-query"));
    authContext.insert(QStringLiteral("initiatedFromOfficialUI"), false);
    authContext.insert(QStringLiteral("includeSensitive"), false);
    const QVariantMap auth = callBackendDirect(QStringLiteral("QueryClipboardPreview"), authContext);
    if (!auth.value(QStringLiteral("ok")).toBool()) {
        return auth;
    }

    QDBusInterface iface(QString::fromLatin1(kSearchService),
                         QString::fromLatin1(kSearchPath),
                         QString::fromLatin1(kSearchIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("GlobalSearchService"));
    }

    QVariantMap queryOptions = options;
    queryOptions.insert(QStringLiteral("includePreview"), true);
    queryOptions.insert(QStringLiteral("initiatedFromOfficialUI"), false);
    queryOptions.insert(QStringLiteral("includeSensitive"), false);
    QDBusMessage reply = iface.call(QStringLiteral("Query"), query, queryOptions);
    if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("QueryClipboardPreview.Query"));
    }
    const QVariantList rows = decodeSearchEntries(reply.arguments().constFirst());
    QVariantList summaries;
    for (const QVariant &rowVar : rows) {
        const QVariantMap row = rowVar.toMap();
        const QVariantMap summary = toSummaryRow(row);
        if (summary.value(QStringLiteral("provider")).toString() == QStringLiteral("clipboard")) {
            summaries.push_back(toClipboardSummaryRow(row));
        }
    }
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("error"), QString()},
        {QStringLiteral("method"), QStringLiteral("QueryClipboardPreview")},
        {QStringLiteral("results"), summaries},
        {QStringLiteral("count"), summaries.size()},
    };
}

QVariantMap PortalService::QueryFiles(const QString &query, const QVariantMap &options)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("QueryFiles"));
    }
    QVariantMap authContext = options;
    authContext.insert(QStringLiteral("query"), query);
    authContext.insert(QStringLiteral("resourceType"), QStringLiteral("search-query"));
    const QVariantMap auth = callBackendDirect(QStringLiteral("QueryFiles"), authContext);
    if (!auth.value(QStringLiteral("ok")).toBool()) {
        return auth;
    }

    QDBusInterface iface(QString::fromLatin1(kSearchService),
                         QString::fromLatin1(kSearchPath),
                         QString::fromLatin1(kSearchIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("GlobalSearchService"));
    }
    QVariantMap queryOptions = options;
    queryOptions.insert(QStringLiteral("includePreview"), true);
    QDBusMessage reply = iface.call(QStringLiteral("Query"), query, queryOptions);
    if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("QueryFiles.Query"));
    }
    const QVariantList rows = decodeSearchEntries(reply.arguments().constFirst());
    static const QSet<QString> providers{
        QStringLiteral("tracker"),
        QStringLiteral("recent_files"),
    };
    QVariantList summaries;
    for (const QVariant &rowVar : rows) {
        const QVariantMap row = rowVar.toMap();
        const QVariantMap summary = toSummaryRow(row);
        if (providers.contains(summary.value(QStringLiteral("provider")).toString())) {
            summaries.push_back(summary);
        }
    }
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("error"), QString()},
        {QStringLiteral("method"), QStringLiteral("QueryFiles")},
        {QStringLiteral("results"), summaries},
        {QStringLiteral("count"), summaries.size()},
    };
}

QVariantMap PortalService::QueryContacts(const QString &query, const QVariantMap &options)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("QueryContacts"));
    }
    QVariantMap authContext = options;
    authContext.insert(QStringLiteral("query"), query);
    authContext.insert(QStringLiteral("resourceType"), QStringLiteral("contacts-summary"));
    const QVariantMap auth = callBackendDirect(QStringLiteral("QueryContacts"), authContext);
    if (!auth.value(QStringLiteral("ok")).toBool()) {
        return auth;
    }

    QDBusInterface iface(QString::fromLatin1(kSearchService),
                         QString::fromLatin1(kSearchPath),
                         QString::fromLatin1(kSearchIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("GlobalSearchService"));
    }
    QVariantMap queryOptions = options;
    queryOptions.insert(QStringLiteral("includePreview"), false);
    QDBusMessage reply = iface.call(QStringLiteral("Query"), query, queryOptions);
    if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("QueryContacts.Query"));
    }
    const QVariantList rows = decodeSearchEntries(reply.arguments().constFirst());
    static const QSet<QString> providers{
        QStringLiteral("contacts"),
    };
    QVariantList summaries;
    for (const QVariant &rowVar : rows) {
        const QVariantMap row = rowVar.toMap();
        const QVariantMap summary = toSummaryRow(row);
        const QString provider = summary.value(QStringLiteral("provider")).toString();
        const QString resultType = summary.value(QStringLiteral("resultType")).toString().toLower();
        if (providers.contains(provider)
            || resultType.contains(QStringLiteral("contact"))) {
            summaries.push_back(summary);
        }
    }
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("error"), QString()},
        {QStringLiteral("method"), QStringLiteral("QueryContacts")},
        {QStringLiteral("results"), summaries},
        {QStringLiteral("count"), summaries.size()},
    };
}

QVariantMap PortalService::QueryEmailMetadata(const QString &query, const QVariantMap &options)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("QueryEmailMetadata"));
    }
    QVariantMap authContext = options;
    authContext.insert(QStringLiteral("query"), query);
    authContext.insert(QStringLiteral("resourceType"), QStringLiteral("mail-metadata-summary"));
    const QVariantMap auth = callBackendDirect(QStringLiteral("QueryEmailMetadata"), authContext);
    if (!auth.value(QStringLiteral("ok")).toBool()) {
        return auth;
    }

    QDBusInterface iface(QString::fromLatin1(kSearchService),
                         QString::fromLatin1(kSearchPath),
                         QString::fromLatin1(kSearchIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("GlobalSearchService"));
    }
    QVariantMap queryOptions = options;
    queryOptions.insert(QStringLiteral("includePreview"), false);
    QDBusMessage reply = iface.call(QStringLiteral("Query"), query, queryOptions);
    if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("QueryEmailMetadata.Query"));
    }
    const QVariantList rows = decodeSearchEntries(reply.arguments().constFirst());
    static const QSet<QString> providers{
        QStringLiteral("email"),
        QStringLiteral("mail"),
    };
    QVariantList summaries;
    for (const QVariant &rowVar : rows) {
        const QVariantMap row = rowVar.toMap();
        const QVariantMap summary = toSummaryRow(row);
        const QString provider = summary.value(QStringLiteral("provider")).toString();
        const QString resultType = summary.value(QStringLiteral("resultType")).toString().toLower();
        if (providers.contains(provider)
            || resultType.contains(QStringLiteral("mail"))
            || resultType.contains(QStringLiteral("email"))) {
            summaries.push_back(summary);
        }
    }
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("error"), QString()},
        {QStringLiteral("method"), QStringLiteral("QueryEmailMetadata")},
        {QStringLiteral("results"), summaries},
        {QStringLiteral("count"), summaries.size()},
    };
}

QVariantMap PortalService::QueryMailMetadata(const QString &query, const QVariantMap &options)
{
    QVariantMap out = QueryEmailMetadata(query, options);
    out.insert(QStringLiteral("method"), QStringLiteral("QueryMailMetadata"));
    return out;
}

QVariantMap PortalService::QueryCalendarEvents(const QString &query, const QVariantMap &options)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("QueryCalendarEvents"));
    }
    QVariantMap authContext = options;
    authContext.insert(QStringLiteral("query"), query);
    authContext.insert(QStringLiteral("resourceType"), QStringLiteral("calendar-summary"));
    const QVariantMap auth = callBackendDirect(QStringLiteral("QueryCalendarEvents"), authContext);
    if (!auth.value(QStringLiteral("ok")).toBool()) {
        return auth;
    }

    QDBusInterface iface(QString::fromLatin1(kSearchService),
                         QString::fromLatin1(kSearchPath),
                         QString::fromLatin1(kSearchIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("GlobalSearchService"));
    }
    QVariantMap queryOptions = options;
    queryOptions.insert(QStringLiteral("includePreview"), false);
    QDBusMessage reply = iface.call(QStringLiteral("Query"), query, queryOptions);
    if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("QueryCalendarEvents.Query"));
    }
    const QVariantList rows = decodeSearchEntries(reply.arguments().constFirst());
    static const QSet<QString> providers{
        QStringLiteral("calendar"),
    };
    QVariantList summaries;
    for (const QVariant &rowVar : rows) {
        const QVariantMap row = rowVar.toMap();
        const QVariantMap summary = toSummaryRow(row);
        const QString provider = summary.value(QStringLiteral("provider")).toString();
        const QString resultType = summary.value(QStringLiteral("resultType")).toString().toLower();
        if (providers.contains(provider)
            || resultType.contains(QStringLiteral("calendar"))
            || resultType.contains(QStringLiteral("event"))) {
            summaries.push_back(summary);
        }
    }
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("error"), QString()},
        {QStringLiteral("method"), QStringLiteral("QueryCalendarEvents")},
        {QStringLiteral("results"), summaries},
        {QStringLiteral("count"), summaries.size()},
    };
}

QVariantMap PortalService::ResolveContact(const QString &resultId, const QVariantMap &options)
{
    const QString id = resultId.trimmed();
    if (id.isEmpty()) {
        return SlmPortalResponseBuilder::invalidArgument(
            QStringLiteral("ResolveContact"),
            {{QStringLiteral("resultId"), resultId},
             {QStringLiteral("options"), options}});
    }
    if (!options.value(QStringLiteral("initiatedByUserGesture"), false).toBool()) {
        return SlmPortalResponseBuilder::permissionDenied(
            QStringLiteral("ResolveContact"),
            {{QStringLiteral("reason"), QStringLiteral("user-gesture-required")}});
    }
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("ResolveContact"));
    }
    QVariantMap request = options;
    request.insert(QStringLiteral("resultId"), id);
    request.insert(QStringLiteral("resourceType"), QStringLiteral("contact-detail"));
    request.insert(QStringLiteral("resourceId"), id);
    request.insert(QStringLiteral("initiatedFromOfficialUI"), false);
    return callBackendRequest(QStringLiteral("ResolveContact"), request);
}

QVariantMap PortalService::ResolveEmailBody(const QString &resultId, const QVariantMap &options)
{
    const QString id = resultId.trimmed();
    if (id.isEmpty()) {
        return SlmPortalResponseBuilder::invalidArgument(
            QStringLiteral("ResolveEmailBody"),
            {{QStringLiteral("resultId"), resultId},
             {QStringLiteral("options"), options}});
    }
    if (!options.value(QStringLiteral("initiatedByUserGesture"), false).toBool()) {
        return SlmPortalResponseBuilder::permissionDenied(
            QStringLiteral("ResolveEmailBody"),
            {{QStringLiteral("reason"), QStringLiteral("user-gesture-required")}});
    }
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("ResolveEmailBody"));
    }
    QVariantMap request = options;
    request.insert(QStringLiteral("resultId"), id);
    request.insert(QStringLiteral("resourceType"), QStringLiteral("mail-body"));
    request.insert(QStringLiteral("resourceId"), id);
    request.insert(QStringLiteral("initiatedFromOfficialUI"), false);
    return callBackendRequest(QStringLiteral("ResolveEmailBody"), request);
}

QVariantMap PortalService::ResolveCalendarEvent(const QString &resultId, const QVariantMap &options)
{
    const QString id = resultId.trimmed();
    if (id.isEmpty()) {
        return SlmPortalResponseBuilder::invalidArgument(
            QStringLiteral("ResolveCalendarEvent"),
            {{QStringLiteral("resultId"), resultId},
             {QStringLiteral("options"), options}});
    }
    if (!options.value(QStringLiteral("initiatedByUserGesture"), false).toBool()) {
        return SlmPortalResponseBuilder::permissionDenied(
            QStringLiteral("ResolveCalendarEvent"),
            {{QStringLiteral("reason"), QStringLiteral("user-gesture-required")}});
    }
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("ResolveCalendarEvent"));
    }
    QVariantMap request = options;
    request.insert(QStringLiteral("resultId"), id);
    request.insert(QStringLiteral("resourceType"), QStringLiteral("calendar-event-detail"));
    request.insert(QStringLiteral("resourceId"), id);
    request.insert(QStringLiteral("initiatedFromOfficialUI"), false);
    return callBackendRequest(QStringLiteral("ResolveCalendarEvent"), request);
}

QVariantMap PortalService::ReadContact(const QString &contactId, const QVariantMap &options)
{
    return ResolveContact(contactId, options);
}

QVariantMap PortalService::ReadCalendarEvent(const QString &eventId, const QVariantMap &options)
{
    return ResolveCalendarEvent(eventId, options);
}

QVariantMap PortalService::ReadMailBody(const QString &messageId, const QVariantMap &options)
{
    return ResolveEmailBody(messageId, options);
}

QVariantMap PortalService::PickContact(const QVariantMap &options)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("PickContact"));
    }
    QVariantMap request = options;
    request.insert(QStringLiteral("resourceType"), QStringLiteral("contact-picker"));
    request.insert(QStringLiteral("resourceId"), QStringLiteral("picker"));
    request.insert(QStringLiteral("initiatedFromOfficialUI"), false);
    return callBackendRequest(QStringLiteral("PickContact"), request);
}

QVariantMap PortalService::PickCalendarEvent(const QVariantMap &options)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("PickCalendarEvent"));
    }
    QVariantMap request = options;
    request.insert(QStringLiteral("resourceType"), QStringLiteral("calendar-picker"));
    request.insert(QStringLiteral("resourceId"), QStringLiteral("picker"));
    request.insert(QStringLiteral("initiatedFromOfficialUI"), false);
    return callBackendRequest(QStringLiteral("PickCalendarEvent"), request);
}

QVariantMap PortalService::QueryActions(const QString &query, const QVariantMap &options)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("QueryActions"));
    }
    QVariantMap authContext = options;
    authContext.insert(QStringLiteral("query"), query);
    authContext.insert(QStringLiteral("resourceType"), QStringLiteral("action-query"));
    const QVariantMap auth = callBackendDirect(QStringLiteral("QueryActions"), authContext);
    if (!auth.value(QStringLiteral("ok")).toBool()) {
        return auth;
    }

    QDBusInterface iface(QString::fromLatin1(kCapabilityService),
                         QString::fromLatin1(kCapabilityPath),
                         QString::fromLatin1(kCapabilityIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("SLMCapabilities"));
    }

    QVariantMap queryContext = options;
    queryContext.insert(QStringLiteral("scope"), QStringLiteral("launcher"));
    QDBusMessage reply = iface.call(QStringLiteral("SearchActions"), query, queryContext);
    if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("QueryActions.SearchActions"));
    }
    const QVariantList rows = reply.arguments().constFirst().toList();
    QVariantList summaries;
    summaries.reserve(rows.size());
    for (const QVariant &value : rows) {
        const QVariantMap row = value.toMap();
        const QVariantMap summary{
            {QStringLiteral("id"), row.value(QStringLiteral("id")).toString()},
            {QStringLiteral("name"), row.value(QStringLiteral("name")).toString()},
            {QStringLiteral("icon"), row.value(QStringLiteral("icon")).toString()},
            {QStringLiteral("score"), row.value(QStringLiteral("score")).toInt()},
            {QStringLiteral("intent"), row.value(QStringLiteral("intent")).toString()},
            {QStringLiteral("group"), row.value(QStringLiteral("group")).toString()},
        };
        summaries.push_back(summary);
    }
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("error"), QString()},
        {QStringLiteral("method"), QStringLiteral("QueryActions")},
        {QStringLiteral("results"), summaries},
        {QStringLiteral("count"), summaries.size()},
    };
}

QVariantMap PortalService::QueryContextActions(const QString &target,
                                               const QVariantList &uris,
                                               const QVariantMap &options)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("QueryContextActions"));
    }
    QVariantMap authContext = options;
    authContext.insert(QStringLiteral("target"), target);
    authContext.insert(QStringLiteral("uris"), uris);
    authContext.insert(QStringLiteral("resourceType"), QStringLiteral("context-action-query"));
    const QVariantMap auth = callBackendDirect(QStringLiteral("QueryContextActions"), authContext);
    if (!auth.value(QStringLiteral("ok")).toBool()) {
        return auth;
    }

    QDBusInterface iface(QString::fromLatin1(kCapabilityService),
                         QString::fromLatin1(kCapabilityPath),
                         QString::fromLatin1(kCapabilityIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("SLMCapabilities"));
    }

    QVariantMap context = options;
    context.insert(QStringLiteral("target"), target);
    context.insert(QStringLiteral("uris"), uris);
    context.insert(QStringLiteral("selection_count"), uris.size());
    QDBusMessage reply = iface.call(QStringLiteral("ListActionsWithContext"),
                                    QStringLiteral("ContextMenu"),
                                    context);
    if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("QueryContextActions.ListActions"));
    }

    const QVariantList rows = reply.arguments().constFirst().toList();
    QVariantList summaries;
    summaries.reserve(rows.size());
    for (const QVariant &value : rows) {
        const QVariantMap row = value.toMap();
        const QVariantMap summary{
            {QStringLiteral("id"), row.value(QStringLiteral("id")).toString()},
            {QStringLiteral("name"), row.value(QStringLiteral("name")).toString()},
            {QStringLiteral("icon"), row.value(QStringLiteral("icon")).toString()},
            {QStringLiteral("group"), row.value(QStringLiteral("group")).toString()},
        };
        summaries.push_back(summary);
    }
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("error"), QString()},
        {QStringLiteral("method"), QStringLiteral("QueryContextActions")},
        {QStringLiteral("results"), summaries},
        {QStringLiteral("count"), summaries.size()},
    };
}

QVariantMap PortalService::InvokeAction(const QString &actionId, const QVariantMap &options)
{
    const QString id = actionId.trimmed();
    if (id.isEmpty()) {
        return SlmPortalResponseBuilder::invalidArgument(
            QStringLiteral("InvokeAction"),
            {{QStringLiteral("actionId"), actionId},
             {QStringLiteral("options"), options}});
    }
    if (!options.value(QStringLiteral("initiatedByUserGesture"), false).toBool()) {
        return SlmPortalResponseBuilder::permissionDenied(
            QStringLiteral("InvokeAction"),
            {{QStringLiteral("reason"), QStringLiteral("user-gesture-required")}});
    }
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("InvokeAction"));
    }
    QVariantMap request = options;
    request.insert(QStringLiteral("actionId"), id);
    request.insert(QStringLiteral("resourceType"), QStringLiteral("action-invoke"));
    request.insert(QStringLiteral("resourceId"), id);
    request.insert(QStringLiteral("initiatedFromOfficialUI"), false);
    return callBackendRequest(QStringLiteral("InvokeAction"), request);
}

QVariantMap PortalService::QueryNotificationHistoryPreview(const QString &query,
                                                           const QVariantMap &options)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(
            QStringLiteral("QueryNotificationHistoryPreview"));
    }
    QVariantMap authContext = options;
    authContext.insert(QStringLiteral("query"), query);
    authContext.insert(QStringLiteral("resourceType"), QStringLiteral("notification-history-preview"));
    const QVariantMap auth = callBackendDirect(QStringLiteral("QueryNotificationHistoryPreview"), authContext);
    if (!auth.value(QStringLiteral("ok")).toBool()) {
        return auth;
    }
    // Conservative phase-4 default: portal notification history remains internal-only.
    return SlmPortalResponseBuilder::permissionDenied(
        QStringLiteral("QueryNotificationHistoryPreview"),
        {{QStringLiteral("reason"), QStringLiteral("internal-only")}});
}

QVariantMap PortalService::ReadNotificationHistoryItem(const QString &itemId,
                                                       const QVariantMap &options)
{
    const QString id = itemId.trimmed();
    if (id.isEmpty()) {
        return SlmPortalResponseBuilder::invalidArgument(
            QStringLiteral("ReadNotificationHistoryItem"),
            {{QStringLiteral("itemId"), itemId},
             {QStringLiteral("options"), options}});
    }
    if (!options.value(QStringLiteral("initiatedByUserGesture"), false).toBool()) {
        return SlmPortalResponseBuilder::permissionDenied(
            QStringLiteral("ReadNotificationHistoryItem"),
            {{QStringLiteral("reason"), QStringLiteral("user-gesture-required")}});
    }
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(
            QStringLiteral("ReadNotificationHistoryItem"));
    }
    QVariantMap request = options;
    request.insert(QStringLiteral("itemId"), id);
    request.insert(QStringLiteral("resourceType"), QStringLiteral("notification-history-content"));
    request.insert(QStringLiteral("resourceId"), id);
    request.insert(QStringLiteral("initiatedFromOfficialUI"), false);
    const QVariantMap auth = callBackendRequest(QStringLiteral("ReadNotificationHistoryItem"), request);
    if (!auth.value(QStringLiteral("ok")).toBool()) {
        return auth;
    }
    // Conservative phase-4 default: portal notification history remains internal-only.
    return SlmPortalResponseBuilder::permissionDenied(
        QStringLiteral("ReadNotificationHistoryItem"),
        {{QStringLiteral("reason"), QStringLiteral("internal-only")}});
}

QVariantMap PortalService::RequestShare(const QVariantMap &options)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("RequestShare"));
    }
    return callBackendRequest(QStringLiteral("RequestShare"), options);
}

QVariantMap PortalService::ShareItems(const QVariantMap &options)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("ShareItems"));
    }
    return callBackendRequest(QStringLiteral("ShareItems"), options);
}

QVariantMap PortalService::ShareText(const QVariantMap &options)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("ShareText"));
    }
    return callBackendRequest(QStringLiteral("ShareText"), options);
}

QVariantMap PortalService::ShareFiles(const QVariantMap &options)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("ShareFiles"));
    }
    return callBackendRequest(QStringLiteral("ShareFiles"), options);
}

QVariantMap PortalService::ShareUri(const QVariantMap &options)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("ShareUri"));
    }
    return callBackendRequest(QStringLiteral("ShareUri"), options);
}

QVariantMap PortalService::CaptureScreen(const QVariantMap &options)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("CaptureScreen"));
    }
    return callBackendRequest(QStringLiteral("CaptureScreen"), options);
}

QVariantMap PortalService::CaptureWindow(const QVariantMap &options)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("CaptureWindow"));
    }
    return callBackendRequest(QStringLiteral("CaptureWindow"), options);
}

QVariantMap PortalService::CaptureArea(const QVariantMap &options)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("CaptureArea"));
    }
    return callBackendRequest(QStringLiteral("CaptureArea"), options);
}

QVariantMap PortalService::GlobalShortcutsCreateSession(const QVariantMap &options)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("GlobalShortcutsCreateSession"));
    }
    return callBackendSessionRequest(QStringLiteral("GlobalShortcutsCreateSession"), options);
}

QVariantMap PortalService::GlobalShortcutsBindShortcuts(const QString &sessionPath,
                                                        const QVariantMap &options)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("GlobalShortcutsBindShortcuts"));
    }
    return callBackendSessionOperation(QStringLiteral("GlobalShortcutsBindShortcuts"),
                                       sessionPath,
                                       options,
                                       true);
}

QVariantMap PortalService::GlobalShortcutsListBindings(const QString &sessionPath,
                                                       const QVariantMap &options)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("GlobalShortcutsListBindings"));
    }
    return callBackendSessionOperation(QStringLiteral("GlobalShortcutsListBindings"),
                                       sessionPath,
                                       options,
                                       true);
}

QVariantMap PortalService::GlobalShortcutsActivate(const QString &sessionPath,
                                                   const QVariantMap &options)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("GlobalShortcutsActivate"));
    }
    return callBackendSessionOperation(QStringLiteral("GlobalShortcutsActivate"),
                                       sessionPath,
                                       options,
                                       true);
}

QVariantMap PortalService::GlobalShortcutsDeactivate(const QString &sessionPath,
                                                     const QVariantMap &options)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("GlobalShortcutsDeactivate"));
    }
    return callBackendSessionOperation(QStringLiteral("GlobalShortcutsDeactivate"),
                                       sessionPath,
                                       options,
                                       false);
}

QVariantMap PortalService::ScreencastSelectSources(const QString &sessionPath,
                                                   const QVariantMap &options)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("ScreencastSelectSources"));
    }
    return callBackendSessionOperation(QStringLiteral("ScreencastSelectSources"),
                                       sessionPath,
                                       options,
                                       true);
}

QVariantMap PortalService::ScreencastStart(const QString &sessionPath, const QVariantMap &options)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("ScreencastStart"));
    }
    return callBackendSessionOperation(QStringLiteral("ScreencastStart"),
                                       sessionPath,
                                       options,
                                       true);
}

QVariantMap PortalService::ScreencastStop(const QString &sessionPath, const QVariantMap &options)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("ScreencastStop"));
    }
    return callBackendSessionOperation(QStringLiteral("ScreencastStop"),
                                       sessionPath,
                                       options,
                                       false);
}

QVariantMap PortalService::StartScreencastSession(const QVariantMap &options)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("StartScreencastSession"));
    }
    return callBackendSessionRequest(QStringLiteral("StartScreencastSession"), options);
}

QVariantMap PortalService::GetScreencastSessionStreams(const QString &sessionPath)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(
            QStringLiteral("GetScreencastSessionStreams"));
    }
    const QString normalized = sessionPath.trimmed();
    if (normalized.isEmpty()) {
        return SlmPortalResponseBuilder::invalidArgument(
            QStringLiteral("GetScreencastSessionStreams"),
            {{QStringLiteral("reason"), QStringLiteral("missing-session-path")}});
    }
    QVariantMap out;
    const bool ok = QMetaObject::invokeMethod(m_backend,
                                               "getSessionMetadata",
                                               Qt::DirectConnection,
                                               Q_RETURN_ARG(QVariantMap, out),
                                               Q_ARG(QString, normalized));
    if (!ok) {
        return SlmPortalResponseBuilder::serviceUnavailable(
            QStringLiteral("GetScreencastSessionStreams"),
            {{QStringLiteral("reason"), QStringLiteral("backend-invoke-failed")}});
    }

    if (!out.value(QStringLiteral("ok")).toBool()) {
        return out;
    }
    QVariantMap results = out.value(QStringLiteral("results")).toMap();
    QVariantMap metadata = results.value(QStringLiteral("metadata")).toMap();
    QVariantList streams = metadata.value(QStringLiteral("screencast.streams")).toList();
    results.insert(QStringLiteral("session_handle"), normalized);
    results.insert(QStringLiteral("streams"), streams);
    results.insert(QStringLiteral("stream_provider"), QStringLiteral("portal-session"));
    out.insert(QStringLiteral("results"), results);
    return out;
}

QVariantMap PortalService::CloseSession(const QString &sessionPath)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("CloseSession"));
    }
    return callBackendCloseSession(sessionPath);
}

QVariantMap PortalService::callBackendRequest(const QString &portalMethod, const QVariantMap &options)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(portalMethod);
    }
    QVariantMap out;
    const bool ok = QMetaObject::invokeMethod(m_backend,
                                               "handleRequest",
                                               Qt::DirectConnection,
                                               Q_RETURN_ARG(QVariantMap, out),
                                               Q_ARG(QString, portalMethod),
                                               Q_ARG(QDBusMessage, calledFromDBus() ? message()
                                                                                   : QDBusMessage()),
                                               Q_ARG(QVariantMap, options));
    if (!ok) {
        return SlmPortalResponseBuilder::serviceUnavailable(
            portalMethod,
            {{QStringLiteral("reason"), QStringLiteral("backend-invoke-failed")}});
    }
    return out;
}

QVariantMap PortalService::callBackendDirect(const QString &portalMethod, const QVariantMap &options)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(portalMethod);
    }
    QVariantMap out;
    const bool ok = QMetaObject::invokeMethod(m_backend,
                                               "handleDirect",
                                               Qt::DirectConnection,
                                               Q_RETURN_ARG(QVariantMap, out),
                                               Q_ARG(QString, portalMethod),
                                               Q_ARG(QDBusMessage, calledFromDBus() ? message()
                                                                                   : QDBusMessage()),
                                               Q_ARG(QVariantMap, options));
    if (!ok) {
        return SlmPortalResponseBuilder::serviceUnavailable(
            portalMethod,
            {{QStringLiteral("reason"), QStringLiteral("backend-invoke-failed")}});
    }
    return out;
}

QVariantMap PortalService::callBackendSessionRequest(const QString &portalMethod,
                                                     const QVariantMap &options)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(portalMethod);
    }
    QVariantMap out;
    const bool ok = QMetaObject::invokeMethod(m_backend,
                                               "handleSessionRequest",
                                               Qt::DirectConnection,
                                               Q_RETURN_ARG(QVariantMap, out),
                                               Q_ARG(QString, portalMethod),
                                               Q_ARG(QDBusMessage, calledFromDBus() ? message()
                                                                                   : QDBusMessage()),
                                               Q_ARG(QVariantMap, options));
    if (!ok) {
        return SlmPortalResponseBuilder::serviceUnavailable(
            portalMethod,
            {{QStringLiteral("reason"), QStringLiteral("backend-invoke-failed")}});
    }
    return out;
}

QVariantMap PortalService::callBackendSessionOperation(const QString &portalMethod,
                                                       const QString &sessionPath,
                                                       const QVariantMap &options,
                                                       bool requireActive)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(portalMethod);
    }
    QVariantMap out;
    const bool ok = QMetaObject::invokeMethod(m_backend,
                                               "handleSessionOperation",
                                               Qt::DirectConnection,
                                               Q_RETURN_ARG(QVariantMap, out),
                                               Q_ARG(QString, portalMethod),
                                               Q_ARG(QDBusMessage, calledFromDBus() ? message()
                                                                                   : QDBusMessage()),
                                               Q_ARG(QString, sessionPath),
                                               Q_ARG(QVariantMap, options),
                                               Q_ARG(bool, requireActive));
    if (!ok) {
        return SlmPortalResponseBuilder::serviceUnavailable(
            portalMethod,
            {{QStringLiteral("reason"), QStringLiteral("backend-invoke-failed")}});
    }
    return out;
}

QVariantMap PortalService::callBackendCloseSession(const QString &sessionPath)
{
    if (!m_backend) {
        return SlmPortalResponseBuilder::serviceUnavailable(QStringLiteral("CloseSession"));
    }
    QVariantMap out;
    const bool ok = QMetaObject::invokeMethod(m_backend,
                                               "closeSession",
                                               Qt::DirectConnection,
                                               Q_RETURN_ARG(QVariantMap, out),
                                               Q_ARG(QString, sessionPath));
    if (!ok) {
        return SlmPortalResponseBuilder::serviceUnavailable(
            QStringLiteral("CloseSession"),
            {{QStringLiteral("reason"), QStringLiteral("backend-invoke-failed")}});
    }
    return out;
}

void PortalService::registerDbusService()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    QDBusConnectionInterface *iface = bus.interface();
    if (!iface) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    if (iface->isServiceRegistered(QString::fromLatin1(kService)).value()) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    if (!bus.registerService(QString::fromLatin1(kService))) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    const bool ok = bus.registerObject(QString::fromLatin1(kPath),
                                       this,
                                       QDBusConnection::ExportAllSlots |
                                           QDBusConnection::ExportAllProperties);
    if (!ok) {
        bus.unregisterService(QString::fromLatin1(kService));
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }

    m_serviceRegistered = true;
    emit serviceRegisteredChanged();
}
