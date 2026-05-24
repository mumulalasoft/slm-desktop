#pragma once

#include <QObject>
#include <QHash>
#include <QVariantMap>

class AvahiAdapter;

class NearbyEngine : public QObject
{
    Q_OBJECT

public:
    explicit NearbyEngine(AvahiAdapter *avahi, QObject *parent = nullptr);

    void startDiscovery();
    void stopDiscovery();
    bool isDiscovering() const { return m_discovering; }

    QVariantList devices() const;
    QVariantMap deviceInfo(const QString &deviceId) const;
    bool deviceKnown(const QString &deviceId) const;

signals:
    void deviceFound(const QString &deviceId, const QVariantMap &info);
    void deviceLost(const QString &deviceId);
    void deviceUpdated(const QString &deviceId, const QVariantMap &info);
    void discoveryStateChanged(bool active);

private slots:
    void onAdapterEvent(const QString &event, const QVariantMap &payload);

private:
    QString deviceIdFromName(const QString &mdnsName) const;
    QVariantMap buildDeviceInfo(const QString &mdnsName, const QVariantMap &payload) const;

    AvahiAdapter *m_avahi = nullptr;
    bool m_discovering = false;
    QHash<QString, QVariantMap> m_devices;
};
