#pragma once

#include <QDBusContext>
#include <QObject>
#include <QVariantMap>

class SharingManager;

class NearbyService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Sharing.Nearby")

public:
    explicit NearbyService(SharingManager *manager, QObject *parent = nullptr);
    ~NearbyService() override;

public slots:
    QVariantMap GetDevices() const;
    QVariantMap StartDiscovery();
    QVariantMap StopDiscovery();
    QVariantMap GetDiscoveryState() const;
    QVariantMap SendFileTo(const QString &deviceId, const QString &path);
    QVariantMap GetDeviceInfo(const QString &deviceId) const;

signals:
    void DeviceFound(const QString &deviceId, const QVariantMap &info);
    void DeviceLost(const QString &deviceId);
    void DeviceUpdated(const QString &deviceId, const QVariantMap &info);
    void DiscoveryStateChanged(bool active);

private:
    void connectEngineSignals();
    static QVariantMap makeResult(bool ok,
                                  const QString &error = QString(),
                                  const QVariantMap &extra = QVariantMap());

    SharingManager *m_manager = nullptr;
};
