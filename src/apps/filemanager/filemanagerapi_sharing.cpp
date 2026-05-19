#include "filemanagerapi.h"

#include <QDateTime>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

#include "../../core/system/componentregistry.h"

namespace {

constexpr const char kSharingService[]     = "org.slm.Sharing";
constexpr const char kSharingPath[]        = "/org/slm/Sharing";
constexpr const char kSharingIface[]       = "org.slm.Sharing";
constexpr const char kSharingNearbyPath[]  = "/org/slm/Sharing/Nearby";
constexpr const char kSharingNearbyIface[] = "org.slm.Sharing.Nearby";
constexpr const char kErrDaemonUnavailable[] = "daemon-unavailable";
constexpr const char kErrDaemonTimeout[] = "daemon-timeout";
constexpr const char kErrDaemonDbusError[] = "daemon-dbus-error";
constexpr int kDbusTimeoutMs = 5000;

enum class DbusFailure {
    None,
    Unavailable,
    Timeout,
    Error,
};

template<typename ReplyT>
DbusFailure classifyDbusReplyError(const ReplyT &reply)
{
    if (reply.isValid()) {
        return DbusFailure::None;
    }
    const QDBusError err = reply.error();
    const QString name = err.name();
    if (err.type() == QDBusError::NoReply || name.endsWith(QStringLiteral(".Timeout"))) {
        return DbusFailure::Timeout;
    }
    if (err.type() == QDBusError::ServiceUnknown
            || err.type() == QDBusError::UnknownObject
            || err.type() == QDBusError::UnknownInterface
            || name.endsWith(QStringLiteral(".ServiceUnknown"))
            || name.endsWith(QStringLiteral(".UnknownObject"))
            || name.endsWith(QStringLiteral(".UnknownInterface"))) {
        return DbusFailure::Unavailable;
    }
    return DbusFailure::Error;
}

QString dbusFailureCode(DbusFailure failure)
{
    switch (failure) {
    case DbusFailure::None:
        return QString();
    case DbusFailure::Unavailable:
        return QString::fromLatin1(kErrDaemonUnavailable);
    case DbusFailure::Timeout:
        return QString::fromLatin1(kErrDaemonTimeout);
    case DbusFailure::Error:
    default:
        return QString::fromLatin1(kErrDaemonDbusError);
    }
}

bool callSharingService(const QString &path,
                        const QString &iface,
                        const QString &method,
                        const QVariantList &args,
                        QVariantMap *resultOut = nullptr,
                        QString *failureCodeOut = nullptr)
{
    QDBusInterface dbusIface(QString::fromLatin1(kSharingService),
                             path,
                             iface,
                             QDBusConnection::sessionBus());
    if (!dbusIface.isValid()) {
        if (failureCodeOut) {
            *failureCodeOut = QString::fromLatin1(kErrDaemonUnavailable);
        }
        return false;
    }
    dbusIface.setTimeout(kDbusTimeoutMs);
    QDBusReply<QVariantMap> reply = dbusIface.callWithArgumentList(QDBus::Block, method, args);
    if (!reply.isValid()) {
        if (failureCodeOut) {
            *failureCodeOut = dbusFailureCode(classifyDbusReplyError(reply));
        }
        return false;
    }
    if (resultOut) {
        *resultOut = reply.value();
    }
    return true;
}

// Convenience wrapper targeting the root /org/slm/Sharing object.
bool callFolderSharingService(const QString &method,
                              const QVariantList &args,
                              QVariantMap *resultOut = nullptr,
                              QString *failureCodeOut = nullptr)
{
    return callSharingService(QString::fromLatin1(kSharingPath),
                              QString::fromLatin1(kSharingIface),
                              method, args, resultOut, failureCodeOut);
}

QString sanitizeShareName(QString name, const QString &fallback)
{
    name = name.trimmed();
    if (name.isEmpty()) {
        name = fallback;
    }
    name = name.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._ -]")),
                        QStringLiteral("-"));
    name = name.simplified();
    if (name.isEmpty()) {
        name = QStringLiteral("Shared Folder");
    }
    if (name.size() > 80) {
        name = name.left(80).trimmed();
    }
    return name;
}

QVariantList normalizeUserList(const QVariant &value)
{
    QVariantList out;
    const QVariantList in = value.toList();
    for (const QVariant &v : in) {
        const QString s = v.toString().trimmed();
        if (!s.isEmpty()) {
            out.push_back(s);
        }
    }
    return out;
}

QString normalizeAccessMode(QString value)
{
    value = value.trimmed().toLower();
    if (value == QStringLiteral("anyone") || value == QStringLiteral("all")) {
        return QStringLiteral("all");
    }
    if (value == QStringLiteral("users") || value == QStringLiteral("specific")) {
        return QStringLiteral("specific");
    }
    return QStringLiteral("owner");
}

QVariantMap normalizeEnvironmentResult(QVariantMap map)
{
    if (!map.contains(QStringLiteral("ok"))) {
        map.insert(QStringLiteral("ok"), false);
    }
    if (!map.contains(QStringLiteral("ready"))) {
        map.insert(QStringLiteral("ready"), false);
    }
    if (!map.contains(QStringLiteral("issues"))) {
        map.insert(QStringLiteral("issues"), QVariantList{});
    }
    if (!map.contains(QStringLiteral("actions"))) {
        map.insert(QStringLiteral("actions"), QVariantList{});
    }
    return map;
}

QVariantMap daemonUnavailableEnvironment(const QString &failureCode)
{
    const QString code = failureCode.trimmed().isEmpty()
            ? QString::fromLatin1(kErrDaemonUnavailable)
            : failureCode.trimmed();
    const QString message = (code == QString::fromLatin1(kErrDaemonTimeout))
            ? QStringLiteral("Layanan slm-sharingd tidak merespons.")
            : QStringLiteral("Layanan slm-sharingd belum berjalan.");
    const QVariantMap issue{
        {QStringLiteral("code"), code},
        {QStringLiteral("message"), message},
        {QStringLiteral("fixable"), false},
        {QStringLiteral("severity"), QStringLiteral("required")},
    };
    return normalizeEnvironmentResult({
        {QStringLiteral("ok"), false},
        {QStringLiteral("error"), code},
        {QStringLiteral("ready"), false},
        {QStringLiteral("issues"), QVariantList{issue}},
        {QStringLiteral("blockingIssues"), QVariantList{issue}},
        {QStringLiteral("backendError"), code},
    });
}

} // namespace

QString FileManagerApi::canonicalSharePath(const QString &path) const
{
    const QString expanded = expandPath(path);
    if (expanded.isEmpty()) {
        return QString();
    }
    QFileInfo info(expanded);
    if (!info.exists() || !info.isDir()) {
        return QString();
    }
    const QString canon = info.canonicalFilePath();
    return canon.isEmpty() ? info.absoluteFilePath() : canon;
}

QVariantMap FileManagerApi::shareRecordForPath(const QString &path) const
{
    const QString p = canonicalSharePath(path);
    if (p.isEmpty()) {
        return QVariantMap{};
    }
    QVariantMap daemonResult;
    if (callFolderSharingService(QStringLiteral("ShareInfo"),
                                 {QVariant::fromValue(p)},
                                 &daemonResult,
                                 nullptr)
            && daemonResult.value(QStringLiteral("ok")).toBool()) {
        return daemonResult;
    }
    return QVariantMap{};
}

QVariantMap FileManagerApi::folderShareInfo(const QString &path) const
{
    const QString p = canonicalSharePath(path);
    if (p.isEmpty()) {
        return makeResult(false, QStringLiteral("not-a-directory"));
    }
    QVariantMap daemonResult;
    QString daemonFailure;
    if (callFolderSharingService(QStringLiteral("ShareInfo"),
                                 {QVariant::fromValue(p)},
                                 &daemonResult,
                                 &daemonFailure)) {
        if (daemonResult.value(QStringLiteral("ok")).toBool()) {
            return daemonResult;
        }
        // Daemon reachable but rejected request; expose daemon response.
        return daemonResult;
    }

    return makeResult(false,
                      daemonFailure.isEmpty()
                              ? QString::fromLatin1(kErrDaemonUnavailable)
                              : daemonFailure,
                      {{QStringLiteral("path"), p},
                       {QStringLiteral("enabled"), false},
                       {QStringLiteral("shareName"), QFileInfo(p).fileName()},
                       {QStringLiteral("access"), QStringLiteral("owner")},
                       {QStringLiteral("permission"), QStringLiteral("read")},
                       {QStringLiteral("allowGuest"), false},
                       {QStringLiteral("users"), QVariantList{}},
                       {QStringLiteral("backendPending"), true},
                       {QStringLiteral("backendApplied"), false},
                       {QStringLiteral("backendError"), daemonFailure}});
}

QVariantList FileManagerApi::folderShares() const
{
    QVariantMap daemonResult;
    if (callFolderSharingService(QStringLiteral("ListShares"),
                                 {},
                                 &daemonResult,
                                 nullptr)
            && daemonResult.value(QStringLiteral("ok")).toBool()) {
        return daemonResult.value(QStringLiteral("shares")).toList();
    }

    return QVariantList{};
}

QVariantMap FileManagerApi::configureFolderShare(const QString &path,
                                                 const QVariantMap &options)
{
    const QString p = canonicalSharePath(path);
    if (p.isEmpty()) {
        return makeResult(false, QStringLiteral("not-a-directory"));
    }

    const QFileInfo info(p);
    const bool enabled = options.value(QStringLiteral("enabled"), true).toBool();
    const QString access = normalizeAccessMode(options.value(QStringLiteral("access"),
                                                             QStringLiteral("owner")).toString());
    const QString permission = options.value(QStringLiteral("permission"),
                                             QStringLiteral("read")).toString();
    const bool allowGuest = options.value(QStringLiteral("allowGuest"), false).toBool();
    const QVariantList users = normalizeUserList(options.value(QStringLiteral("users")));
    const QString shareName = sanitizeShareName(options.value(QStringLiteral("shareName")).toString(),
                                                info.fileName());

    QVariantMap row;
    row.insert(QStringLiteral("enabled"), enabled);
    row.insert(QStringLiteral("shareName"), shareName);
    row.insert(QStringLiteral("access"), access);
    row.insert(QStringLiteral("permission"), permission);
    row.insert(QStringLiteral("allowGuest"), allowGuest);
    row.insert(QStringLiteral("users"), users);
    row.insert(QStringLiteral("updatedAt"),
               QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    row.insert(QStringLiteral("backend"), QStringLiteral("smb-auto"));
    row.insert(QStringLiteral("backendPending"), true);

    QVariantMap daemonResult;
    QString daemonFailure;
    if (callFolderSharingService(QStringLiteral("ConfigureShare"),
                                 {QVariant::fromValue(p), QVariant::fromValue(row)},
                                 &daemonResult,
                                 &daemonFailure)) {
        if (!daemonResult.value(QStringLiteral("ok")).toBool()) {
            return daemonResult;
        }
        emit folderShareStateChanged(p, daemonResult);
        return makeResult(true, QString(), daemonResult);
    }

    return makeResult(false,
                      daemonFailure.isEmpty()
                              ? QString::fromLatin1(kErrDaemonUnavailable)
                              : daemonFailure,
                      {{QStringLiteral("path"), p},
                       {QStringLiteral("enabled"), false},
                       {QStringLiteral("shareName"), shareName},
                       {QStringLiteral("backendPending"), true},
                       {QStringLiteral("backendApplied"), false},
                       {QStringLiteral("backendError"), daemonFailure}});
}

QVariantMap FileManagerApi::disableFolderShare(const QString &path)
{
    const QString p = canonicalSharePath(path);
    if (p.isEmpty()) {
        return makeResult(false, QStringLiteral("not-a-directory"));
    }
    QVariantMap daemonResult;
    QString daemonFailure;
    if (callFolderSharingService(QStringLiteral("DisableShare"),
                                 {QVariant::fromValue(p)},
                                 &daemonResult,
                                 &daemonFailure)) {
        if (!daemonResult.value(QStringLiteral("ok")).toBool()) {
            return daemonResult;
        }
        emit folderShareStateChanged(p, daemonResult);
        return makeResult(true, QString(), daemonResult);
    }

    return makeResult(false,
                      daemonFailure.isEmpty()
                              ? QString::fromLatin1(kErrDaemonUnavailable)
                              : daemonFailure,
                      {{QStringLiteral("path"), p},
                       {QStringLiteral("backendError"), daemonFailure}});
}

QVariantMap FileManagerApi::copyFolderShareAddress(const QString &path) const
{
    const QVariantMap row = folderShareInfo(path);
    if (!row.value(QStringLiteral("ok")).toBool()) {
        return row;
    }
    if (!row.value(QStringLiteral("enabled")).toBool()) {
        return makeResult(false, QStringLiteral("share-disabled"));
    }
    const QString address = row.value(QStringLiteral("address")).toString();
    if (address.isEmpty()) {
        return makeResult(false, QStringLiteral("share-address-unavailable"));
    }
    QVariantMap copied = copyTextToClipboard(address);
    if (!copied.value(QStringLiteral("ok")).toBool()) {
        return copied;
    }
    copied.insert(QStringLiteral("address"), address);
    return copied;
}

QVariantMap FileManagerApi::folderSharingEnvironment() const
{
    QVariantMap daemonResult;
    QString daemonFailure;
    if (callFolderSharingService(QStringLiteral("CheckFileSharingEnvironment"),
                                 {},
                                 &daemonResult,
                                 &daemonFailure)) {
        return normalizeEnvironmentResult(daemonResult);
    }

    return daemonUnavailableEnvironment(daemonFailure);
}

QVariantMap FileManagerApi::repairFolderSharingEnvironment()
{
    QVariantMap daemonResult;
    QString daemonFailure;
    if (callFolderSharingService(QStringLiteral("TryAutoFixFileSharing"),
                                 {},
                                 &daemonResult,
                                 &daemonFailure)) {
        return normalizeEnvironmentResult(daemonResult);
    }

    QVariantMap env = daemonUnavailableEnvironment(daemonFailure);
    QVariantList actions{
        QVariantMap{
            {QStringLiteral("id"), QStringLiteral("contact-slm-sharingd")},
            {QStringLiteral("ok"), false},
            {QStringLiteral("message"), QStringLiteral("slm-sharingd belum tersedia.")}
        }
    };
    env.insert(QStringLiteral("actions"), actions);
    return normalizeEnvironmentResult(env);
}

QVariantMap FileManagerApi::installMissingComponent(const QString &componentId)
{
    return installMissingComponentForDomain(QStringLiteral("filemanager"), componentId);
}

QVariantList FileManagerApi::missingComponentsForDomain(const QString &domain) const
{
    const QString d = domain.trimmed().toLower();
    if (d.isEmpty()) {
        return QVariantList{};
    }
    return Slm::System::ComponentRegistry::missingForDomain(d);
}

QVariantList FileManagerApi::nearbyDevices() const
{
    QDBusInterface iface(QString::fromLatin1(kSharingService),
                         QString::fromLatin1(kSharingNearbyPath),
                         QString::fromLatin1(kSharingNearbyIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return QVariantList{};
    }
    iface.setTimeout(kDbusTimeoutMs);
    QDBusReply<QVariantMap> reply = iface.call(QDBus::Block, QStringLiteral("GetDevices"));
    if (!reply.isValid()) {
        return QVariantList{};
    }
    return reply.value().value(QStringLiteral("devices")).toList();
}

QVariantMap FileManagerApi::sendFileToNearbyDevice(const QString &deviceId,
                                                    const QString &path)
{
    const QString p = expandPath(path);
    if (p.isEmpty() || deviceId.trimmed().isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-arguments"));
    }
    QVariantMap result;
    QString failureCode;
    if (!callSharingService(QString::fromLatin1(kSharingNearbyPath),
                            QString::fromLatin1(kSharingNearbyIface),
                            QStringLiteral("SendFileTo"),
                            {deviceId.trimmed(), QVariant::fromValue(p)},
                            &result,
                            &failureCode)) {
        return makeResult(false,
                          failureCode.isEmpty() ? QStringLiteral("send-failed") : failureCode);
    }
    if (!result.value(QStringLiteral("ok")).toBool()) {
        return result;
    }
    const QString transferId = result.value(QStringLiteral("transferId")).toString();
    const QString deviceName = result.value(QStringLiteral("deviceName")).toString();
    emit nearbyTransferStarted(transferId, deviceName, result);
    return makeResult(true, QString(), result);
}

QVariantMap FileManagerApi::startNearbyDiscovery()
{
    QVariantMap result;
    QString failureCode;
    if (!callSharingService(QString::fromLatin1(kSharingNearbyPath),
                            QString::fromLatin1(kSharingNearbyIface),
                            QStringLiteral("StartDiscovery"),
                            {},
                            &result,
                            &failureCode)) {
        return makeResult(false,
                          failureCode.isEmpty() ? QStringLiteral("discovery-failed") : failureCode);
    }
    return result.value(QStringLiteral("ok")).toBool()
               ? makeResult(true, QString(), result)
               : result;
}

QVariantMap FileManagerApi::stopNearbyDiscovery()
{
    QVariantMap result;
    QString failureCode;
    if (!callSharingService(QString::fromLatin1(kSharingNearbyPath),
                            QString::fromLatin1(kSharingNearbyIface),
                            QStringLiteral("StopDiscovery"),
                            {},
                            &result,
                            &failureCode)) {
        return makeResult(false,
                          failureCode.isEmpty() ? QStringLiteral("stop-failed") : failureCode);
    }
    return result.value(QStringLiteral("ok")).toBool()
               ? makeResult(true, QString(), result)
               : result;
}

QVariantMap FileManagerApi::installMissingComponentForDomain(const QString &domain,
                                                             const QString &componentId)
{
    const QString d = domain.trimmed().toLower();
    const QString id = componentId.trimmed().toLower();
    if (d.isEmpty() || id.isEmpty()) {
        return makeResult(false,
                          QStringLiteral("invalid-arguments"),
                          {{QStringLiteral("domain"), d},
                           {QStringLiteral("componentId"), id}});
    }

    Slm::System::ComponentRequirement req;
    if (!Slm::System::ComponentRegistry::findById(id, &req)) {
        return makeResult(false,
                          QStringLiteral("unsupported-component"),
                          {{QStringLiteral("componentId"), id}});
    }

    bool domainAllowed = false;
    const QList<Slm::System::ComponentRequirement> allowed =
            Slm::System::ComponentRegistry::forDomain(d);
    for (const Slm::System::ComponentRequirement &candidate : allowed) {
        if (candidate.id == id) {
            domainAllowed = true;
            break;
        }
    }
    if (!domainAllowed) {
        return makeResult(false,
                          QStringLiteral("unsupported-component"),
                          {{QStringLiteral("componentId"), id}});
    }

    const QVariantMap install = Slm::System::installComponentWithPolkit(req);
    if (!install.value(QStringLiteral("ok")).toBool()) {
        return makeResult(false,
                          install.value(QStringLiteral("error")).toString(),
                          {{QStringLiteral("domain"), d},
                           {QStringLiteral("componentId"), id},
                           {QStringLiteral("install"), install}});
    }

    QVariantMap out = makeResult(true, QString());
    out.insert(QStringLiteral("domain"), d);
    out.insert(QStringLiteral("componentId"), id);
    out.insert(QStringLiteral("install"), install);
    out.insert(QStringLiteral("message"), QStringLiteral("Komponen berhasil dipasang."));
    out.insert(QStringLiteral("missing"), Slm::System::ComponentRegistry::missingForDomain(d));
    if (d == QStringLiteral("filemanager")) {
        QVariantMap env = folderSharingEnvironment();
        out.insert(QStringLiteral("environment"), env);
        out.insert(QStringLiteral("ready"), env.value(QStringLiteral("ready")).toBool());
    }
    return out;
}
