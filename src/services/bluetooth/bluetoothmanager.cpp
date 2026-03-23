#include "bluetoothmanager.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>
#include <QProcess>
#include <QVariantMap>

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

void BluetoothManager::refresh()
{
    const bool oldAvailable = m_available;
    const bool oldPowered = m_powered;
    const QString oldStatus = m_statusText;
    const QString oldIcon = m_iconName;
    const QStringList oldConnectedDevices = m_connectedDevices;
    const QVariantList oldConnectedItems = m_connectedDeviceItems;

    const QString adapterPath = detectAdapterPath();
    if (adapterPath.isEmpty()) {
        m_available = false;
        m_powered = false;
        m_statusText = QStringLiteral("Bluetooth unavailable");
        m_iconName = QStringLiteral("bluetooth-disabled-symbolic");
    } else {
        bool ok = false;
        const bool p = readPowered(adapterPath, &ok);
        m_available = ok;
        m_powered = ok && p;
        if (!m_available) {
            m_statusText = QStringLiteral("Bluetooth unavailable");
            m_iconName = QStringLiteral("bluetooth-disabled-symbolic");
            m_connectedDevices.clear();
            m_connectedDeviceItems.clear();
        } else if (m_powered) {
            m_connectedDeviceItems = queryConnectedDeviceItems();
            m_connectedDevices.clear();
            for (const QVariant &entryVar : m_connectedDeviceItems) {
                const QVariantMap entry = entryVar.toMap();
                const QString name = entry.value(QStringLiteral("name")).toString().trimmed();
                if (!name.isEmpty()) {
                    m_connectedDevices << name;
                }
            }
            if (m_connectedDevices.isEmpty()) {
                m_statusText = QStringLiteral("Bluetooth On");
            } else {
                m_statusText = QStringLiteral("Bluetooth On (%1 connected)").arg(m_connectedDevices.size());
            }
            m_iconName = QStringLiteral("bluetooth-active-symbolic");
        } else {
            m_statusText = QStringLiteral("Bluetooth Off");
            m_iconName = QStringLiteral("bluetooth-disabled-symbolic");
            m_connectedDevices.clear();
            m_connectedDeviceItems.clear();
        }
    }

    if (oldAvailable != m_available ||
        oldPowered != m_powered ||
        oldStatus != m_statusText ||
        oldIcon != m_iconName ||
        oldConnectedDevices != m_connectedDevices ||
        oldConnectedItems != m_connectedDeviceItems) {
        emit changed();
    }
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

    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(QStringLiteral("bluetoothctl"), QStringList() << QStringLiteral("disconnect") << mac);
    if (!proc.waitForFinished(2000) || proc.exitCode() != 0) {
        return false;
    }

    refresh();
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
