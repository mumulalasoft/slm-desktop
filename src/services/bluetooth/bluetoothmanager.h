#pragma once

#include <QObject>
#include <QTimer>
#include <QStringList>
#include <QVariantList>

class BluetoothManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool powered READ powered NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
    Q_PROPERTY(QString iconName READ iconName NOTIFY changed)
    Q_PROPERTY(QStringList connectedDevices READ connectedDevices NOTIFY changed)
    Q_PROPERTY(QVariantList connectedDeviceItems READ connectedDeviceItems NOTIFY changed)

public:
    explicit BluetoothManager(QObject *parent = nullptr);

    bool available() const;
    bool powered() const;
    QString statusText() const;
    QString iconName() const;
    QStringList connectedDevices() const;
    QVariantList connectedDeviceItems() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool setPowered(bool enabled);
    Q_INVOKABLE bool disconnectDevice(const QString &address);
    Q_INVOKABLE bool openBluetoothSettings();

signals:
    void changed();

private:
    QString detectAdapterPath() const;
    bool readPowered(const QString &adapterPath, bool *ok = nullptr) const;
    QVariantList queryConnectedDeviceItems() const;

    bool m_available = false;
    bool m_powered = false;
    QString m_statusText = QStringLiteral("Bluetooth unavailable");
    QString m_iconName = QStringLiteral("bluetooth-disabled-symbolic");
    QStringList m_connectedDevices;
    QVariantList m_connectedDeviceItems;
    QTimer *m_timer = nullptr;
};
