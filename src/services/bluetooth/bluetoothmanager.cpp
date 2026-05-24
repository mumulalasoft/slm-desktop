#include "bluetoothmanager.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>
#include <QProcess>
#include <QVariantMap>
#include <QtConcurrent>

namespace {

struct BluetoothSnapshot {
    bool available = false;
    bool powered = false;
    QString statusText;
    QString iconName;
    QStringList connectedDevices;
    QVariantList connectedDeviceItems;
};

} // namespace

namespace {
constexpr const char *kBluezService = "org.bluez";
constexpr const char *kObjManagerIface = "org.freedesktop.DBus.ObjectManager";
constexpr const char *kAdapterIface = "org.bluez.Adapter1";
constexpr const char *kPropsIface = "org.freedesktop.DBus.Properties";
}

BluetoothManager::BluetoothManager(QObject *parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(5000);
    connect(m_timer, &QTimer::timeout, this, &BluetoothManager::refresh);
    m_timer->start();

    m_realtimeRefreshTimer = new QTimer(this);
    m_realtimeRefreshTimer->setSingleShot(true);
    m_realtimeRefreshTimer->setInterval(150);
    connect(m_realtimeRefreshTimer, &QTimer::timeout, this, &BluetoothManager::refresh);

    QDBusConnection systemBus = QDBusConnection::systemBus();
    const bool propsConnected = systemBus.connect(QString(),
                                                  QString(),
                                                  QString::fromLatin1(kPropsIface),
                                                  QStringLiteral("PropertiesChanged"),
                                                  this,
                                                  SLOT(onBluezPropertiesChanged(QString,QVariantMap,QStringList)));
    const bool addedConnected = systemBus.connect(QString::fromLatin1(kBluezService),
                                                  QStringLiteral("/"),
                                                  QString::fromLatin1(kObjManagerIface),
                                                  QStringLiteral("InterfacesAdded"),
                                                  this,
                                                  SLOT(onBluezInterfacesAdded(QDBusObjectPath,QVariantMap)));
    const bool removedConnected = systemBus.connect(QString::fromLatin1(kBluezService),
                                                    QStringLiteral("/"),
                                                    QString::fromLatin1(kObjManagerIface),
                                                    QStringLiteral("InterfacesRemoved"),
                                                    this,
                                                    SLOT(onBluezInterfacesRemoved(QDBusObjectPath,QStringList)));
    const bool realtimeSignalsReady = propsConnected && addedConnected && removedConnected;
    if (!realtimeSignalsReady) {
        // Fallback polling lebih rapat saat event subscription DBus gagal.
        m_timer->setInterval(1500);
    }

    refresh();
}

bool BluetoothManager::available() const
{
    return m_available;
}

bool BluetoothManager::powered() const
{
    return m_powered;
}

QString BluetoothManager::statusText() const
{
    return m_statusText;
}

QString BluetoothManager::iconName() const
{
    return m_iconName;
}

QStringList BluetoothManager::connectedDevices() const
{
    return m_connectedDevices;
}

QVariantList BluetoothManager::connectedDeviceItems() const
{
    return m_connectedDeviceItems;
}

void BluetoothManager::scheduleRefresh()
{
    if (!m_realtimeRefreshTimer) {
        refresh();
        return;
    }
    m_realtimeRefreshTimer->start();
}

void BluetoothManager::onBluezPropertiesChanged(const QString &interfaceName,
                                                const QVariantMap &changedProperties,
                                                const QStringList &invalidatedProperties)
{
    const bool adapterSignal = (interfaceName == QString::fromLatin1(kAdapterIface));
    const bool deviceSignal = (interfaceName == QStringLiteral("org.bluez.Device1"));
    if (!adapterSignal && !deviceSignal) {
        return;
    }

    if (adapterSignal) {
        if (changedProperties.contains(QStringLiteral("Powered")) ||
            changedProperties.contains(QStringLiteral("Discovering")) ||
            invalidatedProperties.contains(QStringLiteral("Powered")) ||
            invalidatedProperties.contains(QStringLiteral("Discovering"))) {
            scheduleRefresh();
        }
        return;
    }

    if (changedProperties.contains(QStringLiteral("Connected")) ||
        changedProperties.contains(QStringLiteral("Alias")) ||
        changedProperties.contains(QStringLiteral("Name")) ||
        changedProperties.contains(QStringLiteral("Paired")) ||
        changedProperties.contains(QStringLiteral("ServicesResolved")) ||
        invalidatedProperties.contains(QStringLiteral("Connected")) ||
        invalidatedProperties.contains(QStringLiteral("Alias")) ||
        invalidatedProperties.contains(QStringLiteral("Name")) ||
        invalidatedProperties.contains(QStringLiteral("Paired")) ||
        invalidatedProperties.contains(QStringLiteral("ServicesResolved"))) {
        scheduleRefresh();
    }
}

void BluetoothManager::onBluezInterfacesAdded(const QDBusObjectPath &objectPath,
                                              const QVariantMap &interfacesAndProperties)
{
    if (!objectPath.path().startsWith(QStringLiteral("/org/bluez/"))) {
        return;
    }

    if (interfacesAndProperties.contains(QString::fromLatin1(kAdapterIface)) ||
        interfacesAndProperties.contains(QStringLiteral("org.bluez.Device1"))) {
        scheduleRefresh();
    }
}

void BluetoothManager::onBluezInterfacesRemoved(const QDBusObjectPath &objectPath,
                                                const QStringList &interfaces)
{
    if (!objectPath.path().startsWith(QStringLiteral("/org/bluez/"))) {
        return;
    }

    if (interfaces.contains(QString::fromLatin1(kAdapterIface)) ||
        interfaces.contains(QStringLiteral("org.bluez.Device1"))) {
        scheduleRefresh();
    }
}

void BluetoothManager::refresh()
{
    if (m_refreshPending) {
        return;
    }
    m_refreshPending = true;

    auto *watcher = new QFutureWatcher<BluetoothSnapshot>(this);
    connect(watcher, &QFutureWatcher<BluetoothSnapshot>::finished, this, [this, watcher]() {
        BluetoothSnapshot snap = watcher->result();
        watcher->deleteLater();
        m_refreshPending = false;

        const bool changed =
            m_available           != snap.available           ||
            m_powered             != snap.powered             ||
            m_statusText          != snap.statusText          ||
            m_iconName            != snap.iconName            ||
            m_connectedDevices    != snap.connectedDevices    ||
            m_connectedDeviceItems != snap.connectedDeviceItems;

        m_available            = snap.available;
        m_powered              = snap.powered;
        m_statusText           = snap.statusText;
        m_iconName             = snap.iconName;
        m_connectedDevices     = snap.connectedDevices;
        m_connectedDeviceItems = snap.connectedDeviceItems;

        if (changed) {
            emit this->changed();
        }
    });

    watcher->setFuture(QtConcurrent::run([this]() -> BluetoothSnapshot {
        BluetoothSnapshot snap;
        const QString adapterPath = detectAdapterPath();
        if (adapterPath.isEmpty()) {
            snap.statusText = QStringLiteral("Bluetooth unavailable");
            snap.iconName   = QStringLiteral("bluetooth-disabled-symbolic");
            return snap;
        }

        bool ok = false;
        const bool p = readPowered(adapterPath, &ok);
        snap.available = ok;
        snap.powered   = ok && p;

        if (!snap.available) {
            snap.statusText = QStringLiteral("Bluetooth unavailable");
            snap.iconName   = QStringLiteral("bluetooth-disabled-symbolic");
        } else if (snap.powered) {
            snap.connectedDeviceItems = queryConnectedDeviceItems();
            for (const QVariant &entryVar : snap.connectedDeviceItems) {
                const QVariantMap entry = entryVar.toMap();
                const QString name = entry.value(QStringLiteral("name")).toString().trimmed();
                if (!name.isEmpty()) {
                    snap.connectedDevices << name;
                }
            }
            snap.statusText = snap.connectedDevices.isEmpty()
                ? QStringLiteral("Bluetooth On")
                : QStringLiteral("Bluetooth On (%1 connected)").arg(snap.connectedDevices.size());
            snap.iconName = QStringLiteral("bluetooth-active-symbolic");
        } else {
            snap.statusText = QStringLiteral("Bluetooth Off");
            snap.iconName   = QStringLiteral("bluetooth-disabled-symbolic");
        }
        return snap;
    }));
}

bool BluetoothManager::setPowered(bool enabled)
{
    const QString adapterPath = detectAdapterPath();
    if (adapterPath.isEmpty()) {
        return false;
    }

    QDBusInterface props(QString::fromLatin1(kBluezService),
                         adapterPath,
                         QString::fromLatin1(kPropsIface),
                         QDBusConnection::systemBus());
    if (!props.isValid()) {
        return false;
    }

    QDBusReply<void> reply = props.call(QStringLiteral("Set"),
                                        QString::fromLatin1(kAdapterIface),
                                        QStringLiteral("Powered"),
                                        QVariant::fromValue(QDBusVariant(enabled)));
    if (!reply.isValid()) {
        return false;
    }

    refresh();
    return true;
}

bool BluetoothManager::openBluetoothSettings()
{
    const QList<QStringList> commands = {
        {QStringLiteral("gnome-control-center"), QStringLiteral("bluetooth")},
        {QStringLiteral("kcmshell6"), QStringLiteral("kcm_bluetooth")},
        {QStringLiteral("blueman-manager")}
    };

    for (const QStringList &cmd : commands) {
        if (cmd.isEmpty()) {
            continue;
        }
        QString program = cmd.first();
        QStringList args = cmd;
        args.removeFirst();
        if (QProcess::startDetached(program, args)) {
            return true;
        }
    }
    return false;
}

bool BluetoothManager::disconnectDevice(const QString &address)
{
    const QString mac = address.trimmed();
    if (mac.isEmpty()) {
        return false;
    }

    auto *watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher]() {
        watcher->deleteLater();
        refresh();
    });
    watcher->setFuture(QtConcurrent::run([mac]() -> bool {
        QProcess proc;
        proc.setProcessChannelMode(QProcess::MergedChannels);
        proc.start(QStringLiteral("bluetoothctl"),
                   QStringList() << QStringLiteral("disconnect") << mac);
        return proc.waitForFinished(2000) && proc.exitCode() == 0;
    }));
    return true;
}

QString BluetoothManager::detectAdapterPath() const
{
    for (int i = 0; i < 5; ++i) {
        const QString path = QStringLiteral("/org/bluez/hci%1").arg(i);
        bool ok = false;
        readPowered(path, &ok);
        if (ok) {
            return path;
        }
    }
    return QString();
}

bool BluetoothManager::readPowered(const QString &adapterPath, bool *ok) const
{
    QDBusInterface props(QString::fromLatin1(kBluezService),
                         adapterPath,
                         QString::fromLatin1(kPropsIface),
                         QDBusConnection::systemBus());
    if (!props.isValid()) {
        if (ok) {
            *ok = false;
        }
        return false;
    }

    QDBusReply<QDBusVariant> reply = props.call(QStringLiteral("Get"),
                                                QString::fromLatin1(kAdapterIface),
                                                QStringLiteral("Powered"));
    if (!reply.isValid()) {
        if (ok) {
            *ok = false;
        }
        return false;
    }
    if (ok) {
        *ok = true;
    }
    return reply.value().variant().toBool();
}

QVariantList BluetoothManager::queryConnectedDeviceItems() const
{
    QVariantList devices;
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(QStringLiteral("bluetoothctl"), QStringList() << QStringLiteral("devices") << QStringLiteral("Connected"));
    if (!proc.waitForFinished(1200) || proc.exitCode() != 0) {
        return devices;
    }

    const QString output = QString::fromUtf8(proc.readAllStandardOutput());
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (!trimmed.startsWith(QStringLiteral("Device "))) {
            continue;
        }
        const QStringList parts = trimmed.split(' ', Qt::SkipEmptyParts);
        if (parts.size() < 4) {
            continue;
        }
        const QString address = parts.at(1).trimmed();
        const QString name = parts.mid(2).join(QStringLiteral(" "));
        if (address.isEmpty() || name.isEmpty()) {
            continue;
        }
        QVariantMap item;
        item.insert(QStringLiteral("address"), address);
        item.insert(QStringLiteral("name"), name);
        devices << item;
    }
    return devices;
}
