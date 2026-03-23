#include "networkmanager.h"
#include "../../../resourcepaths.h"
#include <QProcess>
#include <QDebug>
#include <QTimer>
#include <QNetworkInterface>
#include <QHostAddress>
#include <QRegularExpression>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDBusArgument>
#include <algorithm>

namespace {
constexpr const char *kNmService = "org.freedesktop.NetworkManager";
constexpr const char *kNmPath = "/org/freedesktop/NetworkManager";
constexpr const char *kNmIface = "org.freedesktop.NetworkManager";
constexpr uint kNmDeviceTypeEthernet = 1;
constexpr uint kNmDeviceTypeWifi = 2;
const QString kNmPathStr = QString::fromLatin1(kNmPath);
const QString kNmIfaceStr = QString::fromLatin1(kNmIface);

bool isWirelessInterfaceName(const QString &ifaceName)
{
    const QString lower = ifaceName.trimmed().toLower();
    return lower.startsWith(QStringLiteral("wl")) ||
           lower.contains(QStringLiteral("wifi")) ||
           lower.contains(QStringLiteral("wlan"));
}

bool isEthernetInterfaceName(const QString &ifaceName)
{
    const QString lower = ifaceName.trimmed().toLower();
    return lower.startsWith(QStringLiteral("en")) ||
           lower.startsWith(QStringLiteral("eth")) ||
           lower.startsWith(QStringLiteral("em"));
}

QString detectDefaultRouteInterface()
{
    QProcess ip;
    ip.setProcessChannelMode(QProcess::MergedChannels);
    ip.start(QStringLiteral("ip"), QStringList() << "route" << "show" << "default");
    if (!ip.waitForFinished(400)) {
        return QString();
    }

    const QString output = QString::fromUtf8(ip.readAllStandardOutput());
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        const int devIndex = parts.indexOf(QStringLiteral("dev"));
        if (devIndex >= 0 && devIndex + 1 < parts.size()) {
            return parts.at(devIndex + 1).trimmed();
        }
    }
    return QString();
}

QString detectInterfaceName()
{
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const auto &iface : interfaces) {
        const auto flags = iface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp) ||
            !flags.testFlag(QNetworkInterface::IsRunning) ||
            flags.testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }
        for (const auto &entry : iface.addressEntries()) {
            if (!entry.ip().isNull() && !entry.ip().isLoopback()) {
                return iface.humanReadableName().isEmpty() ? iface.name() : iface.humanReadableName();
            }
        }
    }
    return QStringLiteral("n/a");
}

QString detectIpv4Address()
{
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const auto &iface : interfaces) {
        const auto flags = iface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp) ||
            !flags.testFlag(QNetworkInterface::IsRunning) ||
            flags.testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }
        for (const auto &entry : iface.addressEntries()) {
            const auto ip = entry.ip();
            if (ip.protocol() == QAbstractSocket::IPv4Protocol && !ip.isLoopback() && !ip.isNull()) {
                return ip.toString();
            }
        }
    }
    return QStringLiteral("n/a");
}

QString ipv4ForInterface(const QString &ifaceName)
{
    if (ifaceName.isEmpty()) {
        return QStringLiteral("n/a");
    }

    const auto iface = QNetworkInterface::interfaceFromName(ifaceName);
    if (!iface.isValid()) {
        return QStringLiteral("n/a");
    }

    for (const auto &entry : iface.addressEntries()) {
        const auto ip = entry.ip();
        if (ip.protocol() == QAbstractSocket::IPv4Protocol && !ip.isLoopback() && !ip.isNull()) {
            return ip.toString();
        }
    }
    return QStringLiteral("n/a");
}

QString detectFallbackActiveInterface()
{
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const auto &iface : interfaces) {
        const auto flags = iface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp) ||
            !flags.testFlag(QNetworkInterface::IsRunning) ||
            flags.testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }

        for (const auto &entry : iface.addressEntries()) {
            const auto ip = entry.ip();
            if (ip.protocol() == QAbstractSocket::IPv4Protocol && !ip.isLoopback() && !ip.isNull()) {
                return iface.name();
            }
        }
    }
    return QString();
}

QString objectPathFromVariant(const QVariant &value)
{
    if (value.metaType().id() == qMetaTypeId<QDBusObjectPath>()) {
        return value.value<QDBusObjectPath>().path();
    }

    if (value.metaType().id() == qMetaTypeId<QDBusArgument>()) {
        const QDBusArgument arg = value.value<QDBusArgument>();
        QDBusObjectPath path;
        arg >> path;
        return path.path();
    }

    const QString asString = value.toString().trimmed();
    return asString;
}

QStringList objectPathListFromVariant(const QVariant &value)
{
    QStringList out;

    if (value.metaType().id() == qMetaTypeId<QList<QDBusObjectPath>>()) {
        const auto paths = value.value<QList<QDBusObjectPath>>();
        for (const auto &p : paths) {
            if (!p.path().isEmpty()) {
                out.push_back(p.path());
            }
        }
        return out;
    }

    if (value.metaType().id() == qMetaTypeId<QDBusArgument>()) {
        const QDBusArgument arg = value.value<QDBusArgument>();
        arg.beginArray();
        while (!arg.atEnd()) {
            QDBusObjectPath path;
            arg >> path;
            if (!path.path().isEmpty()) {
                out.push_back(path.path());
            }
        }
        arg.endArray();
        return out;
    }

    const QVariantList list = value.toList();
    for (const QVariant &v : list) {
        const QString path = objectPathFromVariant(v);
        if (!path.isEmpty()) {
            out.push_back(path);
        }
    }
    return out;
}

QVariant dbusGetProperty(const QString &path, const QString &interface, const QString &property)
{
    QDBusInterface props(kNmService,
                         path,
                         QStringLiteral("org.freedesktop.DBus.Properties"),
                         QDBusConnection::systemBus());
    if (!props.isValid()) {
        return QVariant();
    }

    QDBusReply<QDBusVariant> reply = props.call(QStringLiteral("Get"), interface, property);
    if (!reply.isValid()) {
        return QVariant();
    }
    return reply.value().variant();
}

QString ssidFromVariant(const QVariant &value)
{
    QByteArray bytes;
    if (value.metaType().id() == QMetaType::QByteArray) {
        bytes = value.toByteArray();
    } else if (value.metaType().id() == qMetaTypeId<QDBusArgument>()) {
        const QDBusArgument arg = value.value<QDBusArgument>();
        arg.beginArray();
        while (!arg.atEnd()) {
            uchar b = 0;
            arg >> b;
            bytes.append(static_cast<char>(b));
        }
        arg.endArray();
    } else if (value.canConvert<QString>()) {
        return value.toString().trimmed();
    }

    const QString ssid = QString::fromUtf8(bytes).trimmed();
    return ssid;
}

QString ipv4FromConfigPath(const QString &ip4ConfigPath)
{
    if (ip4ConfigPath.isEmpty() || ip4ConfigPath == QStringLiteral("/")) {
        return QStringLiteral("n/a");
    }

    const QVariant addressData = dbusGetProperty(ip4ConfigPath,
                                                 QStringLiteral("org.freedesktop.NetworkManager.IP4Config"),
                                                 QStringLiteral("AddressData"));

    const QVariantList entries = addressData.toList();
    for (const QVariant &entry : entries) {
        const QVariantMap map = entry.toMap();
        const QString addr = map.value(QStringLiteral("address")).toString().trimmed();
        if (!addr.isEmpty()) {
            return addr;
        }
    }
    return QStringLiteral("n/a");
}

int wifiStrengthFromApPath(const QString &apPath)
{
    if (apPath.isEmpty() || apPath == QStringLiteral("/")) {
        return 0;
    }

    const QVariant strength = dbusGetProperty(apPath,
                                              QStringLiteral("org.freedesktop.NetworkManager.AccessPoint"),
                                              QStringLiteral("Strength"));
    return qBound(0, strength.toInt(), 100);
}
} // namespace

// AvailableNetworksModel Implementation
AvailableNetworksModel::AvailableNetworksModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int AvailableNetworksModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_networks.size();
}

QVariant AvailableNetworksModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_networks.size())
        return QVariant();

    const AvailableNetwork &network = m_networks.at(index.row());

    switch (role) {
    case SsidRole:
        return network.ssid;
    case SignalStrengthRole:
        return network.signalStrength;
    case IsSecureRole:
        return network.isSecure;
    case IsActiveRole:
        return network.isActive;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> AvailableNetworksModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[SsidRole] = "ssid";
    roles[SignalStrengthRole] = "signalStrength";
    roles[IsSecureRole] = "isSecure";
    roles[IsActiveRole] = "isActive";
    return roles;
}

void AvailableNetworksModel::setNetworks(const QVector<AvailableNetwork> &networks)
{
    beginResetModel();
    m_networks = networks;
    endResetModel();
}

// NetworkManager Implementation
NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent)
{
    m_availableNetworksModel = new AvailableNetworksModel(this);
    setupTimer();
    setupDbusSignalSubscriptions();
    updateNetworkStatus();
    scanAvailableNetworks();
}

NetworkManager::~NetworkManager()
{
    if (m_updateTimer) {
        m_updateTimer->stop();
        m_updateTimer->deleteLater();
    }
}

int NetworkManager::signalStrength() const
{
    return m_signalStrength;
}

QString NetworkManager::connectionType() const
{
    return m_connectionType;
}

bool NetworkManager::isConnected() const
{
    return m_isConnected;
}

// bool isAvailable() const
// {
//     return m_connectionType != "none";
// }

QString NetworkManager::networkName() const
{
    return m_networkName;
}

QAbstractListModel* NetworkManager::availableNetworks() const
{
    return m_availableNetworksModel;
}

bool NetworkManager::hasAvailableNetworks() const
{
    return m_hasAvailableNetworks;
}

bool NetworkManager::wireless() const
{
    return m_connectionType == QStringLiteral("wifi");
}

QString NetworkManager::interfaceName() const
{
    return m_interfaceName;
}

QString NetworkManager::ipv4Address() const
{
    return m_ipv4Address;
}

QString NetworkManager::iconSource() const
{
    if (!m_isConnected) {
        return ResourcePaths::Icons::wifiLow();
    }
    if (m_connectionType == QStringLiteral("ethernet")) {
        return ResourcePaths::Icons::ethernet();
    }
    if (m_signalStrength < 35) {
        return ResourcePaths::Icons::wifiLow();
    }
    if (m_signalStrength < 70) {
        return ResourcePaths::Icons::wifiMedium();
    }
    return ResourcePaths::Icons::wifiHigh();
}

QString NetworkManager::statusText() const
{
    if (!m_isConnected) {
        return QStringLiteral("Offline");
    }
    if (m_connectionType == QStringLiteral("ethernet")) {
        return QStringLiteral("Ethernet");
    }
    if (m_connectionType == QStringLiteral("wifi")) {
        if (m_networkName.isEmpty() || m_networkName == QStringLiteral("N/A")) {
            return QStringLiteral("Wi-Fi");
        }
        return m_networkName;
    }
    return QStringLiteral("Connected");
}

QStringList NetworkManager::availableNetworkNames() const
{
    QStringList names;
    if (!m_availableNetworksModel) {
        return names;
    }

    for (int row = 0; row < m_availableNetworksModel->rowCount(); ++row) {
        const QModelIndex idx = m_availableNetworksModel->index(row, 0);
        const QString ssid = m_availableNetworksModel->data(idx, AvailableNetworksModel::SsidRole).toString();
        names << ssid;
    }
    return names;
}

void NetworkManager::updateNetworkStatus()
{
    queryNetworkManager();
}

void NetworkManager::scanAvailableNetworks()
{
    queryAvailableNetworks(true);
}

void NetworkManager::refresh()
{
    updateNetworkStatus();
}

void NetworkManager::refreshAvailableNetworks()
{
    scanAvailableNetworks();
}

void NetworkManager::setupTimer()
{
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, [this]() {
        updateNetworkStatus();
        queryAvailableNetworks(false);
    });
    // Fallback polling only; realtime updates are driven by DBus signals.
    m_updateTimer->start(15000);
}

void NetworkManager::setupDbusSignalSubscriptions()
{
    QDBusConnection bus = QDBusConnection::systemBus();

    const bool connectedProps = bus.connect(QString::fromLatin1(kNmService),
                                            QString::fromLatin1(kNmPath),
                                            QStringLiteral("org.freedesktop.DBus.Properties"),
                                            QStringLiteral("PropertiesChanged"),
                                            this,
                                            SLOT(onDbusPropertiesChanged(QString,QVariantMap,QStringList)));
    if (!connectedProps) {
        qWarning() << "NetworkManager DBus: failed to subscribe PropertiesChanged on NM root";
    }

    const bool connectedAdded = bus.connect(QString::fromLatin1(kNmService),
                                            QString::fromLatin1(kNmPath),
                                            QString::fromLatin1(kNmIface),
                                            QStringLiteral("DeviceAdded"),
                                            this,
                                            SLOT(onDeviceAdded(QDBusObjectPath)));
    if (!connectedAdded) {
        qWarning() << "NetworkManager DBus: failed to subscribe DeviceAdded";
    }

    const bool connectedRemoved = bus.connect(QString::fromLatin1(kNmService),
                                              QString::fromLatin1(kNmPath),
                                              QString::fromLatin1(kNmIface),
                                              QStringLiteral("DeviceRemoved"),
                                              this,
                                              SLOT(onDeviceRemoved(QDBusObjectPath)));
    if (!connectedRemoved) {
        qWarning() << "NetworkManager DBus: failed to subscribe DeviceRemoved";
    }

    m_dbusRefreshTimer = new QTimer(this);
    m_dbusRefreshTimer->setSingleShot(true);
    m_dbusRefreshTimer->setInterval(120);
    connect(m_dbusRefreshTimer, &QTimer::timeout, this, [this]() {
        updateNetworkStatus();
        if (m_pendingAvailableRefresh) {
            queryAvailableNetworks(false);
            m_pendingAvailableRefresh = false;
        }
    });

    refreshDbusDeviceSubscriptions();
}

void NetworkManager::refreshDbusDeviceSubscriptions()
{
    QDBusConnection bus = QDBusConnection::systemBus();
    const QString nmService = QString::fromLatin1(kNmService);
    const QString nmPath = QString::fromLatin1(kNmPath);
    const QString nmIface = QString::fromLatin1(kNmIface);
    const QString devIface = QStringLiteral("org.freedesktop.NetworkManager.Device");
    const QString wifiIface = QStringLiteral("org.freedesktop.NetworkManager.Device.Wireless");

    const QStringList devicePaths = objectPathListFromVariant(
        dbusGetProperty(nmPath, nmIface, QStringLiteral("Devices")));

    QStringList wifiDevices;
    for (const QString &devicePath : devicePaths) {
        const uint deviceType = dbusGetProperty(devicePath, devIface, QStringLiteral("DeviceType")).toUInt();
        if (deviceType == kNmDeviceTypeWifi) {
            wifiDevices << devicePath;
        }
    }

    for (const QString &oldPath : m_subscribedWifiDevices) {
        if (wifiDevices.contains(oldPath)) {
            continue;
        }
        bus.disconnect(nmService,
                       oldPath,
                       QStringLiteral("org.freedesktop.DBus.Properties"),
                       QStringLiteral("PropertiesChanged"),
                       this,
                       SLOT(onDbusPropertiesChanged(QString,QVariantMap,QStringList)));
        bus.disconnect(nmService,
                       oldPath,
                       wifiIface,
                       QStringLiteral("AccessPointAdded"),
                       this,
                       SLOT(onAccessPointAdded(QDBusObjectPath)));
        bus.disconnect(nmService,
                       oldPath,
                       wifiIface,
                       QStringLiteral("AccessPointRemoved"),
                       this,
                       SLOT(onAccessPointRemoved(QDBusObjectPath)));
    }

    for (const QString &newPath : wifiDevices) {
        if (m_subscribedWifiDevices.contains(newPath)) {
            continue;
        }
        bus.connect(nmService,
                    newPath,
                    QStringLiteral("org.freedesktop.DBus.Properties"),
                    QStringLiteral("PropertiesChanged"),
                    this,
                    SLOT(onDbusPropertiesChanged(QString,QVariantMap,QStringList)));
        bus.connect(nmService,
                    newPath,
                    wifiIface,
                    QStringLiteral("AccessPointAdded"),
                    this,
                    SLOT(onAccessPointAdded(QDBusObjectPath)));
        bus.connect(nmService,
                    newPath,
                    wifiIface,
                    QStringLiteral("AccessPointRemoved"),
                    this,
                    SLOT(onAccessPointRemoved(QDBusObjectPath)));
    }

    m_subscribedWifiDevices = wifiDevices;
}

void NetworkManager::queueRefresh(bool includeAvailableNetworks)
{
    m_pendingAvailableRefresh = m_pendingAvailableRefresh || includeAvailableNetworks;
    if (m_dbusRefreshTimer) {
        m_dbusRefreshTimer->start();
    } else {
        updateNetworkStatus();
        if (m_pendingAvailableRefresh) {
            queryAvailableNetworks(false);
            m_pendingAvailableRefresh = false;
        }
    }
}

void NetworkManager::onDbusPropertiesChanged(const QString &interfaceName,
                                             const QVariantMap &changedProperties,
                                             const QStringList &invalidatedProperties)
{
    Q_UNUSED(changedProperties)
    Q_UNUSED(invalidatedProperties)
    const bool includeAvailable = interfaceName.contains(QStringLiteral("Wireless")) ||
                                  interfaceName.contains(QStringLiteral("NetworkManager")) ||
                                  interfaceName.contains(QStringLiteral("AccessPoint"));
    queueRefresh(includeAvailable);
}

void NetworkManager::onDeviceAdded(const QDBusObjectPath &devicePath)
{
    Q_UNUSED(devicePath)
    refreshDbusDeviceSubscriptions();
    queueRefresh(true);
}

void NetworkManager::onDeviceRemoved(const QDBusObjectPath &devicePath)
{
    Q_UNUSED(devicePath)
    refreshDbusDeviceSubscriptions();
    queueRefresh(true);
}

void NetworkManager::onAccessPointAdded(const QDBusObjectPath &apPath)
{
    Q_UNUSED(apPath)
    queueRefresh(true);
}

void NetworkManager::onAccessPointRemoved(const QDBusObjectPath &apPath)
{
    Q_UNUSED(apPath)
    queueRefresh(true);
}

int NetworkManager::normalizeSignalStrength(int percentage)
{
    return qBound(0, percentage, 100);
}

void NetworkManager::queryNetworkManager()
{
    const bool wasConnected = m_isConnected;
    const QString oldConnectionType = m_connectionType;
    const QString oldNetworkName = m_networkName;
    const int oldSignalStrength = m_signalStrength;
    const QString oldInterfaceName = m_interfaceName;
    const QString oldIpv4Address = m_ipv4Address;
    const QString oldIconSource = iconSource();

    m_isConnected = false;
    m_signalStrength = 0;
    m_connectionType = "none";
    m_networkName = "N/A";
    m_interfaceName = detectInterfaceName();
    m_ipv4Address = detectIpv4Address();

    const QDBusConnection bus = QDBusConnection::systemBus();
    QDBusInterface nm(kNmService, kNmPath, kNmIface, bus);

    if (nm.isValid()) {
        QString activeConnPath = objectPathFromVariant(
            dbusGetProperty(kNmPathStr,
                            kNmIfaceStr,
                            QStringLiteral("PrimaryConnection")));

        if (activeConnPath.isEmpty() || activeConnPath == QStringLiteral("/")) {
            const QStringList activeConnections = objectPathListFromVariant(
                dbusGetProperty(kNmPathStr,
                                kNmIfaceStr,
                                QStringLiteral("ActiveConnections")));
            if (!activeConnections.isEmpty()) {
                activeConnPath = activeConnections.first();
            }
        }

        if (!activeConnPath.isEmpty() && activeConnPath != QStringLiteral("/")) {
            const QString acIface = QStringLiteral("org.freedesktop.NetworkManager.Connection.Active");
            const QString acType = dbusGetProperty(activeConnPath, acIface, QStringLiteral("Type")).toString().trimmed();
            const QString acId = dbusGetProperty(activeConnPath, acIface, QStringLiteral("Id")).toString().trimmed();
            const QStringList devices = objectPathListFromVariant(
                dbusGetProperty(activeConnPath, acIface, QStringLiteral("Devices")));

            m_isConnected = true;
            m_networkName = acId.isEmpty() ? QStringLiteral("Connected") : acId;

            if (acType.contains(QStringLiteral("wireless"), Qt::CaseInsensitive) ||
                acType.contains(QStringLiteral("wifi"), Qt::CaseInsensitive) ||
                acType.contains(QStringLiteral("802-11"), Qt::CaseInsensitive)) {
                m_connectionType = QStringLiteral("wifi");
            } else if (acType.contains(QStringLiteral("ethernet"), Qt::CaseInsensitive) ||
                       acType.contains(QStringLiteral("802-3"), Qt::CaseInsensitive)) {
                m_connectionType = QStringLiteral("ethernet");
                m_signalStrength = 100;
            } else {
                m_connectionType = QStringLiteral("unknown");
                m_signalStrength = 60;
            }

            if (!devices.isEmpty()) {
                const QString devicePath = devices.first();
                const QString devIface = QStringLiteral("org.freedesktop.NetworkManager.Device");
                const QString ifaceName = dbusGetProperty(devicePath, devIface, QStringLiteral("Interface")).toString().trimmed();
                if (!ifaceName.isEmpty()) {
                    m_interfaceName = ifaceName;
                }

                const QString ip4Path = objectPathFromVariant(
                    dbusGetProperty(devicePath, devIface, QStringLiteral("Ip4Config")));
                const QString dbusIp = ipv4FromConfigPath(ip4Path);
                if (dbusIp != QStringLiteral("n/a")) {
                    m_ipv4Address = dbusIp;
                } else if (!ifaceName.isEmpty()) {
                    m_ipv4Address = ipv4ForInterface(ifaceName);
                }

                if (m_connectionType == QStringLiteral("wifi")) {
                    const QString wifiIface = QStringLiteral("org.freedesktop.NetworkManager.Device.Wireless");
                    const QString apPath = objectPathFromVariant(
                        dbusGetProperty(devicePath, wifiIface, QStringLiteral("ActiveAccessPoint")));
                    if (!apPath.isEmpty() && apPath != QStringLiteral("/")) {
                        const QString ssid = ssidFromVariant(
                            dbusGetProperty(apPath,
                                            QStringLiteral("org.freedesktop.NetworkManager.AccessPoint"),
                                            QStringLiteral("Ssid")));
                        if (!ssid.isEmpty()) {
                            m_networkName = ssid;
                        }
                        m_signalStrength = wifiStrengthFromApPath(apPath);
                    }
                }
            }
        }
    } else {
        QString activeIface = detectDefaultRouteInterface();
        if (activeIface.isEmpty()) {
            activeIface = detectFallbackActiveInterface();
        }

        if (!activeIface.isEmpty()) {
            m_isConnected = true;
            m_interfaceName = activeIface;
            m_ipv4Address = ipv4ForInterface(activeIface);

            if (isWirelessInterfaceName(activeIface)) {
                m_connectionType = QStringLiteral("wifi");
                m_networkName = QStringLiteral("Wi-Fi");
                m_signalStrength = 0;
            } else if (isEthernetInterfaceName(activeIface)) {
                m_connectionType = QStringLiteral("ethernet");
                m_networkName = QStringLiteral("Ethernet");
                m_signalStrength = 100;
            } else {
                m_connectionType = QStringLiteral("unknown");
                m_networkName = QStringLiteral("Connected");
                m_signalStrength = 60;
            }
        }
    }

    // Emit signals for changed properties
    if (m_isConnected != wasConnected) {
        emit isConnectedChanged();
    }
    if (m_connectionType != oldConnectionType) {
        emit connectionTypeChanged();
        emit statusTextChanged();
    }
    if (m_networkName != oldNetworkName) {
        emit networkNameChanged();
        emit statusTextChanged();
    }
    if (m_signalStrength != oldSignalStrength) {
        emit signalStrengthChanged();
    }
    if (m_interfaceName != oldInterfaceName) {
        emit interfaceNameChanged();
    }
    if (m_ipv4Address != oldIpv4Address) {
        emit ipv4AddressChanged();
    }
    if (m_isConnected != wasConnected) {
        emit statusTextChanged();
    }
    if (iconSource() != oldIconSource) {
        emit iconSourceChanged();
    }
}

void NetworkManager::queryAvailableNetworks(bool rescan)
{
    QVector<AvailableNetwork> networks;
    QHash<QString, AvailableNetwork> bySsid;
    const QDBusConnection bus = QDBusConnection::systemBus();
    QDBusInterface nm(kNmService, kNmPath, kNmIface, bus);
    if (!nm.isValid()) {
        m_availableNetworksModel->setNetworks(networks);
        m_hasAvailableNetworks = false;
        emit availableNetworksChanged();
        return;
    }

    const QStringList devicePaths = objectPathListFromVariant(
        dbusGetProperty(kNmPathStr,
                        kNmIfaceStr,
                        QStringLiteral("Devices")));
    const QString apIface = QStringLiteral("org.freedesktop.NetworkManager.AccessPoint");
    const QString devIface = QStringLiteral("org.freedesktop.NetworkManager.Device");
    const QString wifiIface = QStringLiteral("org.freedesktop.NetworkManager.Device.Wireless");

    for (const QString &devicePath : devicePaths) {
        const uint deviceType = dbusGetProperty(devicePath, devIface, QStringLiteral("DeviceType")).toUInt();
        if (deviceType != kNmDeviceTypeWifi) {
            continue;
        }

        if (rescan) {
            QDBusInterface wifiDev(kNmService, devicePath, wifiIface, bus);
            if (wifiDev.isValid()) {
                wifiDev.call(QStringLiteral("RequestScan"), QVariantMap());
            }
        }

        const QString activeApPath = objectPathFromVariant(
            dbusGetProperty(devicePath, wifiIface, QStringLiteral("ActiveAccessPoint")));
        const QStringList apPaths = objectPathListFromVariant(
            dbusGetProperty(devicePath, wifiIface, QStringLiteral("AccessPoints")));

        for (const QString &apPath : apPaths) {
            QString ssid = ssidFromVariant(dbusGetProperty(apPath, apIface, QStringLiteral("Ssid"))).trimmed();
            if (ssid.isEmpty()) {
                ssid = QStringLiteral("<Hidden Network>");
            }

            const int normalized = normalizeSignalStrength(
                dbusGetProperty(apPath, apIface, QStringLiteral("Strength")).toInt());
            const bool isActive = (!activeApPath.isEmpty() && apPath == activeApPath);

            const uint flags = dbusGetProperty(apPath, apIface, QStringLiteral("Flags")).toUInt();
            const uint wpaFlags = dbusGetProperty(apPath, apIface, QStringLiteral("WpaFlags")).toUInt();
            const uint rsnFlags = dbusGetProperty(apPath, apIface, QStringLiteral("RsnFlags")).toUInt();
            const bool isSecure = (wpaFlags != 0 || rsnFlags != 0 || (flags & 0x1u) == 0);

            AvailableNetwork net;
            net.ssid = ssid;
            net.signalStrength = normalized;
            net.isSecure = isSecure;
            net.isActive = isActive;

            const auto it = bySsid.find(ssid);
            if (it == bySsid.end()) {
                bySsid.insert(ssid, net);
            } else {
                const bool existingInUse = it->isActive;
                if ((isActive && !existingInUse) ||
                    (isActive == existingInUse && normalized > it->signalStrength)) {
                    bySsid[ssid] = net;
                }
            }
        }
    }

    for (auto it = bySsid.cbegin(); it != bySsid.cend(); ++it) {
        networks.push_back(it.value());
    }

    // Sort active first, then by signal strength (strongest first).
    std::sort(networks.begin(), networks.end(),
              [](const AvailableNetwork &a, const AvailableNetwork &b) {
                  if (a.isActive != b.isActive) {
                      return a.isActive;
                  }
                  return a.signalStrength > b.signalStrength;
              });

    m_availableNetworksModel->setNetworks(networks);

    // Update availability flag
    bool wasAvailable = m_hasAvailableNetworks;
    m_hasAvailableNetworks = !networks.isEmpty();

    emit availableNetworksChanged();
    if (m_hasAvailableNetworks != wasAvailable) {
        // availableNetworksChanged signal already emitted, which triggers the property update
    }
}

void NetworkManager::queryWiFiSignalStrength()
{
    if (!m_isConnected || m_connectionType != QStringLiteral("wifi")) {
        return;
    }

    const QDBusConnection bus = QDBusConnection::systemBus();
    const QDBusInterface nm(kNmService, kNmPath, kNmIface, bus);
    if (!nm.isValid()) {
        return;
    }

    const QStringList devicePaths = objectPathListFromVariant(
        dbusGetProperty(kNmPathStr,
                        kNmIfaceStr,
                        QStringLiteral("Devices")));

    for (const QString &devicePath : devicePaths) {
        const uint deviceType = dbusGetProperty(devicePath,
                                                QStringLiteral("org.freedesktop.NetworkManager.Device"),
                                                QStringLiteral("DeviceType")).toUInt();
        if (deviceType != kNmDeviceTypeWifi) {
            continue;
        }

        const QString apPath = objectPathFromVariant(
            dbusGetProperty(devicePath,
                            QStringLiteral("org.freedesktop.NetworkManager.Device.Wireless"),
                            QStringLiteral("ActiveAccessPoint")));
        if (!apPath.isEmpty() && apPath != QStringLiteral("/")) {
            m_signalStrength = wifiStrengthFromApPath(apPath);
            return;
        }
    }
}
