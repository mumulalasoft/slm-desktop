#include "sharingmanager.h"
#include "nearbyengine.h"
#include "transfersession.h"
#include "adapters/avahiadapter.h"
#include "adapters/cupsadapter.h"
#include "adapters/sambaadapter.h"
#include "adapters/sshadapter.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSysInfo>
#include <unistd.h>
#include <pwd.h>

SharingManager::SharingManager(QObject *parent)
    : QObject(parent)
{
    m_sambaAdapter = new SambaAdapter(this);
    m_avahiAdapter = new AvahiAdapter(this);
    m_cupsAdapter = new CupsAdapter(this);
    m_sshAdapter = new SshAdapter(this);
    m_nearbyEngine = new NearbyEngine(m_avahiAdapter, this);
    m_trustDb = new TrustDatabase(this);

    const QStringList features = {
        QStringLiteral("file-sharing"),
        QStringLiteral("nearby-sharing"),
        QStringLiteral("screen-sharing"),
        QStringLiteral("printer-sharing"),
        QStringLiteral("remote-access"),
        QStringLiteral("media-sharing"),
        QStringLiteral("clipboard-sharing"),
    };
    for (const auto &f : features)
        m_featureStates.insert(f, false);
}

SharingManager::~SharingManager() = default;

bool SharingManager::initialize()
{
    if (!m_trustDb->open()) {
        qWarning("slm-sharingd: failed to open trust database");
        return false;
    }

    // Probe all adapters — unavailable ones just stay disabled
    m_sambaAdapter->probe();
    m_avahiAdapter->probe();
    m_cupsAdapter->probe();
    m_sshAdapter->probe();

    loadPersistedShares();
    return true;
}

bool SharingManager::setFeatureEnabled(const QString &feature, bool enabled)
{
    if (!m_featureStates.contains(feature))
        return false;

    bool ok = true;
    if (feature == QLatin1String("file-sharing")) {
        ok = enabled ? m_sambaAdapter->activate() : m_sambaAdapter->deactivate();
    } else if (feature == QLatin1String("nearby-sharing")) {
        if (enabled) {
            ok = m_avahiAdapter->activate();
            if (ok) m_nearbyEngine->startDiscovery();
        } else {
            m_nearbyEngine->stopDiscovery();
            ok = m_avahiAdapter->deactivate();
        }
    } else if (feature == QLatin1String("printer-sharing")) {
        ok = enabled ? m_cupsAdapter->activate() : m_cupsAdapter->deactivate();
    } else if (feature == QLatin1String("remote-access")) {
        ok = enabled ? m_sshAdapter->activate() : m_sshAdapter->deactivate();
    }

    if (ok) {
        m_featureStates.insert(feature, enabled);
        emit featureStateChanged(feature, enabled);
    }
    return ok;
}

bool SharingManager::featureEnabled(const QString &feature) const
{
    return m_featureStates.value(feature, false);
}

QVariantMap SharingManager::featureStates() const
{
    QVariantMap result;
    for (auto it = m_featureStates.begin(); it != m_featureStates.end(); ++it)
        result.insert(it.key(), it.value());
    return result;
}

QVariantMap SharingManager::capabilities() const
{
    const auto toStatus = [](ISharingAdapter::Status s) -> QString {
        switch (s) {
        case ISharingAdapter::Status::Available:   return QStringLiteral("available");
        case ISharingAdapter::Status::Degraded:    return QStringLiteral("degraded");
        case ISharingAdapter::Status::Unavailable: return QStringLiteral("unavailable");
        }
        return QStringLiteral("unavailable");
    };

    return {
        {QStringLiteral("samba"),  toStatus(m_sambaAdapter->probe())},
        {QStringLiteral("avahi"),  toStatus(m_avahiAdapter->probe())},
        {QStringLiteral("cups"),   toStatus(m_cupsAdapter->probe())},
        {QStringLiteral("ssh"),    toStatus(m_sshAdapter->probe())},
    };
}

QVariantMap SharingManager::checkEnvironment() const
{
    return {
        {QStringLiteral("fileSharing"),    m_sambaAdapter->statusDetail()},
        {QStringLiteral("nearbySharing"),  m_avahiAdapter->statusDetail()},
        {QStringLiteral("printerSharing"), m_cupsAdapter->statusDetail()},
        {QStringLiteral("remoteAccess"),   m_sshAdapter->statusDetail()},
    };
}

QVariantMap SharingManager::tryAutoFix(const QString &issue)
{
    if (issue == QLatin1String("samba-not-found"))
        m_sambaAdapter->recover();
    else if (issue == QLatin1String("avahi-not-found"))
        m_avahiAdapter->recover();
    else if (issue == QLatin1String("cups-not-found"))
        m_cupsAdapter->recover();
    else if (issue == QLatin1String("sshd-not-found"))
        m_sshAdapter->recover();
    else
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("unknown-issue")}};

    return {{QStringLiteral("ok"), true}, {QStringLiteral("issue"), issue}};
}

QVariantMap SharingManager::addSharedFolder(const QString &path, const QVariantMap &options)
{
    if (!m_featureStates.value(QStringLiteral("file-sharing")))
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("file-sharing-disabled")}};

    if (!m_sambaAdapter->configureShare(path, options))
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("samba-configure-failed")}};

    const QVariantMap info = {
        {QStringLiteral("path"), path},
        {QStringLiteral("access"), options.value(QStringLiteral("access"), QStringLiteral("ro"))},
        {QStringLiteral("guestAccess"), options.value(QStringLiteral("guestAccess"), false)},
    };
    m_sharedFolders.insert(path, info);
    savePersistedShares();
    emit sharedFolderAdded(path, info);

    return {{QStringLiteral("ok"), true}, {QStringLiteral("info"), info}};
}

QVariantMap SharingManager::removeSharedFolder(const QString &path)
{
    if (!m_sambaAdapter->removeShare(path))
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("samba-remove-failed")}};

    m_sharedFolders.remove(path);
    savePersistedShares();
    emit sharedFolderRemoved(path);

    return {{QStringLiteral("ok"), true}};
}

QVariantMap SharingManager::updateSharedFolder(const QString &path, const QVariantMap &options)
{
    removeSharedFolder(path);
    return addSharedFolder(path, options);
}

QVariantMap SharingManager::listSharedFolders() const
{
    return {{QStringLiteral("ok"), true}, {QStringLiteral("folders"), m_sharedFolders}};
}

TransferSession *SharingManager::startOutgoingTransfer(const QString &deviceId, const QString &filePath)
{
    auto *session = new TransferSession(TransferSession::Direction::Outgoing, deviceId, filePath, this);
    m_transfers.insert(session->transferId(), session);

    connect(session, &TransferSession::completed, this, [this, session](bool success, const QString &error) {
        emit transferCompleted(session->transferId(), success, error);
        m_transfers.remove(session->transferId());
        session->deleteLater();
    });
    connect(session, &TransferSession::progressChanged, this, [this, session](qint64 b, qint64 t) {
        emit transferProgress(session->transferId(), b, t);
    });

    emit transferStarted(session->transferId(), session->toVariantMap());
    return session;
}

bool SharingManager::cancelTransfer(const QString &transferId)
{
    auto *session = m_transfers.value(transferId);
    if (!session)
        return false;
    session->cancel();
    return true;
}

QVariantList SharingManager::activeTransfers() const
{
    QVariantList result;
    for (auto *s : m_transfers)
        result.append(s->toVariantMap());
    return result;
}

TransferSession *SharingManager::transferById(const QString &transferId) const
{
    return m_transfers.value(transferId);
}

void SharingManager::loadPersistedShares()
{
    QFile f(stateFilePath());
    if (!f.open(QIODevice::ReadOnly))
        return;
    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (doc.isObject()) {
        const auto obj = doc.object();
        for (auto it = obj.begin(); it != obj.end(); ++it)
            m_sharedFolders.insert(it.key(), it.value().toVariant());
    }
}

void SharingManager::savePersistedShares() const
{
    const QString path = stateFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    f.write(QJsonDocument::fromVariant(m_sharedFolders).toJson());
}

QString SharingManager::stateFilePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QLatin1String("/slm-sharingd/shares.json");
}

// ── Folder sharing compat ─────────────────────────────────────────────────

static constexpr const char kPkexecSetupCmd[] =
    "groupadd -f sambashare && "
    "mkdir -p /var/lib/samba/usershares && "
    "chown root:sambashare /var/lib/samba/usershares && "
    "chmod 1770 /var/lib/samba/usershares && "
    "usermod -aG sambashare \"$1\"";

QString SharingManager::folderSharesStatePath() const
{
    const QString xdgDataHome = qEnvironmentVariable("XDG_DATA_HOME");
    const QString base = xdgDataHome.isEmpty()
        ? QDir::homePath() + QStringLiteral("/.local/share/slm-desktop")
        : xdgDataHome + QStringLiteral("/slm-desktop");
    QDir().mkpath(base);
    return QDir(base).filePath(QStringLiteral("folder_shares.json"));
}

QVariantMap SharingManager::loadFolderSharesState() const
{
    QFile file(folderSharesStatePath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
        return {};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    return doc.isObject() ? doc.object().toVariantMap() : QVariantMap{};
}

bool SharingManager::saveFolderSharesState(const QVariantMap &state, QString *error) const
{
    QFile file(folderSharesStatePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) *error = QStringLiteral("state-write-failed");
        return false;
    }
    const QJsonDocument doc(QJsonObject::fromVariantMap(state));
    if (file.write(doc.toJson(QJsonDocument::Indented)) < 0) {
        if (error) *error = QStringLiteral("state-write-failed");
        file.close();
        return false;
    }
    file.close();
    return true;
}

QString SharingManager::canonicalSharePath(const QString &path) const
{
    const QString expanded = path == QStringLiteral("~")
        ? QDir::homePath()
        : (path.startsWith(QStringLiteral("~/")) ? QDir::homePath() + path.mid(1) : path);
    if (expanded.trimmed().isEmpty())
        return {};
    QFileInfo info(expanded);
    if (!info.exists() || !info.isDir())
        return {};
    const QString canon = info.canonicalFilePath();
    return canon.isEmpty() ? info.absoluteFilePath() : canon;
}

QVariantMap SharingManager::folderShareRecord(const QString &path) const
{
    const QVariantMap state = loadFolderSharesState();
    QVariantMap row = state.value(path).toMap();
    if (row.isEmpty())
        return {};
    row.insert(QStringLiteral("path"), path);
    const QVariantMap address = buildAddressPayload(row.value(QStringLiteral("shareName")).toString());
    for (auto ai = address.constBegin(); ai != address.constEnd(); ++ai)
        row.insert(ai.key(), ai.value());
    row.insert(QStringLiteral("ok"), true);
    return row;
}

QString SharingManager::sanitizeShareName(const QString &name, const QString &fallback)
{
    QString n = name.trimmed();
    if (n.isEmpty()) n = fallback;
    n = n.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._ -]")), QStringLiteral("-"));
    n = n.simplified();
    if (n.isEmpty()) n = QStringLiteral("Shared Folder");
    if (n.size() > 80) n = n.left(80).trimmed();
    return n;
}

QString SharingManager::normalizeAccessMode(const QString &value)
{
    const QString v = value.trimmed().toLower();
    if (v == QStringLiteral("anyone") || v == QStringLiteral("all"))
        return QStringLiteral("all");
    if (v == QStringLiteral("users") || v == QStringLiteral("specific"))
        return QStringLiteral("specific");
    return QStringLiteral("owner");
}

QVariantList SharingManager::normalizeUserList(const QVariant &value)
{
    QVariantList out;
    for (const QVariant &v : value.toList()) {
        const QString s = v.toString().trimmed();
        if (!s.isEmpty()) out.push_back(s);
    }
    return out;
}

QString SharingManager::buildUsershareAcl(const QVariantMap &record)
{
    const bool write = record.value(QStringLiteral("permission")).toString() == QStringLiteral("write");
    const QString perm = write ? QStringLiteral("F") : QStringLiteral("R");
    const QString access = record.value(QStringLiteral("access")).toString();
    const bool guest = record.value(QStringLiteral("allowGuest")).toBool();
    const QString currentUser = qEnvironmentVariable("USER").trimmed();
    if (access == QStringLiteral("specific")) {
        QStringList acl;
        for (const QVariant &u : record.value(QStringLiteral("users")).toList()) {
            const QString name = u.toString().trimmed();
            if (!name.isEmpty()) acl << (name + QStringLiteral(":") + perm);
        }
        if (acl.isEmpty() && !currentUser.isEmpty())
            acl << (currentUser + QStringLiteral(":") + perm);
        return acl.join(QStringLiteral(","));
    }
    if (access == QStringLiteral("all") || guest)
        return QStringLiteral("Everyone:") + perm;
    if (!currentUser.isEmpty())
        return currentUser + QStringLiteral(":") + perm;
    return QStringLiteral("Everyone:R");
}

QVariantMap SharingManager::buildAddressPayload(const QString &shareName)
{
    const QString host = QSysInfo::machineHostName().trimmed().isEmpty()
        ? QStringLiteral("slm-desktop")
        : QSysInfo::machineHostName().trimmed();
    const QString encoded = QString(shareName).trimmed().replace(QStringLiteral(" "), QStringLiteral("%20"));
    return {
        {QStringLiteral("address"),        QStringLiteral("smb://") + host + QStringLiteral("/") + encoded},
        {QStringLiteral("addressWindows"), QStringLiteral("\\\\") + host + QStringLiteral("\\") + shareName},
        {QStringLiteral("host"),           host},
    };
}

QVariantMap SharingManager::makeFsResult(bool ok, const QString &error, const QVariantMap &extra)
{
    QVariantMap out = extra;
    out.insert(QStringLiteral("ok"), ok);
    if (!ok)
        out.insert(QStringLiteral("error"), error.isEmpty() ? QStringLiteral("operation-failed") : error);
    return out;
}

QVariantMap SharingManager::configureShare(const QString &path, const QVariantMap &options)
{
    const QString p = canonicalSharePath(path);
    if (p.isEmpty())
        return makeFsResult(false, QStringLiteral("not-a-directory"));

    const QFileInfo info(p);
    QVariantMap row;
    row.insert(QStringLiteral("enabled"),    options.value(QStringLiteral("enabled"), true).toBool());
    row.insert(QStringLiteral("shareName"),  sanitizeShareName(options.value(QStringLiteral("shareName")).toString(), info.fileName()));
    row.insert(QStringLiteral("access"),     normalizeAccessMode(options.value(QStringLiteral("access"), QStringLiteral("owner")).toString()));
    row.insert(QStringLiteral("permission"), options.value(QStringLiteral("permission"), QStringLiteral("read")).toString());
    row.insert(QStringLiteral("allowGuest"), options.value(QStringLiteral("allowGuest"), false).toBool());
    row.insert(QStringLiteral("users"),      normalizeUserList(options.value(QStringLiteral("users"))));
    row.insert(QStringLiteral("updatedAt"),  QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    row.insert(QStringLiteral("backend"),    QStringLiteral("smb-auto"));
    row.insert(QStringLiteral("backendPending"), false);

    const QString acl = buildUsershareAcl(row);
    const bool guestOk = row.value(QStringLiteral("allowGuest")).toBool();
    const QVariantMap backend = m_sambaAdapter->applyShare(row.value(QStringLiteral("shareName")).toString(), p, acl, guestOk);
    const bool applied = backend.value(QStringLiteral("applied")).toBool();
    row.insert(QStringLiteral("backendApplied"),  applied);
    row.insert(QStringLiteral("backendError"),    backend.value(QStringLiteral("error")).toString());
    row.insert(QStringLiteral("backendMessage"),  backend.value(QStringLiteral("message")).toString());
    if (!applied) {
        row.insert(QStringLiteral("enabled"),       false);
        row.insert(QStringLiteral("backendPending"), true);
        row.insert(QStringLiteral("lastFailedAt"),   QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    }

    QVariantMap state = loadFolderSharesState();
    state.insert(p, row);
    QString saveError;
    if (!saveFolderSharesState(state, &saveError))
        return makeFsResult(false, saveError);

    QVariantMap out = folderShareRecord(p);
    emit shareStateChanged(p, out);
    if (!applied)
        return makeFsResult(false, row.value(QStringLiteral("backendError")).toString(), out);
    return makeFsResult(true, QString(), out);
}

QVariantMap SharingManager::disableShare(const QString &path)
{
    const QString p = canonicalSharePath(path);
    if (p.isEmpty())
        return makeFsResult(false, QStringLiteral("not-a-directory"));

    QVariantMap state = loadFolderSharesState();
    QVariantMap row = state.value(p).toMap();
    if (row.isEmpty())
        return makeFsResult(true, QString(), {{QStringLiteral("path"), p}, {QStringLiteral("enabled"), false}});

    const QString shareName = row.value(QStringLiteral("shareName")).toString();
    const QVariantMap backend = m_sambaAdapter->deleteShare(shareName);
    const bool applied = backend.value(QStringLiteral("applied")).toBool();
    if (!applied) {
        row.insert(QStringLiteral("backendApplied"),  false);
        row.insert(QStringLiteral("backendError"),    backend.value(QStringLiteral("error")).toString());
        row.insert(QStringLiteral("backendPending"),  true);
        row.insert(QStringLiteral("lastFailedAt"),    QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        state.insert(p, row);
        QString writeErr;
        saveFolderSharesState(state, &writeErr);
        QVariantMap out = folderShareRecord(p);
        emit shareStateChanged(p, out);
        return makeFsResult(false, row.value(QStringLiteral("backendError")).toString(), out);
    }
    row.insert(QStringLiteral("enabled"),       false);
    row.insert(QStringLiteral("updatedAt"),     QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    row.insert(QStringLiteral("backendApplied"), true);
    row.insert(QStringLiteral("backendPending"), false);
    state.insert(p, row);
    QString saveError;
    if (!saveFolderSharesState(state, &saveError))
        return makeFsResult(false, saveError);
    QVariantMap out = folderShareRecord(p);
    emit shareStateChanged(p, out);
    return makeFsResult(true, QString(), out);
}

QVariantMap SharingManager::shareInfo(const QString &path) const
{
    const QString p = canonicalSharePath(path);
    if (p.isEmpty())
        return makeFsResult(false, QStringLiteral("not-a-directory"));
    QVariantMap row = folderShareRecord(p);
    if (row.isEmpty()) {
        return makeFsResult(true, QString(), {
            {QStringLiteral("path"),         p},
            {QStringLiteral("enabled"),      false},
            {QStringLiteral("shareName"),    QFileInfo(p).fileName()},
            {QStringLiteral("access"),       QStringLiteral("owner")},
            {QStringLiteral("permission"),   QStringLiteral("read")},
            {QStringLiteral("allowGuest"),   false},
            {QStringLiteral("users"),        QVariantList{}},
            {QStringLiteral("backend"),      QStringLiteral("smb-auto")},
            {QStringLiteral("backendPending"), true},
            {QStringLiteral("backendApplied"), false},
        });
    }
    return row;
}

QVariantMap SharingManager::listSharesCompat() const
{
    QVariantList out;
    const QVariantMap state = loadFolderSharesState();
    for (auto it = state.constBegin(); it != state.constEnd(); ++it) {
        QVariantMap row = it.value().toMap();
        if (!row.value(QStringLiteral("enabled")).toBool())
            continue;
        row.insert(QStringLiteral("path"), it.key());
        const QVariantMap address = buildAddressPayload(row.value(QStringLiteral("shareName")).toString());
        for (auto ai = address.constBegin(); ai != address.constEnd(); ++ai)
            row.insert(ai.key(), ai.value());
        out.push_back(row);
    }
    return makeFsResult(true, QString(), {{QStringLiteral("shares"), out}});
}

QVariantMap SharingManager::checkFileSharingEnvironment() const
{
    QVariantList issues;
    auto pushIssue = [&issues](const QString &code, const QString &message, bool fixable) {
        issues.push_back(QVariantMap{
            {QStringLiteral("code"),    code},
            {QStringLiteral("message"), message},
            {QStringLiteral("fixable"), fixable},
        });
    };

    const QString netExec = QStandardPaths::findExecutable(QStringLiteral("net"));
    if (netExec.isEmpty()) {
        pushIssue(QStringLiteral("samba-net-not-found"),
                  QStringLiteral("Komponen berbagi jaringan belum terpasang."), false);
    } else {
        QProcess checkProc;
        checkProc.start(netExec, {QStringLiteral("usershare"), QStringLiteral("info")});
        if (!checkProc.waitForFinished(5000)) {
            checkProc.kill();
            checkProc.waitForFinished();
            pushIssue(QStringLiteral("usershare-timeout"),
                      QStringLiteral("Layanan berbagi jaringan tidak merespons."), false);
        } else if (checkProc.exitCode() != 0) {
            const QString err = QString::fromUtf8(checkProc.readAllStandardError()).toLower();
            if (err.contains(QStringLiteral("permission denied")) || err.contains(QStringLiteral("denied")))
                pushIssue(QStringLiteral("usershare-permission-denied"),
                          QStringLiteral("Akun ini belum diizinkan untuk berbagi folder."), false);
            else if (err.contains(QStringLiteral("usershare")) && err.contains(QStringLiteral("not allowed")))
                pushIssue(QStringLiteral("usershare-not-enabled"),
                          QStringLiteral("Berbagi jaringan belum diaktifkan pada sistem."), false);
            else
                pushIssue(QStringLiteral("usershare-check-failed"),
                          QStringLiteral("Pemeriksaan layanan berbagi gagal."), false);
        }
    }

    const bool ready = issues.isEmpty();
    return makeFsResult(ready,
                        ready ? QString() : QStringLiteral("sharing-env-not-ready"),
                        {{QStringLiteral("ready"),  ready},
                         {QStringLiteral("issues"), issues}});
}

QVariantMap SharingManager::tryAutoFixFileSharing()
{
    QVariantList actions;
    auto pushAction = [&actions](const QString &id, bool ok, const QString &message) {
        actions.push_back(QVariantMap{
            {QStringLiteral("id"),      id},
            {QStringLiteral("ok"),      ok},
            {QStringLiteral("message"), message},
        });
    };

    const QString p = folderSharesStatePath();
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

    QVariantMap env = checkFileSharingEnvironment();
    bool ready = env.value(QStringLiteral("ready")).toBool();
    if (!ready) {
        uint callerPid = 0, callerUid = 0;
        if (callerPidUid(&callerPid, &callerUid) && authorizeSetupAction(callerPid)) {
            const QVariantMap setup = runPrivilegedSetup(callerUid);
            pushAction(QStringLiteral("setup-sharing-prerequisites"),
                       setup.value(QStringLiteral("ok")).toBool(),
                       setup.value(QStringLiteral("ok")).toBool()
                           ? QStringLiteral("Prasyarat berbagi berhasil disiapkan.")
                           : setup.value(QStringLiteral("error")).toString());
            env = checkFileSharingEnvironment();
            ready = env.value(QStringLiteral("ready")).toBool();
        } else {
            pushAction(QStringLiteral("setup-sharing-prerequisites"), false,
                       QStringLiteral("Authorization denied"));
        }
    }

    return makeFsResult(ready,
                        ready ? QString() : QStringLiteral("sharing-env-not-ready"),
                        {{QStringLiteral("ready"),   ready},
                         {QStringLiteral("actions"), actions},
                         {QStringLiteral("issues"),  env.value(QStringLiteral("issues")).toList()}});
}

QVariantMap SharingManager::setupSharingPrerequisites()
{
    uint callerPid = 0, callerUid = 0;
    if (!callerPidUid(&callerPid, &callerUid))
        return makeFsResult(false, QStringLiteral("caller-unresolved"));
    if (!authorizeSetupAction(callerPid))
        return makeFsResult(false, QStringLiteral("authorization-denied"));
    return runPrivilegedSetup(callerUid);
}

bool SharingManager::callerPidUid(uint *outPid, uint *outUid) const
{
    const uint pid = static_cast<uint>(::getpid());
    const uint uid = static_cast<uint>(::getuid());
    if (outPid) *outPid = pid;
    if (outUid) *outUid = uid;
    return true;
}

bool SharingManager::authorizeSetupAction(uint callerPid) const
{
    QProcess pkcheck;
    pkcheck.start(QStringLiteral("pkcheck"),
                  {QStringLiteral("--action-id"),
                   QStringLiteral("org.slm.desktop.foldersharing.setup"),
                   QStringLiteral("--process"), QString::number(callerPid),
                   QStringLiteral("--allow-user-interaction")});
    if (!pkcheck.waitForStarted(2000)) return false;
    pkcheck.waitForFinished(30000);
    return pkcheck.exitStatus() == QProcess::NormalExit && pkcheck.exitCode() == 0;
}

QVariantMap SharingManager::runPrivilegedSetup(uint callerUid) const
{
    const struct passwd *pw = ::getpwuid(callerUid);
    const QString userName = pw ? QString::fromUtf8(pw->pw_name) : QString();
    if (userName.trimmed().isEmpty())
        return makeFsResult(false, QStringLiteral("caller-user-unresolved"));

    QProcess proc;
    proc.setProgram(QStringLiteral("pkexec"));
    proc.setArguments({QStringLiteral("/bin/sh"), QStringLiteral("-c"),
                       QString::fromLatin1(kPkexecSetupCmd),
                       QStringLiteral("slm-setup"), userName});
    proc.start();
    if (!proc.waitForStarted(3000))
        return makeFsResult(false, QStringLiteral("pkexec-start-failed"));
    if (!proc.waitForFinished(30000)) {
        proc.kill();
        proc.waitForFinished();
        return makeFsResult(false, QStringLiteral("pkexec-timeout"));
    }
    if (proc.exitCode() != 0) {
        const QString stdErr = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        return makeFsResult(false, stdErr.isEmpty() ? QStringLiteral("setup-failed") : stdErr);
    }
    return makeFsResult(true, QString(), {
        {QStringLiteral("restartSessionRecommended"), true},
        {QStringLiteral("message"), QStringLiteral("setup-complete")},
    });
}
