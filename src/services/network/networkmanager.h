#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QAbstractListModel>
#include <QVector>
#include <QStringList>
#include <QVariantMap>
#include <QDBusObjectPath>

struct AvailableNetwork {
    QString ssid;
    int signalStrength;
    bool isSecure;
    bool isActive;
};

class AvailableNetworksModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum NetworkRoles {
        SsidRole = Qt::UserRole + 1,
        SignalStrengthRole,
        IsSecureRole,
        IsActiveRole
    };

    explicit AvailableNetworksModel(QObject *parent = nullptr);
    
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    
    void setNetworks(const QVector<AvailableNetwork> &networks);

private:
    QVector<AvailableNetwork> m_networks;
};

class NetworkManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(int signalStrength READ signalStrength NOTIFY signalStrengthChanged)
    Q_PROPERTY(QString connectionType READ connectionType NOTIFY connectionTypeChanged)
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY isConnectedChanged)
    Q_PROPERTY(QString networkName READ networkName NOTIFY networkNameChanged)
    Q_PROPERTY(QAbstractListModel* availableNetworks READ availableNetworks NOTIFY availableNetworksChanged)
    Q_PROPERTY(bool hasAvailableNetworks READ hasAvailableNetworks NOTIFY availableNetworksChanged)
    Q_PROPERTY(bool online READ isConnected NOTIFY isConnectedChanged)
    Q_PROPERTY(bool wireless READ wireless NOTIFY connectionTypeChanged)
    Q_PROPERTY(QString interfaceName READ interfaceName NOTIFY interfaceNameChanged)
    Q_PROPERTY(QString ipv4Address READ ipv4Address NOTIFY ipv4AddressChanged)
    Q_PROPERTY(QString iconSource READ iconSource NOTIFY iconSourceChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QStringList availableNetworkNames READ availableNetworkNames NOTIFY availableNetworksChanged)

public:
    explicit NetworkManager(QObject *parent = nullptr);
    ~NetworkManager();

    int signalStrength() const;
    QString connectionType() const;
    bool isConnected() const;
    QString networkName() const;
    QAbstractListModel* availableNetworks() const;
    bool hasAvailableNetworks() const;
    bool wireless() const;
    QString interfaceName() const;
    QString ipv4Address() const;
    QString iconSource() const;
    QString statusText() const;
    QStringList availableNetworkNames() const;

    Q_INVOKABLE void updateNetworkStatus();
    Q_INVOKABLE void scanAvailableNetworks();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void refreshAvailableNetworks();

signals:
    void signalStrengthChanged();
    void connectionTypeChanged();
    void isConnectedChanged();
    void networkNameChanged();
    void interfaceNameChanged();
    void ipv4AddressChanged();
    void iconSourceChanged();
    void statusTextChanged();
    void availableNetworksChanged();

private:
    void setupTimer();
    void setupDbusSignalSubscriptions();
    void refreshDbusDeviceSubscriptions();
    void queueRefresh(bool includeAvailableNetworks);
    void queryNetworkManager();
    void queryWiFiSignalStrength();
    void queryAvailableNetworks(bool rescan);
    int normalizeSignalStrength(int percentage);

private slots:
    void onDbusPropertiesChanged(const QString &interfaceName,
                                 const QVariantMap &changedProperties,
                                 const QStringList &invalidatedProperties);
    void onDeviceAdded(const QDBusObjectPath &devicePath);
    void onDeviceRemoved(const QDBusObjectPath &devicePath);
    void onAccessPointAdded(const QDBusObjectPath &apPath);
    void onAccessPointRemoved(const QDBusObjectPath &apPath);

private:
    int m_signalStrength = 0;
    QString m_connectionType = "unknown";
    bool m_isConnected = false;
    QString m_networkName = "N/A";
    QString m_interfaceName = "n/a";
    QString m_ipv4Address = "n/a";
    bool m_hasAvailableNetworks = false;
    QTimer *m_updateTimer = nullptr;
    QTimer *m_dbusRefreshTimer = nullptr;
    bool m_pendingAvailableRefresh = false;
    QStringList m_subscribedWifiDevices;
    AvailableNetworksModel *m_availableNetworksModel = nullptr;
};
