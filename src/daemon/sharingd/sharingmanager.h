#pragma once

#include <QHash>
#include <QObject>
#include <QVariantMap>

#include "trustdatabase.h"
#include <QVariantList>
#include <pwd.h>

class ISharingAdapter;
class SambaAdapter;
class AvahiAdapter;
class CupsAdapter;
class SshAdapter;
class NearbyEngine;
class TransferSession;

class SharingManager : public QObject
{
    Q_OBJECT

public:
    explicit SharingManager(QObject *parent = nullptr);
    ~SharingManager() override;

    bool initialize();

    // Feature control
    bool setFeatureEnabled(const QString &feature, bool enabled);
    bool featureEnabled(const QString &feature) const;
    QVariantMap featureStates() const;
    QVariantMap capabilities() const;
    QVariantMap checkEnvironment() const;
    QVariantMap tryAutoFix(const QString &issue);

    // File sharing
    QVariantMap addSharedFolder(const QString &path, const QVariantMap &options);
    QVariantMap removeSharedFolder(const QString &path);
    QVariantMap updateSharedFolder(const QString &path, const QVariantMap &options);
    QVariantMap listSharedFolders() const;

    // Folder sharing compat (replaces org.slm.Desktop.FolderSharing)
    QVariantMap configureShare(const QString &path, const QVariantMap &options);
    QVariantMap disableShare(const QString &path);
    QVariantMap shareInfo(const QString &path) const;
    QVariantMap listSharesCompat() const;
    QVariantMap checkFileSharingEnvironment() const;
    QVariantMap tryAutoFixFileSharing();
    QVariantMap setupSharingPrerequisites();

    // Transfers
    TransferSession *startOutgoingTransfer(const QString &deviceId, const QString &filePath);
    bool cancelTransfer(const QString &transferId);
    QVariantList activeTransfers() const;
    TransferSession *transferById(const QString &transferId) const;

    // Nearby
    NearbyEngine *nearbyEngine() const { return m_nearbyEngine; }

    // Trust
    TrustDatabase *trustDatabase() const { return m_trustDb; }

    // Adapters
    SambaAdapter *sambaAdapter() const { return m_sambaAdapter; }
    AvahiAdapter *avahiAdapter() const { return m_avahiAdapter; }
    CupsAdapter *cupsAdapter() const { return m_cupsAdapter; }
    SshAdapter *sshAdapter() const { return m_sshAdapter; }

signals:
    void sharedFolderAdded(const QString &path, const QVariantMap &info);
    void sharedFolderRemoved(const QString &path);
    void transferStarted(const QString &transferId, const QVariantMap &info);
    void transferProgress(const QString &transferId, qint64 transferred, qint64 total);
    void transferCompleted(const QString &transferId, bool success, const QString &error);
    void featureStateChanged(const QString &feature, bool enabled);
    void shareStateChanged(const QString &path, const QVariantMap &shareInfo);

private:
    void onAdapterStatusChanged();
    void loadPersistedShares();
    void savePersistedShares() const;
    QString stateFilePath() const;

    static QString sanitizeShareName(const QString &name, const QString &fallback);
    static QString normalizeAccessMode(const QString &value);
    static QVariantList normalizeUserList(const QVariant &value);
    static QString buildUsershareAcl(const QVariantMap &record);
    static QVariantMap buildAddressPayload(const QString &shareName);
    QString folderSharesStatePath() const;
    QVariantMap loadFolderSharesState() const;
    bool saveFolderSharesState(const QVariantMap &state, QString *error = nullptr) const;
    QString canonicalSharePath(const QString &path) const;
    QVariantMap folderShareRecord(const QString &path) const;
    bool callerPidUid(uint *outPid, uint *outUid) const;
    bool authorizeSetupAction(uint callerPid) const;
    QVariantMap runPrivilegedSetup(uint callerUid) const;
    static QVariantMap makeFsResult(bool ok, const QString &error = {},
                                    const QVariantMap &extra = {});

    SambaAdapter *m_sambaAdapter = nullptr;
    AvahiAdapter *m_avahiAdapter = nullptr;
    CupsAdapter *m_cupsAdapter = nullptr;
    SshAdapter *m_sshAdapter = nullptr;
    NearbyEngine *m_nearbyEngine = nullptr;
    TrustDatabase *m_trustDb = nullptr;

    QHash<QString, bool> m_featureStates;
    QVariantMap m_sharedFolders;
    QHash<QString, TransferSession *> m_transfers;
};
