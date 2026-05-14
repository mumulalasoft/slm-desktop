#pragma once

#include <QDBusContext>
#include <QObject>
#include <QVariantMap>

class SharingManager;

class TrustService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Sharing.Trust")

public:
    explicit TrustService(SharingManager *manager, QObject *parent = nullptr);
    ~TrustService() override;

public slots:
    QVariantMap GetTrustedDevices() const;
    QVariantMap GetDeviceTrust(const QString &deviceId) const;
    QVariantMap SetDeviceTrust(const QString &deviceId, const QString &level);
    QVariantMap PairDevice(const QString &deviceId);
    QVariantMap UnpairDevice(const QString &deviceId);
    QVariantMap GetDevicePermissions(const QString &deviceId) const;
    QVariantMap SetDevicePermission(const QString &deviceId, const QString &permission, bool allowed);
    QVariantMap AcceptPairingRequest(const QString &pairingId);
    QVariantMap RejectPairingRequest(const QString &pairingId);
    QVariantMap BlockDevice(const QString &deviceId);
    QVariantMap UnblockDevice(const QString &deviceId);

signals:
    void DeviceTrustChanged(const QString &deviceId, const QString &level);
    void PairingRequested(const QString &pairingId, const QVariantMap &deviceInfo);
    void PairingCompleted(const QString &pairingId, bool success);
    void DevicePermissionChanged(const QString &deviceId, const QString &permission, bool allowed);

private:
    static QVariantMap makeResult(bool ok,
                                  const QString &error = QString(),
                                  const QVariantMap &extra = QVariantMap());

    SharingManager *m_manager = nullptr;
};
