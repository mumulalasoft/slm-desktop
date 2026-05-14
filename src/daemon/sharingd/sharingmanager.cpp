#include "sharingmanager.h"
#include "nearbyengine.h"
#include "transfersession.h"
#include "adapters/avahiadapter.h"
#include "adapters/cupsadapter.h"
#include "adapters/sambaadapter.h"
#include "adapters/sshadapter.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

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
