#include "portalbridgeservice.h"

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>

static constexpr char kPortalDir[] = "/usr/share/xdg-desktop-portal/portals";
static constexpr char kConfigDir[] = "xdg-desktop-portal";
static constexpr char kConfigFile[] = "portals.conf";

PortalBridgeService::PortalBridgeService(QObject *parent)
    : QObject(parent)
{
    m_configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                   + QStringLiteral("/%1/%2").arg(QLatin1String(kConfigDir),
                                                   QLatin1String(kConfigFile));

    connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &PortalBridgeService::reload);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged,
            this, [this](const QString &) { loadConfig(); emit configChanged(); });

    m_watcher.addPath(QLatin1String(kPortalDir));
    if (QFile::exists(m_configPath))
        m_watcher.addPath(m_configPath);

    reload();
}

void PortalBridgeService::reload()
{
    loadDescriptors();
    loadConfig();
    emit descriptorsChanged();
    emit configChanged();
}

// ── Descriptor loading ─────────────────────────────────────────────────────

void PortalBridgeService::loadDescriptors()
{
    m_descriptors.clear();
    QSet<QString> ifaceSet;

    const QDir dir(QLatin1String(kPortalDir));
    const QStringList entries = dir.entryList({ QStringLiteral("*.portal") },
                                               QDir::Files, QDir::Name);
    for (const QString &fname : entries) {
        const QString path = dir.filePath(fname);
        QSettings ini(path, QSettings::IniFormat);

        ini.beginGroup(QStringLiteral("portal"));
        const QString dbusName = ini.value(QStringLiteral("DBusName")).toString();
        const QStringList ifaces = ini.value(QStringLiteral("Interfaces"))
                                       .toString()
                                       .split(QLatin1Char(';'), Qt::SkipEmptyParts);
        const QStringList useIn = ini.value(QStringLiteral("UseIn"))
                                      .toString()
                                      .split(QLatin1Char(';'), Qt::SkipEmptyParts);
        ini.endGroup();

        if (dbusName.isEmpty())
            continue;

        const QString id = QFileInfo(fname).baseName();

        QVariantMap entry;
        entry[QStringLiteral("id")]       = id;
        entry[QStringLiteral("dbusName")] = dbusName;
        entry[QStringLiteral("ifaces")]   = ifaces;
        entry[QStringLiteral("useIn")]    = useIn;
        m_descriptors.append(entry);

        for (const QString &iface : ifaces)
            ifaceSet.insert(iface);
    }

    m_interfaces = ifaceSet.values();
    m_interfaces.sort();
}

// ── Config loading ─────────────────────────────────────────────────────────

void PortalBridgeService::loadConfig()
{
    m_overrides.clear();
    m_defaultHandler.clear();

    if (!QFile::exists(m_configPath))
        return;

    QSettings ini(m_configPath, QSettings::IniFormat);
    ini.beginGroup(QStringLiteral("preferred"));
    const QStringList keys = ini.childKeys();
    for (const QString &key : keys) {
        const QString val = ini.value(key).toString().trimmed();
        if (key == QLatin1String("default"))
            m_defaultHandler = val;
        else
            m_overrides[key] = val;
    }
    ini.endGroup();
}

// ── Queries ────────────────────────────────────────────────────────────────

QVariantList PortalBridgeService::handlersFor(const QString &iface) const
{
    QVariantList result;
    for (const QVariant &v : m_descriptors) {
        const QVariantMap d = v.toMap();
        if (d.value(QStringLiteral("ifaces")).toStringList().contains(iface))
            result.append(d);
    }
    return result;
}

QString PortalBridgeService::preferredHandler(const QString &iface) const
{
    if (m_overrides.contains(iface))
        return m_overrides.value(iface).toString();
    return m_defaultHandler;
}

// ── portals.conf writing ───────────────────────────────────────────────────

bool PortalBridgeService::setPreferredHandler(const QString &iface, const QString &handler)
{
    // Ensure config directory exists.
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
             + QStringLiteral("/%1").arg(QLatin1String(kConfigDir)));
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        m_lastError = QStringLiteral("Could not create config directory: %1").arg(dir.path());
        return false;
    }

    // Re-read the file fresh to avoid stomping other keys.
    QSettings ini(m_configPath, QSettings::IniFormat);
    ini.beginGroup(QStringLiteral("preferred"));
    if (handler.isEmpty())
        ini.remove(iface);
    else
        ini.setValue(iface, handler);
    ini.endGroup();
    ini.sync();

    if (ini.status() != QSettings::NoError) {
        m_lastError = QStringLiteral("Failed to write portals.conf");
        return false;
    }

    // Watch the file if we just created it.
    if (!m_watcher.files().contains(m_configPath))
        m_watcher.addPath(m_configPath);

    loadConfig();
    emit configChanged();
    return true;
}

bool PortalBridgeService::resetAllHandlers()
{
    if (!QFile::exists(m_configPath))
        return true;

    QSettings ini(m_configPath, QSettings::IniFormat);
    ini.remove(QStringLiteral("preferred"));
    ini.sync();

    if (ini.status() != QSettings::NoError) {
        m_lastError = QStringLiteral("Failed to clear portals.conf");
        return false;
    }

    loadConfig();
    emit configChanged();
    return true;
}
