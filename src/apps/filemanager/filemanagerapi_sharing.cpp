#include "filemanagerapi.h"

#include <QDateTime>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSysInfo>
#include <QProcess>

#include "src/core/system/dependencyguard.h"
#include "src/core/system/componentregistry.h"

namespace {

constexpr const char kFolderSharingService[] = "org.slm.Desktop.FolderSharing";
constexpr const char kFolderSharingPath[] = "/org/slm/Desktop/FolderSharing";
constexpr const char kFolderSharingIface[] = "org.slm.Desktop.FolderSharing";
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

bool callFolderSharingService(const QString &method,
                              const QVariantList &args,
                              QVariantMap *resultOut = nullptr,
                              QString *failureCodeOut = nullptr)
{
    QDBusInterface iface(QString::fromLatin1(kFolderSharingService),
                         QString::fromLatin1(kFolderSharingPath),
                         QString::fromLatin1(kFolderSharingIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        if (failureCodeOut) {
            *failureCodeOut = QString::fromLatin1(kErrDaemonUnavailable);
        }
        return false;
    }
    iface.setTimeout(kDbusTimeoutMs);
    QDBusReply<QVariantMap> reply = iface.callWithArgumentList(QDBus::Block, method, args);
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

} // namespace

QString FileManagerApi::folderSharesStatePath() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.local/share/slm-desktop");
    }
    QDir().mkpath(base);
    return QDir(base).filePath(QStringLiteral("folder_shares.json"));
}

QVariantMap FileManagerApi::loadFolderSharesState() const
{
    QFile file(folderSharesStatePath());
    if (!file.exists()) {
        return QVariantMap{};
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return QVariantMap{};
    }
    const QByteArray payload = file.readAll();
    file.close();
    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) {
        return QVariantMap{};
    }
    return doc.object().toVariantMap();
}

bool FileManagerApi::saveFolderSharesState(const QVariantMap &state, QString *error) const
{
    QFile file(folderSharesStatePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = QStringLiteral("state-write-failed");
        }
        return false;
    }
    const QJsonObject obj = QJsonObject::fromVariantMap(state);
    const QJsonDocument doc(obj);
    if (file.write(doc.toJson(QJsonDocument::Indented)) < 0) {
        file.close();
        if (error) {
            *error = QStringLiteral("state-write-failed");
        }
        return false;
    }
    file.close();
    return true;
}

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

QVariantMap FileManagerApi::buildShareAddressPayload(const QString &shareName) const
{
    const QString host = QSysInfo::machineHostName().trimmed().isEmpty()
            ? QStringLiteral("slm-desktop")
            : QSysInfo::machineHostName().trimmed();
    const QString encoded = QString(shareName).trimmed().replace(QStringLiteral(" "),
                                                                  QStringLiteral("%20"));
    const QString smbUri = QStringLiteral("smb://") + host + QStringLiteral("/") + encoded;
    const QString windowsUri = QStringLiteral("\\\\") + host + QStringLiteral("\\") + shareName;
    return {
        {QStringLiteral("address"), smbUri},
        {QStringLiteral("addressWindows"), windowsUri},
        {QStringLiteral("host"), host}
    };
}

QVariantMap FileManagerApi::shareRecordForPath(const QString &path) const
{
    const QString p = canonicalSharePath(path);
    if (p.isEmpty()) {
        return QVariantMap{};
    }
    const QVariantMap all = loadFolderSharesState();
    QVariantMap row = all.value(p).toMap();
    if (row.isEmpty()) {
        return QVariantMap{};
    }
    if (row.value(QStringLiteral("enabled")).toBool()) {
        const QString shareName = sanitizeShareName(row.value(QStringLiteral("shareName")).toString(),
                                                     QFileInfo(p).fileName());
        row.insert(QStringLiteral("shareName"), shareName);
        const QVariantMap address = buildShareAddressPayload(shareName);
        row.insert(QStringLiteral("address"), address.value(QStringLiteral("address")));
        row.insert(QStringLiteral("addressWindows"), address.value(QStringLiteral("addressWindows")));
        row.insert(QStringLiteral("host"), address.value(QStringLiteral("host")));
    }
    row.insert(QStringLiteral("path"), p);
    row.insert(QStringLiteral("ok"), true);
    return row;
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

    QVariantMap row = shareRecordForPath(p);
    if (row.isEmpty()) {
        row.insert(QStringLiteral("ok"), true);
        row.insert(QStringLiteral("path"), p);
        row.insert(QStringLiteral("enabled"), false);
        row.insert(QStringLiteral("shareName"), QFileInfo(p).fileName());
        row.insert(QStringLiteral("access"), QStringLiteral("owner"));
        row.insert(QStringLiteral("permission"), QStringLiteral("read"));
        row.insert(QStringLiteral("allowGuest"), false);
        row.insert(QStringLiteral("users"), QVariantList{});
        row.insert(QStringLiteral("backendPending"), true);
        row.insert(QStringLiteral("backendApplied"), false);
        row.insert(QStringLiteral("backendError"), daemonFailure);
        return row;
    }
    if (!daemonFailure.isEmpty()) {
        row.insert(QStringLiteral("backendError"), daemonFailure);
    }
    return row;
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

    QVariantList out;
    const QVariantMap all = loadFolderSharesState();
    for (auto it = all.constBegin(); it != all.constEnd(); ++it) {
        QVariantMap row = it.value().toMap();
        if (!row.value(QStringLiteral("enabled")).toBool()) {
            continue;
        }
        const QString p = it.key();
        const QString shareName = sanitizeShareName(row.value(QStringLiteral("shareName")).toString(),
                                                     QFileInfo(p).fileName());
        const QVariantMap address = buildShareAddressPayload(shareName);
        row.insert(QStringLiteral("path"), p);
        row.insert(QStringLiteral("shareName"), shareName);
        row.insert(QStringLiteral("address"), address.value(QStringLiteral("address")));
        row.insert(QStringLiteral("addressWindows"), address.value(QStringLiteral("addressWindows")));
        row.insert(QStringLiteral("host"), address.value(QStringLiteral("host")));
        out.push_back(row);
    }
    return out;
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

    QVariantMap all = loadFolderSharesState();
    all.insert(p, row);
    QString error;
    if (!saveFolderSharesState(all, &error)) {
        return makeResult(false, error);
    }

    QVariantMap out = shareRecordForPath(p);
    if (out.isEmpty()) {
        out = row;
        out.insert(QStringLiteral("path"), p);
    }
    out.insert(QStringLiteral("backendError"), daemonFailure);
    emit folderShareStateChanged(p, out);
    return makeResult(true, QString(), out);
}

QVariantMap FileManagerApi::disableFolderShare(const QString &path)
{
    const QString p = canonicalSharePath(path);
    if (p.isEmpty()) {
        return makeResult(false, QStringLiteral("not-a-directory"));
    }
    QVariantMap all = loadFolderSharesState();
    QVariantMap row = all.value(p).toMap();
    if (row.isEmpty()) {
        return makeResult(true, QString(), {{QStringLiteral("path"), p},
                                            {QStringLiteral("enabled"), false}});
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

    row.insert(QStringLiteral("enabled"), false);
    row.insert(QStringLiteral("updatedAt"),
               QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    all.insert(p, row);
    QString error;
    if (!saveFolderSharesState(all, &error)) {
        return makeResult(false, error);
    }
    QVariantMap out = shareRecordForPath(p);
    if (out.isEmpty()) {
        out.insert(QStringLiteral("ok"), true);
        out.insert(QStringLiteral("path"), p);
        out.insert(QStringLiteral("enabled"), false);
    }
    out.insert(QStringLiteral("backendError"), daemonFailure);
    emit folderShareStateChanged(p, out);
    return makeResult(true, QString(), out);
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
    if (callFolderSharingService(QStringLiteral("CheckEnvironment"),
                                 {},
                                 &daemonResult,
                                 &daemonFailure)) {
        return normalizeEnvironmentResult(daemonResult);
    }

    QVariantList issues;
    QVariantList blockingIssues;
    bool hasBlockingMissing = false;
    auto pushIssue = [&issues, &blockingIssues, &hasBlockingMissing](const QString &code,
                                                                      const QString &message,
                                                                      bool fixable,
                                                                      const QString &severity = QStringLiteral("required")) {
        const QString level = severity.trimmed().isEmpty()
                ? QStringLiteral("required")
                : severity.trimmed().toLower();
        if (level == QStringLiteral("required")) {
            hasBlockingMissing = true;
        }
        issues.push_back(QVariantMap{
            {QStringLiteral("code"), code},
            {QStringLiteral("message"), message},
            {QStringLiteral("fixable"), fixable},
            {QStringLiteral("severity"), level},
        });
        if (level == QStringLiteral("required")) {
            blockingIssues.push_back(issues.back());
        }
    };

    const QVariantList missingComponents = Slm::System::ComponentRegistry::missingForDomain(QStringLiteral("filemanager"));
    bool sambaMissing = false;
    for (const QVariant &rowVar : missingComponents) {
        const QVariantMap row = rowVar.toMap();
        const QString componentId = row.value(QStringLiteral("componentId")).toString();
        if (componentId == QStringLiteral("samba")) {
            sambaMissing = true;
        }
        const QString severity = row.value(QStringLiteral("severity")).toString();
        const QString level = severity.trimmed().isEmpty()
                ? QStringLiteral("required")
                : severity.trimmed().toLower();
        if (level == QStringLiteral("required")) {
            hasBlockingMissing = true;
        }
        QVariantMap issue{
            {QStringLiteral("code"), QStringLiteral("component-missing:%1").arg(componentId)},
            {QStringLiteral("message"), row.value(QStringLiteral("description")).toString()},
            {QStringLiteral("fixable"), row.value(QStringLiteral("autoInstallable")).toBool()},
            {QStringLiteral("componentId"), componentId},
            {QStringLiteral("title"), row.value(QStringLiteral("title")).toString()},
            {QStringLiteral("guidance"), row.value(QStringLiteral("guidance")).toString()},
            {QStringLiteral("packageName"), row.value(QStringLiteral("packageName")).toString()},
            {QStringLiteral("autoInstallable"), row.value(QStringLiteral("autoInstallable")).toBool()},
            {QStringLiteral("severity"), level},
        };
        issues.push_back(issue);
        if (level == QStringLiteral("required")) {
            blockingIssues.push_back(issue);
        }
    }

    if (!sambaMissing) {
        const QString netExec = QStandardPaths::findExecutable(QStringLiteral("net"));
        QProcess proc;
        proc.start(netExec, {QStringLiteral("usershare"), QStringLiteral("info")});
        if (!proc.waitForFinished(5000)) {
            proc.kill();
            proc.waitForFinished();
            pushIssue(QStringLiteral("usershare-timeout"),
                      QStringLiteral("Layanan berbagi jaringan tidak merespons."),
                      false,
                      QStringLiteral("required"));
        } else if (proc.exitCode() != 0) {
            const QString err = QString::fromUtf8(proc.readAllStandardError()).toLower();
            if (err.contains(QStringLiteral("permission denied")) || err.contains(QStringLiteral("denied"))) {
                pushIssue(QStringLiteral("usershare-permission-denied"),
                          QStringLiteral("Akun ini belum diizinkan untuk berbagi folder."),
                          false,
                          QStringLiteral("required"));
            } else if (err.contains(QStringLiteral("usershare")) && err.contains(QStringLiteral("not allowed"))) {
                pushIssue(QStringLiteral("usershare-not-enabled"),
                          QStringLiteral("Berbagi jaringan belum diaktifkan pada sistem."),
                          false,
                          QStringLiteral("required"));
            } else {
                pushIssue(QStringLiteral("usershare-check-failed"),
                          QStringLiteral("Pemeriksaan layanan berbagi gagal."),
                          false,
                          QStringLiteral("required"));
            }
        }
    }
    const bool ready = !hasBlockingMissing;
    return normalizeEnvironmentResult(makeResult(ready,
                                                 ready ? QString() : QStringLiteral("sharing-env-not-ready"),
                                                 {{QStringLiteral("ready"), ready},
                                                  {QStringLiteral("issues"), issues},
                                                  {QStringLiteral("blockingIssues"), blockingIssues},
                                                  {QStringLiteral("backendError"), daemonFailure}}));
}

QVariantMap FileManagerApi::repairFolderSharingEnvironment()
{
    QVariantMap daemonResult;
    if (callFolderSharingService(QStringLiteral("TryAutoFix"),
                                 {},
                                 &daemonResult,
                                 nullptr)) {
        return normalizeEnvironmentResult(daemonResult);
    }

    // Fallback local: initialize state file only.
    QString err;
    if (!saveFolderSharesState(loadFolderSharesState(), &err)) {
        return normalizeEnvironmentResult(makeResult(false,
                                                     err.isEmpty() ? QStringLiteral("repair-failed") : err,
                                                     {{QStringLiteral("ready"), false},
                                                      {QStringLiteral("issues"), QVariantList{}},
                                                      {QStringLiteral("actions"), QVariantList{
                                                           QVariantMap{
                                                               {QStringLiteral("id"), QStringLiteral("init-share-state")},
                                                               {QStringLiteral("ok"), false},
                                                               {QStringLiteral("message"), QStringLiteral("Tidak dapat menyiapkan state berbagi.")}
                                                           }
                                                       }}}));
    }
    QVariantMap env = folderSharingEnvironment();
    QVariantList actions{
        QVariantMap{
            {QStringLiteral("id"), QStringLiteral("init-share-state")},
            {QStringLiteral("ok"), true},
            {QStringLiteral("message"), QStringLiteral("State berbagi diinisialisasi.")}
        }
    };
    env.insert(QStringLiteral("actions"), actions);
    return normalizeEnvironmentResult(env);
}

QVariantMap FileManagerApi::installMissingComponent(const QString &componentId)
{
    const QString id = componentId.trimmed().toLower();
    Slm::System::ComponentRequirement req;
    if (!Slm::System::ComponentRegistry::findById(id, &req)) {
        return makeResult(false,
                          QStringLiteral("unsupported-component"),
                          {{QStringLiteral("componentId"), id}});
    }

    bool domainAllowed = false;
    const QList<Slm::System::ComponentRequirement> allowed =
            Slm::System::ComponentRegistry::forDomain(QStringLiteral("filemanager"));
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
                          install);
    }

    QVariantMap env = folderSharingEnvironment();
    env.insert(QStringLiteral("componentId"), id);
    env.insert(QStringLiteral("install"), install);
    env.insert(QStringLiteral("message"), QStringLiteral("Komponen berhasil dipasang."));
    return env;
}
