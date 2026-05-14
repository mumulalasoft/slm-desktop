#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class QDBusInterface;

class SharingServiceClient : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool available READ available NOTIFY availableChanged)

    Q_PROPERTY(bool fileSharingEnabled      READ fileSharingEnabled      NOTIFY featureStatesChanged)
    Q_PROPERTY(bool nearbySharingEnabled    READ nearbySharingEnabled    NOTIFY featureStatesChanged)
    Q_PROPERTY(bool screenSharingEnabled    READ screenSharingEnabled    NOTIFY featureStatesChanged)
    Q_PROPERTY(bool printerSharingEnabled   READ printerSharingEnabled   NOTIFY featureStatesChanged)
    Q_PROPERTY(bool remoteAccessEnabled     READ remoteAccessEnabled     NOTIFY featureStatesChanged)
    Q_PROPERTY(bool mediaSharingEnabled     READ mediaSharingEnabled     NOTIFY featureStatesChanged)
    Q_PROPERTY(bool clipboardSharingEnabled READ clipboardSharingEnabled NOTIFY featureStatesChanged)

    Q_PROPERTY(QVariantList nearbyDevices  READ nearbyDevices  NOTIFY nearbyDevicesChanged)
    Q_PROPERTY(QVariantList trustedDevices READ trustedDevices NOTIFY trustedDevicesChanged)
    Q_PROPERTY(QVariantList sharedFolders  READ sharedFolders  NOTIFY sharedFoldersChanged)
    Q_PROPERTY(QVariantList activeTransfers READ activeTransfers NOTIFY activeTransfersChanged)
    Q_PROPERTY(bool nearbyDiscovering      READ nearbyDiscovering NOTIFY nearbyDiscoveringChanged)

public:
    explicit SharingServiceClient(QObject *parent = nullptr);
    ~SharingServiceClient() override;

    bool available() const;

    bool fileSharingEnabled() const;
    bool nearbySharingEnabled() const;
    bool screenSharingEnabled() const;
    bool printerSharingEnabled() const;
    bool remoteAccessEnabled() const;
    bool mediaSharingEnabled() const;
    bool clipboardSharingEnabled() const;

    QVariantList nearbyDevices() const;
    QVariantList trustedDevices() const;
    QVariantList sharedFolders() const;
    QVariantList activeTransfers() const;
    bool nearbyDiscovering() const;

    Q_INVOKABLE bool refresh();
    Q_INVOKABLE bool setFeatureEnabled(const QString &feature, bool enabled);
    Q_INVOKABLE QVariantMap addSharedFolder(const QString &path, const QVariantMap &options);
    Q_INVOKABLE bool removeSharedFolder(const QString &path);
    Q_INVOKABLE QVariantMap sendFileTo(const QString &deviceId, const QString &path);
    Q_INVOKABLE bool cancelTransfer(const QString &transferId);
    Q_INVOKABLE bool startDiscovery();
    Q_INVOKABLE bool stopDiscovery();
    Q_INVOKABLE QVariantMap pairDevice(const QString &deviceId);
    Q_INVOKABLE bool unpairDevice(const QString &deviceId);
    Q_INVOKABLE bool blockDevice(const QString &deviceId);
    Q_INVOKABLE bool unblockDevice(const QString &deviceId);
    Q_INVOKABLE bool setDevicePermission(const QString &deviceId, const QString &permission, bool allowed);
    Q_INVOKABLE bool acceptPairingRequest(const QString &pairingId);
    Q_INVOKABLE bool rejectPairingRequest(const QString &pairingId);
    Q_INVOKABLE QVariantMap checkEnvironment();

signals:
    void availableChanged();
    void featureStatesChanged();
    void nearbyDevicesChanged();
    void trustedDevicesChanged();
    void sharedFoldersChanged();
    void activeTransfersChanged();
    void nearbyDiscoveringChanged();
    void transferIncomingRequest(const QString &transferId,
                                  const QString &fromDeviceId,
                                  const QVariantMap &info);
    void pairingRequested(const QString &pairingId, const QVariantMap &deviceInfo);

private slots:
    void onNameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);
    void onFeatureStateChanged(const QString &feature, bool enabled);
    void onSharedFolderAdded(const QString &path, const QVariantMap &info);
    void onSharedFolderRemoved(const QString &path);
    void onDeviceFound(const QString &deviceId, const QVariantMap &info);
    void onDeviceLost(const QString &deviceId);
    void onDeviceUpdated(const QString &deviceId, const QVariantMap &info);
    void onTransferStarted(const QString &transferId, const QVariantMap &info);
    void onTransferProgress(const QString &transferId, qint64 transferred, qint64 total);
    void onTransferCompleted(const QString &transferId, bool success, const QString &error);
    void onTransferIncomingRequest(const QString &transferId,
                                    const QString &fromDeviceId,
                                    const QVariantMap &info);
    void onDeviceTrustChanged(const QString &deviceId, const QString &level);
    void onPairingRequested(const QString &pairingId, const QVariantMap &deviceInfo);

private:
    bool ensureIface();
    void applyFeatureStates(const QVariantMap &features);
    void refreshNearbyDevices();
    void refreshTrustedDevices();
    void refreshSharedFolders();
    void refreshActiveTransfers();
    static QVariantMap normalizeMap(const QVariantMap &map);
    static QVariantList normalizeList(const QVariantList &list);
    static QVariant normalizeVariant(const QVariant &v);

    QDBusInterface *m_iface       = nullptr;
    QDBusInterface *m_nearbyIface = nullptr;
    QDBusInterface *m_trustIface  = nullptr;

    bool m_available = false;

    bool m_fileSharingEnabled      = false;
    bool m_nearbySharingEnabled    = false;
    bool m_screenSharingEnabled    = false;
    bool m_printerSharingEnabled   = false;
    bool m_remoteAccessEnabled     = false;
    bool m_mediaSharingEnabled     = false;
    bool m_clipboardSharingEnabled = false;

    QVariantList m_nearbyDevices;
    QVariantList m_trustedDevices;
    QVariantList m_sharedFolders;
    QVariantList m_activeTransfers;
    bool m_nearbyDiscovering = false;
};
