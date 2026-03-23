#include "batterymanager.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>
#include <QProcess>

namespace {
constexpr const char *kUpowerService = "org.freedesktop.UPower";
constexpr const char *kUpowerDisplayPath = "/org/freedesktop/UPower/devices/DisplayDevice";
constexpr const char *kUpowerDeviceIface = "org.freedesktop.UPower.Device";
const QString kUpowerServiceStr = QString::fromLatin1(kUpowerService);
const QString kUpowerDisplayPathStr = QString::fromLatin1(kUpowerDisplayPath);
const QString kUpowerDeviceIfaceStr = QString::fromLatin1(kUpowerDeviceIface);

QVariant dbusProperty(const QString &service,
                      const QString &path,
                      const QString &interface,
                      const QString &property)
{
    QDBusInterface props(service,
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
} // namespace

BatteryManager::BatteryManager(QObject *parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(10000);
    connect(m_timer, &QTimer::timeout, this, &BatteryManager::refresh);
    m_timer->start();
    refresh();
}

bool BatteryManager::available() const
{
    return m_available;
}

int BatteryManager::percentage() const
{
    return m_percentage;
}

bool BatteryManager::charging() const
{
    return m_charging;
}

QString BatteryManager::levelText() const
{
    return m_levelText;
}

QString BatteryManager::durationText() const
{
    return m_durationText;
}

QString BatteryManager::iconName() const
{
    return m_iconName;
}

QString BatteryManager::powerProfile() const
{
    return m_powerProfile;
}

bool BatteryManager::canSetPowerProfile() const
{
    return m_canSetPowerProfile;
}

void BatteryManager::refresh()
{
    const bool oldAvailable = m_available;
    const int oldPercentage = m_percentage;
    const bool oldCharging = m_charging;
    const QString oldLevel = m_levelText;
    const QString oldDuration = m_durationText;
    const QString oldIcon = m_iconName;
    const QString oldProfile = m_powerProfile;
    const bool oldCanSetProfile = m_canSetPowerProfile;

    updateBatteryState();
    updatePowerProfile();

    if (oldAvailable != m_available ||
        oldPercentage != m_percentage ||
        oldCharging != m_charging ||
        oldLevel != m_levelText ||
        oldDuration != m_durationText ||
        oldIcon != m_iconName ||
        oldProfile != m_powerProfile ||
        oldCanSetProfile != m_canSetPowerProfile) {
        emit changed();
    }
}

bool BatteryManager::setPowerProfile(const QString &profile)
{
    const QString normalized = profile.trimmed();
    if (normalized.isEmpty()) {
        return false;
    }

    QProcess proc;
    proc.start(QStringLiteral("powerprofilesctl"), QStringList() << QStringLiteral("set") << normalized);
    if (!proc.waitForFinished(1500) || proc.exitCode() != 0) {
        return false;
    }
    refresh();
    return true;
}

bool BatteryManager::openPowerSettings()
{
    const QList<QStringList> commands = {
        {QStringLiteral("gnome-control-center"), QStringLiteral("power")},
        {QStringLiteral("kcmshell6"), QStringLiteral("powerdevilprofilesconfig")},
        {QStringLiteral("xfce4-power-manager-settings")}
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

void BatteryManager::updateBatteryState()
{
    const QVariant isPresentVar = dbusProperty(kUpowerServiceStr,
                                               kUpowerDisplayPathStr,
                                               kUpowerDeviceIfaceStr,
                                               QStringLiteral("IsPresent"));
    const QVariant typeVar = dbusProperty(kUpowerServiceStr,
                                          kUpowerDisplayPathStr,
                                          kUpowerDeviceIfaceStr,
                                          QStringLiteral("Type"));
    const bool isPresent = isPresentVar.toBool();
    const uint type = typeVar.toUInt();
    m_available = isPresent && type == 2u; // 2 = Battery

    if (!m_available) {
        m_percentage = 0;
        m_charging = false;
        m_levelText = QStringLiteral("Battery unavailable");
        m_durationText = QStringLiteral("No battery detected");
        m_iconName = QStringLiteral("battery-missing-symbolic");
        return;
    }

    const double pct = dbusProperty(kUpowerServiceStr,
                                    kUpowerDisplayPathStr,
                                    kUpowerDeviceIfaceStr,
                                    QStringLiteral("Percentage")).toDouble();
    const uint state = dbusProperty(kUpowerServiceStr,
                                    kUpowerDisplayPathStr,
                                    kUpowerDeviceIfaceStr,
                                    QStringLiteral("State")).toUInt();
    const qulonglong timeToEmpty = dbusProperty(kUpowerServiceStr,
                                                kUpowerDisplayPathStr,
                                                kUpowerDeviceIfaceStr,
                                                QStringLiteral("TimeToEmpty")).toULongLong();
    const qulonglong timeToFull = dbusProperty(kUpowerServiceStr,
                                               kUpowerDisplayPathStr,
                                               kUpowerDeviceIfaceStr,
                                               QStringLiteral("TimeToFull")).toULongLong();

    m_percentage = qBound(0, qRound(pct), 100);
    m_charging = (state == 1u || state == 5u);
    const bool full = (state == 4u) || m_percentage >= 100;
    const bool discharging = (state == 2u || state == 6u);

    m_levelText = QStringLiteral("Battery %1%").arg(m_percentage);
    if (full) {
        m_durationText = QStringLiteral("Fully charged");
    } else if (m_charging) {
        if (timeToFull > 0) {
            m_durationText = QStringLiteral("%1 until full").arg(formatDuration(timeToFull));
        } else {
            m_durationText = QStringLiteral("Charging");
        }
    } else if (discharging) {
        if (timeToEmpty > 0) {
            m_durationText = QStringLiteral("%1 remaining").arg(formatDuration(timeToEmpty));
        } else {
            m_durationText = QStringLiteral("On battery");
        }
    } else {
        m_durationText = QStringLiteral("Battery status unknown");
    }

    m_iconName = computeIconName();
}

void BatteryManager::updatePowerProfile()
{
    QProcess proc;
    proc.start(QStringLiteral("powerprofilesctl"), QStringList() << QStringLiteral("get"));
    if (!proc.waitForFinished(1000) || proc.exitCode() != 0) {
        m_canSetPowerProfile = false;
        return;
    }

    const QString profile = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    if (!profile.isEmpty()) {
        m_powerProfile = profile;
    }
    m_canSetPowerProfile = true;
}

QString BatteryManager::formatDuration(qulonglong seconds) const
{
    const qulonglong hours = seconds / 3600;
    const qulonglong minutes = (seconds % 3600) / 60;
    if (hours > 0) {
        return QStringLiteral("%1h %2m").arg(hours).arg(minutes);
    }
    return QStringLiteral("%1m").arg(minutes);
}

QString BatteryManager::computeIconName() const
{
    if (!m_available) {
        return QStringLiteral("battery-missing-symbolic");
    }

    QString level;
    if (m_percentage >= 90) {
        level = QStringLiteral("full");
    } else if (m_percentage >= 70) {
        level = QStringLiteral("good");
    } else if (m_percentage >= 40) {
        level = QStringLiteral("medium");
    } else if (m_percentage >= 15) {
        level = QStringLiteral("low");
    } else {
        level = QStringLiteral("caution");
    }

    if (m_charging) {
        return QStringLiteral("battery-%1-charging-symbolic").arg(level);
    }
    return QStringLiteral("battery-%1-symbolic").arg(level);
}
