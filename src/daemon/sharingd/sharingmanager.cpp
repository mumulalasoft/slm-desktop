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
        QStringLiteral("require-approval"),
        QStringLiteral("remote-interaction"),
        QStringLiteral("printer-sharing"),
        QStringLiteral("remote-access"),
        QStringLiteral("media-sharing"),
        QStringLiteral("clipboard-sharing"),
        QStringLiteral("restrict-local"),
        QStringLiteral("require-auth"),
        QStringLiteral("auto-reject-untrusted"),
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

    // Load feature states
    loadFeatureStates();

    // Probe all adapters — unavailable ones just stay disabled
    m_sambaAdapter->probe();
    m_avahiAdapter->probe();
    m_cupsAdapter->probe();
    m_sshAdapter->probe();

    // Re-apply states based on saved settings
    for (auto it = m_featureStates.constBegin(); it != m_featureStates.constEnd(); ++it) {
        if (it.value()) {
            // Re-trigger activation logic
            // Note: simple re-triggering might need care, but for now
            // we rely on the logic in setFeatureEnabled to call adapter activation.
            // A temporary workaround:
            const bool state = it.value();
            m_featureStates.insert(it.key(), false); // Toggle it properly
            setFeatureEnabled(it.key(), state);
        }
    }

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
    // Other features are currently state-only in the UI/backend

    if (ok) {
        m_featureStates.insert(feature, enabled);
        saveFeatureStates(); // Persist changes
        emit featureStateChanged(feature, enabled);
    }
    return ok;
}

void SharingManager::loadFeatureStates()
{
    QFile file(featureStatesPath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isObject()) {
        const QVariantMap map = doc.object().toVariantMap();
        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
            if (m_featureStates.contains(it.key()))
                m_featureStates.insert(it.key(), it.value().toBool());
        }
    }
    file.close();
}

void SharingManager::saveFeatureStates() const
{
    QFile file(featureStatesPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;

    QVariantMap map;
    for (auto it = m_featureStates.constBegin(); it != m_featureStates.constEnd(); ++it)
        map.insert(it.key(), it.value());

    const QJsonDocument doc(QJsonObject::fromVariantMap(map));
    file.write(doc.toJson());
    file.close();
}

QString SharingManager::featureStatesPath() const
{
    const QString xdgDataHome = qEnvironmentVariable("XDG_DATA_HOME");
    const QString base = xdgDataHome.isEmpty()
        ? QDir::homePath() + QStringLiteral("/.local/share/slm-desktop")
        : xdgDataHome + QStringLiteral("/slm-desktop");
    QDir().mkpath(base);
    return QDir(base).filePath(QStringLiteral("feature_states.json"));
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
    QVariantMap normalized = options;
    const QString legacyAccess = options.value(QStringLiteral("access"), QStringLiteral("ro")).toString();
    normalized.insert(QStringLiteral("enabled"), true);
    normalized.insert(QStringLiteral("permission"),
                      legacyAccess == QLatin1String("rw") ? QStringLiteral("write") : QStringLiteral("read"));
    normalized.insert(QStringLiteral("access"),
                      options.value(QStringLiteral("guestAccess"), false).toBool()
                          ? QStringLiteral("all")
                          : QStringLiteral("owner"));
    normalized.insert(QStringLiteral("allowGuest"),
                      options.value(QStringLiteral("guestAccess"), false).toBool());

    return configureShare(path, normalized);
}

QVariantMap SharingManager::removeSharedFolder(const QString &path)
{
    return disableShare(path);
}

QVariantMap SharingManager::updateSharedFolder(const QString &path, const QVariantMap &options)
{
    return addSharedFolder(path, options);
}

QVariantMap SharingManager::listSharedFolders() const
{
    QVariantMap folders;
    const QVariantMap state = loadFolderSharesState();
    for (auto it = state.constBegin(); it != state.constEnd(); ++it) {
        QVariantMap row = it.value().toMap();
        if (!row.value(QStringLiteral("enabled")).toBool())
            continue;
        row.insert(QStringLiteral("path"), it.key());
        const QVariantMap address = buildAddressPayload(row.value(QStringLiteral("shareName")).toString());
        for (auto ai = address.constBegin(); ai != address.constEnd(); ++ai)
            row.insert(ai.key(), ai.value());
        folders.insert(it.key(), row);
    }
    return {{QStringLiteral("ok"), true}, {QStringLiteral("folders"), folders}};
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

// ── Folder sharing compat ─────────────────────────────────────────────────

static constexpr const char kPkexecSetupCmd[] =
    "set -eu; "
    "groupadd -f sambashare && "
    "mkdir -p /var/lib/samba/usershares && "
    "chown root:sambashare /var/lib/samba/usershares && "
    "chmod 1770 /var/lib/samba/usershares && "
    "if [ -f /etc/samba/smb.conf ] && ! grep -q '^[[:space:]]*usershare max shares' /etc/samba/smb.conf; then "
    "cp /etc/samba/smb.conf /etc/samba/smb.conf.slm-bak.$(date +%s) && "
    "sed -i '/^\\[global\\]/a\\   usershare max shares = 100\\n   usershare path = /var/lib/samba/usershares\\n   usershare allow guests = yes\\n   usershare owner only = no' /etc/samba/smb.conf; "
    "fi && "
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
    if (!options.value(QStringLiteral("enabled"), true).toBool())
        return disableShare(p);

    const QFileInfo info(p);
    QVariantMap row;
    row.insert(QStringLiteral("enabled"),    true);
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
    if (row.value(QStringLiteral("enabled")).toBool())
        emit sharedFolderAdded(p, out);
    else
        emit sharedFolderRemoved(p);
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
    emit sharedFolderRemoved(p);
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
    auto pushIssue = [&issues](const QString &code,
                               const QString &title,
                               const QString &message,
                               const QString &guidance,
                               bool fixable,
                               const QVariantMap &extra = QVariantMap{}) {
        QVariantMap issue{
            {QStringLiteral("code"), code},
            {QStringLiteral("title"), title},
            {QStringLiteral("message"), message},
            {QStringLiteral("guidance"), guidance},
            {QStringLiteral("fixable"), fixable},
            {QStringLiteral("severity"), QStringLiteral("required")},
        };
        for (auto it = extra.constBegin(); it != extra.constEnd(); ++it) {
            issue.insert(it.key(), it.value());
        }
        issues.push_back(issue);
    };

    const QString netExec = QStandardPaths::findExecutable(QStringLiteral("net"));
    if (netExec.isEmpty()) {
        pushIssue(QStringLiteral("samba-net-not-found"),
                  QStringLiteral("Samba belum terpasang"),
                  QStringLiteral("Komponen berbagi folder jaringan belum tersedia."),
                  QStringLiteral("Pasang paket samba. Di Ubuntu/Debian: sudo apt install samba samba-common-bin. Setelah instalasi selesai, klik Periksa lagi."),
                  true,
                  {{QStringLiteral("componentId"), QStringLiteral("samba")},
                   {QStringLiteral("packageName"), QStringLiteral("samba")},
                   {QStringLiteral("packageNames"), QStringList{QStringLiteral("samba"), QStringLiteral("samba-common-bin")}},
                   {QStringLiteral("autoInstallable"), true},
                   {QStringLiteral("actionLabel"), QStringLiteral("Install Samba")}});
    } else {
        QProcess checkProc;
        checkProc.start(netExec, {QStringLiteral("usershare"), QStringLiteral("info")});
        if (!checkProc.waitForFinished(5000)) {
            checkProc.kill();
            checkProc.waitForFinished();
            pushIssue(QStringLiteral("usershare-timeout"),
                      QStringLiteral("Samba tidak merespons"),
                      QStringLiteral("Perintah usershare tidak selesai tepat waktu."),
                      QStringLiteral("Pastikan layanan smbd/nmbd berjalan, lalu klik Periksa lagi."),
                      true,
                      {{QStringLiteral("actionLabel"), QStringLiteral("Perbaiki otomatis")}});
        } else if (checkProc.exitCode() != 0) {
            const QString err = (QString::fromUtf8(checkProc.readAllStandardError())
                                 + QLatin1Char('\n')
                                 + QString::fromUtf8(checkProc.readAllStandardOutput())).toLower();
            if (err.contains(QStringLiteral("permission denied")) || err.contains(QStringLiteral("denied")))
                pushIssue(QStringLiteral("usershare-permission-denied"),
                          QStringLiteral("Izin berbagi belum aktif"),
                          QStringLiteral("Akun ini belum dapat menulis konfigurasi Samba usershare."),
                          QStringLiteral("Tambahkan akun ke grup sambashare, pastikan /var/lib/samba/usershares dapat ditulis, lalu logout/login ulang jika perlu."),
                          true,
                          {{QStringLiteral("requiredGroup"), QStringLiteral("sambashare")},
                           {QStringLiteral("actionLabel"), QStringLiteral("Perbaiki izin")}});
            else if (err.contains(QStringLiteral("usershare")) && err.contains(QStringLiteral("not allowed")))
                pushIssue(QStringLiteral("usershare-not-enabled"),
                          QStringLiteral("Samba usershare belum aktif"),
                          QStringLiteral("Konfigurasi Samba belum mengizinkan usershare."),
                          QStringLiteral("Aktifkan usershare max shares, usershare path, usershare allow guests, dan direktori /var/lib/samba/usershares."),
                          true,
                          {{QStringLiteral("configFile"), QStringLiteral("/etc/samba/smb.conf")},
                           {QStringLiteral("actionLabel"), QStringLiteral("Aktifkan usershare")}});
            else
                pushIssue(QStringLiteral("usershare-check-failed"),
                          QStringLiteral("Pemeriksaan Samba gagal"),
                          QStringLiteral("Pemeriksaan usershare gagal: %1").arg(err.trimmed()),
                          QStringLiteral("Jalankan Perbaiki otomatis untuk menyiapkan Samba usershare, lalu klik Periksa lagi."),
                          true,
                          {{QStringLiteral("actionLabel"), QStringLiteral("Perbaiki otomatis")}});
        }
    }

    const bool ready = issues.isEmpty();
    return makeFsResult(ready,
                        ready ? QString() : QStringLiteral("sharing-env-not-ready"),
                        {{QStringLiteral("ready"),  ready},
                         {QStringLiteral("issues"), issues},
                         {QStringLiteral("blockingIssues"), issues}});
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
