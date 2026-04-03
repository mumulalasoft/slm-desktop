#include "foldersharingservice.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDateTime>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSysInfo>
#include <pwd.h>
#include <unistd.h>

namespace {
constexpr const char kService[] = "org.slm.Desktop.FolderSharing";
constexpr const char kPath[] = "/org/slm/Desktop/FolderSharing";
constexpr const char kApiVersion[] = "1.0";
constexpr int kProcTimeoutMs = 10000;
constexpr const char kSetupPolkitAction[] = "org.slm.desktop.foldersharing.setup";
constexpr const char kPkexecSetupCmd[] =
        "groupadd -f sambashare && "
        "mkdir -p /var/lib/samba/usershares && "
        "chown root:sambashare /var/lib/samba/usershares && "
        "chmod 1770 /var/lib/samba/usershares && "
        "usermod -aG sambashare \"$1\"";
}

FolderSharingService::FolderSharingService(QObject *parent)
    : QObject(parent)
    , m_auditLogger(&m_permissionStore)
{
    setupSecurity();
    registerDbusService();
}

FolderSharingService::~FolderSharingService()
{
    if (!m_serviceRegistered) {
        return;
    }
    QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(kPath));
    QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(kService));
}

bool FolderSharingService::serviceRegistered() const
{
    return m_serviceRegistered;
}

QString FolderSharingService::apiVersion() const
{
    return QString::fromLatin1(kApiVersion);
}

QVariantMap FolderSharingService::Ping() const
{
    return makeResult(true,
                      QString(),
                      {{QStringLiteral("service"), QString::fromLatin1(kService)},
                       {QStringLiteral("apiVersion"), apiVersion()}});
}

QVariantMap FolderSharingService::GetCapabilities() const
{
    return makeResult(true,
                      QString(),
                      {{QStringLiteral("service"), QString::fromLatin1(kService)},
                       {QStringLiteral("apiVersion"), apiVersion()},
                       {QStringLiteral("capabilities"),
                        QStringList{QStringLiteral("configure_share"),
                                    QStringLiteral("disable_share"),
                                    QStringLiteral("share_info"),
                                    QStringLiteral("list_shares"),
                                    QStringLiteral("check_environment"),
                                    QStringLiteral("try_autofix"),
                                    QStringLiteral("setup_prerequisites")}}});
}

QVariantMap FolderSharingService::ConfigureShare(const QString &path, const QVariantMap &options)
{
    if (!checkPermission(Slm::Permissions::Capability::ShareInvoke, QStringLiteral("ConfigureShare"))) {
        return {};
    }

    const QString p = canonicalSharePath(path);
    if (p.isEmpty()) {
        return makeResult(false, QStringLiteral("not-a-directory"));
    }

    const QFileInfo info(p);
    QVariantMap row;
    row.insert(QStringLiteral("enabled"), options.value(QStringLiteral("enabled"), true).toBool());
    row.insert(QStringLiteral("shareName"), sanitizeShareName(options.value(QStringLiteral("shareName")).toString(),
                                                               info.fileName()));
    row.insert(QStringLiteral("access"),
               normalizeAccessMode(options.value(QStringLiteral("access"), QStringLiteral("owner")).toString()));
    row.insert(QStringLiteral("permission"), options.value(QStringLiteral("permission"), QStringLiteral("read")).toString());
    row.insert(QStringLiteral("allowGuest"), options.value(QStringLiteral("allowGuest"), false).toBool());
    row.insert(QStringLiteral("users"), normalizeUserList(options.value(QStringLiteral("users"))));
    row.insert(QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    row.insert(QStringLiteral("backend"), QStringLiteral("smb-auto"));
    row.insert(QStringLiteral("backendPending"), false);
    row.insert(QStringLiteral("requestedEnabled"), row.value(QStringLiteral("enabled")).toBool());

    QVariantMap backend = applyShareBackend(p, row);
    row.insert(QStringLiteral("backendApplied"), backend.value(QStringLiteral("applied")).toBool());
    row.insert(QStringLiteral("backendError"), backend.value(QStringLiteral("error")).toString());
    row.insert(QStringLiteral("backendMessage"), backend.value(QStringLiteral("message")).toString());
    if (!row.value(QStringLiteral("backendApplied")).toBool()) {
        // Do not mark as active share when backend provisioning fails.
        row.insert(QStringLiteral("enabled"), false);
        row.insert(QStringLiteral("backendPending"), true);
        row.insert(QStringLiteral("lastFailedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    }

    QVariantMap state = loadState();
    state.insert(p, row);
    QString saveError;
    if (!saveState(state, &saveError)) {
        return makeResult(false, saveError);
    }

    QVariantMap out = recordForPath(p);
    emit ShareStateChanged(p, out);
    if (!row.value(QStringLiteral("backendApplied")).toBool()) {
        return makeResult(false,
                          row.value(QStringLiteral("backendError")).toString(),
                          out);
    }
    return makeResult(true, QString(), out);
}

QVariantMap FolderSharingService::DisableShare(const QString &path)
{
    if (!checkPermission(Slm::Permissions::Capability::ShareInvoke, QStringLiteral("DisableShare"))) {
        return {};
    }

    const QString p = canonicalSharePath(path);
    if (p.isEmpty()) {
        return makeResult(false, QStringLiteral("not-a-directory"));
    }

    QVariantMap state = loadState();
    QVariantMap row = state.value(p).toMap();
    if (row.isEmpty()) {
        return makeResult(true, QString(), {{QStringLiteral("path"), p}, {QStringLiteral("enabled"), false}});
    }

    const QVariantMap backend = disableShareBackend(row);
    if (!backend.value(QStringLiteral("applied")).toBool()) {
        // Keep row enabled state unchanged if backend refused disable request.
        row.insert(QStringLiteral("backendApplied"), false);
        row.insert(QStringLiteral("backendError"), backend.value(QStringLiteral("error")).toString());
        row.insert(QStringLiteral("backendMessage"), backend.value(QStringLiteral("message")).toString());
        row.insert(QStringLiteral("backendPending"), true);
        row.insert(QStringLiteral("lastFailedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        state.insert(p, row);
        QString writeErr;
        saveState(state, &writeErr);
        QVariantMap out = recordForPath(p);
        emit ShareStateChanged(p, out);
        return makeResult(false,
                          row.value(QStringLiteral("backendError")).toString(),
                          out);
    }
    row.insert(QStringLiteral("enabled"), false);
    row.insert(QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    row.insert(QStringLiteral("backendApplied"), backend.value(QStringLiteral("applied")).toBool());
    row.insert(QStringLiteral("backendError"), backend.value(QStringLiteral("error")).toString());
    row.insert(QStringLiteral("backendMessage"), backend.value(QStringLiteral("message")).toString());
    row.insert(QStringLiteral("backendPending"), !row.value(QStringLiteral("backendApplied")).toBool());
    state.insert(p, row);

    QString saveError;
    if (!saveState(state, &saveError)) {
        return makeResult(false, saveError);
    }

    QVariantMap out = recordForPath(p);
    emit ShareStateChanged(p, out);
    return makeResult(true, QString(), out);
}

QVariantMap FolderSharingService::ShareInfo(const QString &path) const
{
    if (!const_cast<FolderSharingService *>(this)->checkPermission(Slm::Permissions::Capability::ShareInvoke,
                                                                    QStringLiteral("ShareInfo"))) {
        return {};
    }
    const QString p = canonicalSharePath(path);
    if (p.isEmpty()) {
        return makeResult(false, QStringLiteral("not-a-directory"));
    }
    QVariantMap row = recordForPath(p);
    if (row.isEmpty()) {
        row = {
            {QStringLiteral("ok"), true},
            {QStringLiteral("path"), p},
            {QStringLiteral("enabled"), false},
            {QStringLiteral("shareName"), QFileInfo(p).fileName()},
            {QStringLiteral("access"), QStringLiteral("owner")},
            {QStringLiteral("permission"), QStringLiteral("read")},
            {QStringLiteral("allowGuest"), false},
            {QStringLiteral("users"), QVariantList{}},
            {QStringLiteral("backend"), QStringLiteral("smb-auto")},
            {QStringLiteral("backendPending"), true},
            {QStringLiteral("backendApplied"), false},
        };
    }
    return row;
}

QVariantMap FolderSharingService::ListShares() const
{
    if (!const_cast<FolderSharingService *>(this)->checkPermission(Slm::Permissions::Capability::ShareInvoke,
                                                                    QStringLiteral("ListShares"))) {
        return {};
    }
    QVariantList out;
    const QVariantMap state = loadState();
    for (auto it = state.constBegin(); it != state.constEnd(); ++it) {
        QVariantMap row = it.value().toMap();
        if (!row.value(QStringLiteral("enabled")).toBool()) {
            continue;
        }
        row.insert(QStringLiteral("path"), it.key());
        const QVariantMap address = buildAddressPayload(row.value(QStringLiteral("shareName")).toString());
        for (auto ai = address.constBegin(); ai != address.constEnd(); ++ai) {
            row.insert(ai.key(), ai.value());
        }
        out.push_back(row);
    }
    return makeResult(true, QString(), {{QStringLiteral("shares"), out}});
}

QVariantMap FolderSharingService::CheckEnvironment() const
{
    if (!const_cast<FolderSharingService *>(this)->checkPermission(Slm::Permissions::Capability::ShareInvoke,
                                                                    QStringLiteral("CheckEnvironment"))) {
        return {};
    }

    QVariantList issues;
    auto pushIssue = [&issues](const QString &code, const QString &message, bool fixable) {
        issues.push_back(QVariantMap{
            {QStringLiteral("code"), code},
            {QStringLiteral("message"), message},
            {QStringLiteral("fixable"), fixable},
        });
    };

    const QString netExec = QStandardPaths::findExecutable(QStringLiteral("net"));
    if (netExec.isEmpty()) {
        pushIssue(QStringLiteral("samba-net-not-found"),
                  QStringLiteral("Komponen berbagi jaringan belum terpasang."),
                  false);
    } else {
        QProcess checkProc;
        checkProc.start(netExec, {QStringLiteral("usershare"), QStringLiteral("info")});
        if (!checkProc.waitForFinished(5000)) {
            checkProc.kill();
            checkProc.waitForFinished();
            pushIssue(QStringLiteral("usershare-timeout"),
                      QStringLiteral("Layanan berbagi jaringan tidak merespons."),
                      false);
        } else if (checkProc.exitCode() != 0) {
            const QString err = QString::fromUtf8(checkProc.readAllStandardError()).toLower();
            if (err.contains(QStringLiteral("permission denied")) || err.contains(QStringLiteral("denied"))) {
                pushIssue(QStringLiteral("usershare-permission-denied"),
                          QStringLiteral("Akun ini belum diizinkan untuk berbagi folder."),
                          false);
            } else if (err.contains(QStringLiteral("usershare")) && err.contains(QStringLiteral("not allowed"))) {
                pushIssue(QStringLiteral("usershare-not-enabled"),
                          QStringLiteral("Berbagi jaringan belum diaktifkan pada sistem."),
                          false);
            } else {
                pushIssue(QStringLiteral("usershare-check-failed"),
                          QStringLiteral("Pemeriksaan layanan berbagi gagal."),
                          false);
            }
        }
    }

    const bool ready = issues.isEmpty();
    qInfo().noquote() << "[folder-sharing][env-check]"
                      << "caller=" << (calledFromDBus() ? message().service() : QStringLiteral("<local>"))
                      << "ready=" << ready
                      << "issueCount=" << issues.size();
    return makeResult(ready,
                      ready ? QString() : QStringLiteral("sharing-env-not-ready"),
                      {{QStringLiteral("ready"), ready},
                       {QStringLiteral("issues"), issues}});
}

QVariantMap FolderSharingService::TryAutoFix()
{
    if (!checkPermission(Slm::Permissions::Capability::ShareInvoke, QStringLiteral("TryAutoFix"))) {
        return {};
    }

    QVariantList actions;
    auto pushAction = [&actions](const QString &id, bool ok, const QString &message) {
        actions.push_back(QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("ok"), ok},
            {QStringLiteral("message"), message},
        });
    };

    // Limited non-privileged fixups only.
    const QString p = statePath();
    QFile f(p);
    if (!f.exists()) {
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write("{}");
            f.close();
            pushAction(QStringLiteral("init-share-state"), true, QStringLiteral("State berbagi diinisialisasi."));
        } else {
            pushAction(QStringLiteral("init-share-state"), false, QStringLiteral("Tidak dapat menyiapkan state berbagi."));
        }
    } else {
        pushAction(QStringLiteral("init-share-state"), true, QStringLiteral("State berbagi sudah tersedia."));
    }

    // Re-check environment after lightweight fixups.
    QVariantMap env = CheckEnvironment();
    bool ready = env.value(QStringLiteral("ready")).toBool();

    if (!ready) {
        uint callerPid = 0;
        uint callerUid = 0;
        if (callerPidUid(&callerPid, &callerUid) && authorizeSetupAction(callerPid)) {
            const QVariantMap setup = runPrivilegedSetup(callerUid);
            actions.push_back(QVariantMap{
                {QStringLiteral("id"), QStringLiteral("setup-sharing-prerequisites")},
                {QStringLiteral("ok"), setup.value(QStringLiteral("ok")).toBool()},
                {QStringLiteral("message"),
                 setup.value(QStringLiteral("ok")).toBool()
                         ? QStringLiteral("Prasyarat berbagi berhasil disiapkan.")
                         : setup.value(QStringLiteral("error")).toString()},
            });
            env = CheckEnvironment();
            ready = env.value(QStringLiteral("ready")).toBool();
        } else {
            actions.push_back(QVariantMap{
                {QStringLiteral("id"), QStringLiteral("setup-sharing-prerequisites")},
                {QStringLiteral("ok"), false},
                {QStringLiteral("message"), QStringLiteral("Authorization denied")},
            });
        }
    }

    qInfo().noquote() << "[folder-sharing][env-fix]"
                      << "caller=" << (calledFromDBus() ? message().service() : QStringLiteral("<local>"))
                      << "readyAfterFix=" << ready
                      << "actionCount=" << actions.size();
    return makeResult(ready,
                      ready ? QString() : QStringLiteral("sharing-env-not-ready"),
                      {{QStringLiteral("ready"), ready},
                       {QStringLiteral("actions"), actions},
                       {QStringLiteral("issues"), env.value(QStringLiteral("issues")).toList()}});
}

QVariantMap FolderSharingService::SetupSharingPrerequisites()
{
    if (!checkPermission(Slm::Permissions::Capability::ShareInvoke, QStringLiteral("SetupSharingPrerequisites"))) {
        return {};
    }
    uint callerPid = 0;
    uint callerUid = 0;
    if (!callerPidUid(&callerPid, &callerUid)) {
        return makeResult(false, QStringLiteral("caller-unresolved"));
    }
    if (!authorizeSetupAction(callerPid)) {
        return makeResult(false, QStringLiteral("authorization-denied"));
    }
    return runPrivilegedSetup(callerUid);
}

void FolderSharingService::registerDbusService()
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
    if (!bus.registerObject(QString::fromLatin1(kPath),
                            this,
                            QDBusConnection::ExportAllSlots
                                | QDBusConnection::ExportAllSignals
                                | QDBusConnection::ExportAllProperties)) {
        bus.unregisterService(QString::fromLatin1(kService));
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }
    m_serviceRegistered = true;
    emit serviceRegisteredChanged();
}

void FolderSharingService::setupSecurity()
{
    if (!m_permissionStore.open()) {
        qWarning() << "[desktopd] failed to open permission store for FolderSharingService";
    }
    m_policyEngine.setStore(&m_permissionStore);
    m_permissionBroker.setStore(&m_permissionStore);
    m_permissionBroker.setPolicyEngine(&m_policyEngine);
    m_permissionBroker.setAuditLogger(&m_auditLogger);
    m_securityGuard.setTrustResolver(&m_trustResolver);
    m_securityGuard.setPermissionBroker(&m_permissionBroker);
    const QString iface = QStringLiteral("org.slm.Desktop.FolderSharing");
    m_securityGuard.registerMethodCapability(iface, QStringLiteral("ConfigureShare"),
                                             Slm::Permissions::Capability::ShareInvoke);
    m_securityGuard.registerMethodCapability(iface, QStringLiteral("DisableShare"),
                                             Slm::Permissions::Capability::ShareInvoke);
    m_securityGuard.registerMethodCapability(iface, QStringLiteral("ShareInfo"),
                                             Slm::Permissions::Capability::ShareInvoke);
    m_securityGuard.registerMethodCapability(iface, QStringLiteral("ListShares"),
                                             Slm::Permissions::Capability::ShareInvoke);
    m_securityGuard.registerMethodCapability(iface, QStringLiteral("CheckEnvironment"),
                                             Slm::Permissions::Capability::ShareInvoke);
    m_securityGuard.registerMethodCapability(iface, QStringLiteral("TryAutoFix"),
                                             Slm::Permissions::Capability::ShareInvoke);
    m_securityGuard.registerMethodCapability(iface, QStringLiteral("SetupSharingPrerequisites"),
                                             Slm::Permissions::Capability::ShareInvoke);
}

bool FolderSharingService::checkPermission(Slm::Permissions::Capability capability,
                                           const QString &methodName) const
{
    if (!calledFromDBus()) {
        return true;
    }
    QVariantMap ctx;
    ctx.insert(QStringLiteral("method"), methodName);
    ctx.insert(QStringLiteral("sensitivityLevel"), QStringLiteral("Medium"));
    const Slm::Permissions::PolicyDecision d = m_securityGuard.check(message(), capability, ctx);
    if (!d.isAllowed()) {
        const_cast<FolderSharingService *>(this)->sendErrorReply(QStringLiteral("org.slm.PermissionDenied"), d.reason);
        return false;
    }
    return true;
}

QVariantMap FolderSharingService::makeResult(bool ok,
                                             const QString &error,
                                             const QVariantMap &extra)
{
    QVariantMap out = extra;
    out.insert(QStringLiteral("ok"), ok);
    if (!ok) {
        out.insert(QStringLiteral("error"), error.isEmpty() ? QStringLiteral("operation-failed") : error);
    }
    return out;
}

QString FolderSharingService::statePath() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.local/share/slm-desktop");
    }
    QDir().mkpath(base);
    return QDir(base).filePath(QStringLiteral("folder_shares.json"));
}

QVariantMap FolderSharingService::loadState() const
{
    QFile file(statePath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    return doc.isObject() ? doc.object().toVariantMap() : QVariantMap{};
}

bool FolderSharingService::saveState(const QVariantMap &state, QString *error) const
{
    QFile file(statePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = QStringLiteral("state-write-failed");
        }
        return false;
    }
    const QJsonDocument doc(QJsonObject::fromVariantMap(state));
    if (file.write(doc.toJson(QJsonDocument::Indented)) < 0) {
        if (error) {
            *error = QStringLiteral("state-write-failed");
        }
        file.close();
        return false;
    }
    file.close();
    return true;
}

QString FolderSharingService::canonicalSharePath(const QString &path) const
{
    const QString expanded = path == QStringLiteral("~")
            ? QDir::homePath()
            : (path.startsWith(QStringLiteral("~/")) ? QDir::homePath() + path.mid(1)
                                                     : path);
    if (expanded.trimmed().isEmpty()) {
        return {};
    }
    QFileInfo info(expanded);
    if (!info.exists() || !info.isDir()) {
        return {};
    }
    const QString canon = info.canonicalFilePath();
    return canon.isEmpty() ? info.absoluteFilePath() : canon;
}

QVariantMap FolderSharingService::recordForPath(const QString &path) const
{
    const QVariantMap state = loadState();
    QVariantMap row = state.value(path).toMap();
    if (row.isEmpty()) {
        return {};
    }
    row.insert(QStringLiteral("path"), path);
    const QVariantMap address = buildAddressPayload(row.value(QStringLiteral("shareName")).toString());
    for (auto ai = address.constBegin(); ai != address.constEnd(); ++ai) {
        row.insert(ai.key(), ai.value());
    }
    row.insert(QStringLiteral("ok"), true);
    return row;
}

QVariantMap FolderSharingService::buildAddressPayload(const QString &shareName) const
{
    const QString host = QSysInfo::machineHostName().trimmed().isEmpty()
            ? QStringLiteral("slm-desktop")
            : QSysInfo::machineHostName().trimmed();
    const QString encoded = QString(shareName).trimmed().replace(QStringLiteral(" "), QStringLiteral("%20"));
    return {
        {QStringLiteral("address"), QStringLiteral("smb://") + host + QStringLiteral("/") + encoded},
        {QStringLiteral("addressWindows"), QStringLiteral("\\\\") + host + QStringLiteral("\\") + shareName},
        {QStringLiteral("host"), host},
    };
}

QString FolderSharingService::sanitizeShareName(QString name, const QString &fallback) const
{
    name = name.trimmed();
    if (name.isEmpty()) {
        name = fallback;
    }
    name = name.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._ -]")), QStringLiteral("-"));
    name = name.simplified();
    if (name.isEmpty()) {
        name = QStringLiteral("Shared Folder");
    }
    if (name.size() > 80) {
        name = name.left(80).trimmed();
    }
    return name;
}

QString FolderSharingService::normalizeAccessMode(const QString &value) const
{
    const QString v = value.trimmed().toLower();
    if (v == QStringLiteral("anyone") || v == QStringLiteral("all")) {
        return QStringLiteral("all");
    }
    if (v == QStringLiteral("users") || v == QStringLiteral("specific")) {
        return QStringLiteral("specific");
    }
    return QStringLiteral("owner");
}

QVariantList FolderSharingService::normalizeUserList(const QVariant &value) const
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

QString FolderSharingService::buildUsershareAcl(const QVariantMap &record) const
{
    const bool write = record.value(QStringLiteral("permission")).toString() == QStringLiteral("write");
    const QString perm = write ? QStringLiteral("F") : QStringLiteral("R");
    const QString access = record.value(QStringLiteral("access")).toString();
    const bool guest = record.value(QStringLiteral("allowGuest")).toBool();
    const QString currentUser = qEnvironmentVariable("USER").trimmed();
    if (access == QStringLiteral("specific")) {
        QStringList acl;
        const QVariantList users = record.value(QStringLiteral("users")).toList();
        for (const QVariant &u : users) {
            const QString name = u.toString().trimmed();
            if (!name.isEmpty()) {
                acl << (name + QStringLiteral(":") + perm);
            }
        }
        if (acl.isEmpty() && !currentUser.isEmpty()) {
            acl << (currentUser + QStringLiteral(":") + perm);
        }
        return acl.join(QStringLiteral(","));
    }
    if (access == QStringLiteral("all") || guest) {
        return QStringLiteral("Everyone:") + perm;
    }
    if (!currentUser.isEmpty()) {
        return currentUser + QStringLiteral(":") + perm;
    }
    return QStringLiteral("Everyone:R");
}

QVariantMap FolderSharingService::applyShareBackend(const QString &path, const QVariantMap &record) const
{
    if (!record.value(QStringLiteral("enabled")).toBool()) {
        return {{QStringLiteral("applied"), false},
                {QStringLiteral("error"), QStringLiteral("share-disabled")},
                {QStringLiteral("message"), QStringLiteral("share-disabled")}};
    }

    const QString netExec = QStandardPaths::findExecutable(QStringLiteral("net"));
    if (netExec.isEmpty()) {
        return {{QStringLiteral("applied"), false},
                {QStringLiteral("error"), QStringLiteral("samba-net-not-found")},
                {QStringLiteral("message"), QStringLiteral("Samba tools are not installed")}};
    }

    const QString shareName = sanitizeShareName(record.value(QStringLiteral("shareName")).toString(),
                                                QFileInfo(path).fileName());
    const QString comment = QStringLiteral("SLM shared folder");
    const QString acl = buildUsershareAcl(record);
    const QString guestFlag = QStringLiteral("guest_ok=")
            + (record.value(QStringLiteral("allowGuest")).toBool() ? QStringLiteral("y")
                                                                    : QStringLiteral("n"));

    QProcess removeProc;
    removeProc.start(netExec, {QStringLiteral("usershare"), QStringLiteral("delete"), shareName});
    removeProc.waitForFinished(3000);

    QProcess proc;
    proc.start(netExec,
               {QStringLiteral("usershare"),
                QStringLiteral("add"),
                shareName,
                path,
                comment,
                acl,
                guestFlag});
    if (!proc.waitForFinished(kProcTimeoutMs)) {
        proc.kill();
        proc.waitForFinished();
        return {{QStringLiteral("applied"), false},
                {QStringLiteral("error"), QStringLiteral("usershare-timeout")},
                {QStringLiteral("message"), QStringLiteral("Timed out while creating network share")}};
    }
    if (proc.exitCode() != 0) {
        const QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        const QString e = err.toLower();
        QString code = QStringLiteral("usershare-add-failed");
        if (e.contains(QStringLiteral("permission denied")) || e.contains(QStringLiteral("denied"))) {
            code = QStringLiteral("usershare-permission-denied");
        } else if (e.contains(QStringLiteral("usershare")) && e.contains(QStringLiteral("not allowed"))) {
            code = QStringLiteral("usershare-not-enabled");
        } else if (e.contains(QStringLiteral("invalid usershare"))) {
            code = QStringLiteral("usershare-invalid-config");
        }
        return {{QStringLiteral("applied"), false},
                {QStringLiteral("error"), code},
                {QStringLiteral("message"), err.isEmpty() ? QStringLiteral("Unable to create network share")
                                                           : err}};
    }
    return {{QStringLiteral("applied"), true},
            {QStringLiteral("message"), QStringLiteral("share-configured")}};
}

QVariantMap FolderSharingService::disableShareBackend(const QVariantMap &record) const
{
    const QString netExec = QStandardPaths::findExecutable(QStringLiteral("net"));
    if (netExec.isEmpty()) {
        return {{QStringLiteral("applied"), false},
                {QStringLiteral("error"), QStringLiteral("samba-net-not-found")},
                {QStringLiteral("message"), QStringLiteral("Samba tools are not installed")}};
    }
    const QString shareName = record.value(QStringLiteral("shareName")).toString().trimmed();
    if (shareName.isEmpty()) {
        return {{QStringLiteral("applied"), false},
                {QStringLiteral("error"), QStringLiteral("missing-share-name")},
                {QStringLiteral("message"), QStringLiteral("missing-share-name")}};
    }
    QProcess proc;
    proc.start(netExec, {QStringLiteral("usershare"), QStringLiteral("delete"), shareName});
    if (!proc.waitForFinished(kProcTimeoutMs)) {
        proc.kill();
        proc.waitForFinished();
        return {{QStringLiteral("applied"), false},
                {QStringLiteral("error"), QStringLiteral("usershare-timeout")},
                {QStringLiteral("message"), QStringLiteral("Timed out while disabling network share")}};
    }
    if (proc.exitCode() != 0) {
        const QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        const QString e = err.toLower();
        QString code = QStringLiteral("usershare-delete-failed");
        if (e.contains(QStringLiteral("permission denied")) || e.contains(QStringLiteral("denied"))) {
            code = QStringLiteral("usershare-permission-denied");
        } else if (e.contains(QStringLiteral("does not exist")) || e.contains(QStringLiteral("not found"))) {
            // treat missing share as already disabled
            return {{QStringLiteral("applied"), true},
                    {QStringLiteral("message"), QStringLiteral("share-already-disabled")}};
        }
        return {{QStringLiteral("applied"), false},
                {QStringLiteral("error"), code},
                {QStringLiteral("message"), err.isEmpty() ? QStringLiteral("Unable to disable network share")
                                                           : err}};
    }
    return {{QStringLiteral("applied"), true},
            {QStringLiteral("message"), QStringLiteral("share-disabled")}};
}

bool FolderSharingService::callerPidUid(uint *outPid, uint *outUid) const
{
    if (!calledFromDBus()) {
        const uint pid = static_cast<uint>(::getpid());
        const uint uid = static_cast<uint>(::getuid());
        if (outPid) {
            *outPid = pid;
        }
        if (outUid) {
            *outUid = uid;
        }
        return true;
    }
    const QString sender = message().service();
    if (sender.isEmpty()) {
        return false;
    }
    QDBusConnectionInterface *iface = connection().interface();
    if (!iface) {
        return false;
    }
    const QDBusReply<uint> pidReply = iface->servicePid(sender);
    const QDBusReply<uint> uidReply = iface->serviceUid(sender);
    if (!pidReply.isValid() || !uidReply.isValid()) {
        return false;
    }
    if (outPid) {
        *outPid = pidReply.value();
    }
    if (outUid) {
        *outUid = uidReply.value();
    }
    return true;
}

bool FolderSharingService::authorizeSetupAction(uint callerPid) const
{
    QProcess pkcheck;
    pkcheck.start(QStringLiteral("pkcheck"),
                  {QStringLiteral("--action-id"), QString::fromLatin1(kSetupPolkitAction),
                   QStringLiteral("--process"), QString::number(callerPid),
                   QStringLiteral("--allow-user-interaction")});
    if (!pkcheck.waitForStarted(2000)) {
        return false;
    }
    pkcheck.waitForFinished(30000);
    return pkcheck.exitStatus() == QProcess::NormalExit && pkcheck.exitCode() == 0;
}

QVariantMap FolderSharingService::runPrivilegedSetup(uint callerUid) const
{
    const struct passwd *pw = ::getpwuid(callerUid);
    const QString userName = pw ? QString::fromUtf8(pw->pw_name) : QString();
    if (userName.trimmed().isEmpty()) {
        return makeResult(false, QStringLiteral("caller-user-unresolved"));
    }

    QProcess proc;
    proc.setProgram(QStringLiteral("pkexec"));
    proc.setArguments({QStringLiteral("/bin/sh"),
                       QStringLiteral("-c"),
                       QString::fromLatin1(kPkexecSetupCmd),
                       QStringLiteral("slm-setup"),
                       userName});
    proc.start();
    if (!proc.waitForStarted(3000)) {
        return makeResult(false, QStringLiteral("pkexec-start-failed"));
    }
    if (!proc.waitForFinished(30000)) {
        proc.kill();
        proc.waitForFinished();
        return makeResult(false, QStringLiteral("pkexec-timeout"));
    }
    if (proc.exitCode() != 0) {
        const QString stdErr = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        return makeResult(false,
                          stdErr.isEmpty() ? QStringLiteral("setup-failed") : stdErr);
    }
    return makeResult(true,
                      QString(),
                      {{QStringLiteral("restartSessionRecommended"), true},
                       {QStringLiteral("message"), QStringLiteral("setup-complete")}});
}
