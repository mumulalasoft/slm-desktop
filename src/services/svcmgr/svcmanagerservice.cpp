#include "svcmanagerservice.h"

#include <QDBusConnection>
#include <QDBusReply>
#include <QFile>
#include <QDebug>

// Registered shell components. Only these are visible through the API.
static const QList<QPair<QString,QString>> kComponentDefs = {
    // {name,               unitName}
    { QStringLiteral("envd"),          QStringLiteral("slm-envd.service")     },
    { QStringLiteral("loggerd"),       QStringLiteral("slm-loggerd.service")  },
    { QStringLiteral("svcmgrd"),       QStringLiteral("slm-svcmgrd.service")  },
    { QStringLiteral("portald"),       QStringLiteral("slm-portald.service")  },
    { QStringLiteral("clipboardd"),    QStringLiteral("slm-clipboardd.service") },
};

static const QHash<QString, QPair<QString,QString>> kComponentMeta = {
    { QStringLiteral("envd"),       { QStringLiteral("Environment Service"),  QStringLiteral("Manages environment variables for all scopes") } },
    { QStringLiteral("loggerd"),    { QStringLiteral("Logger Service"),       QStringLiteral("Aggregates and streams shell component logs")  } },
    { QStringLiteral("svcmgrd"),    { QStringLiteral("Service Manager"),      QStringLiteral("Monitors and controls shell services")         } },
    { QStringLiteral("portald"),    { QStringLiteral("Portal Service"),       QStringLiteral("XDG portal implementation for the shell")      } },
    { QStringLiteral("clipboardd"), { QStringLiteral("Clipboard Service"),    QStringLiteral("Clipboard history and sync")                   } },
};

SvcManagerService::SvcManagerService(QObject *parent)
    : QObject(parent)
{
    m_pollTimer.setInterval(3000);
    connect(&m_pollTimer, &QTimer::timeout, this, &SvcManagerService::pollAll);
}

SvcManagerService::~SvcManagerService()
{
    m_pollTimer.stop();
}

bool SvcManagerService::start()
{
    initComponents();
    pollAll();
    m_pollTimer.start();
    qInfo() << "[slm-svcmgrd] monitoring" << m_components.size() << "components";
    return true;
}

void SvcManagerService::initComponents()
{
    m_components.clear();
    for (const auto &def : kComponentDefs) {
        ComponentInfo info;
        info.name     = def.first;
        info.unitName = def.second;
        const auto meta = kComponentMeta.value(info.name);
        info.displayName  = meta.first;
        info.description  = meta.second;
        info.status       = QStringLiteral("unknown");
        m_components.append(info);
    }
}

QDBusInterface *SvcManagerService::systemdManager() const
{
    if (!m_systemd || !m_systemd->isValid()) {
        delete m_systemd;
        m_systemd = new QDBusInterface(
            QStringLiteral("org.freedesktop.systemd1"),
            QStringLiteral("/org/freedesktop/systemd1"),
            QStringLiteral("org.freedesktop.systemd1.Manager"),
            QDBusConnection::sessionBus(),
            const_cast<SvcManagerService *>(this));
    }
    return m_systemd;
}

QString SvcManagerService::systemdUnitState(const QString &unitName) const
{
    QDBusInterface *mgr = systemdManager();
    if (!mgr->isValid()) return QStringLiteral("unknown");

    QDBusReply<QDBusObjectPath> reply = mgr->call(QStringLiteral("GetUnit"), unitName);
    if (!reply.isValid()) return QStringLiteral("inactive");

    QDBusInterface unit(
        QStringLiteral("org.freedesktop.systemd1"),
        reply.value().path(),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QDBusConnection::sessionBus());

    QDBusReply<QDBusVariant> stateReply = unit.call(
        QStringLiteral("Get"),
        QStringLiteral("org.freedesktop.systemd1.Unit"),
        QStringLiteral("ActiveState"));

    return stateReply.isValid() ? stateReply.value().variant().toString()
                                : QStringLiteral("unknown");
}

qint64 SvcManagerService::systemdUnitPid(const QString &unitName) const
{
    QDBusInterface *mgr = systemdManager();
    if (!mgr->isValid()) return -1;

    QDBusReply<QDBusObjectPath> reply = mgr->call(QStringLiteral("GetUnit"), unitName);
    if (!reply.isValid()) return -1;

    QDBusInterface unit(
        QStringLiteral("org.freedesktop.systemd1"),
        reply.value().path(),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QDBusConnection::sessionBus());

    QDBusReply<QDBusVariant> pidReply = unit.call(
        QStringLiteral("Get"),
        QStringLiteral("org.freedesktop.systemd1.Service"),
        QStringLiteral("MainPID"));

    return pidReply.isValid() ? pidReply.value().variant().toLongLong() : -1;
}

qint64 SvcManagerService::unitActiveSince(const QString &unitName) const
{
    QDBusInterface *mgr = systemdManager();
    if (!mgr->isValid()) return 0;

    QDBusReply<QDBusObjectPath> reply = mgr->call(QStringLiteral("GetUnit"), unitName);
    if (!reply.isValid()) return 0;

    QDBusInterface unit(
        QStringLiteral("org.freedesktop.systemd1"),
        reply.value().path(),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QDBusConnection::sessionBus());

    QDBusReply<QDBusVariant> tsReply = unit.call(
        QStringLiteral("Get"),
        QStringLiteral("org.freedesktop.systemd1.Unit"),
        QStringLiteral("ActiveEnterTimestamp"));

    // ActiveEnterTimestamp is in microseconds since epoch.
    return tsReply.isValid() ? tsReply.value().variant().toLongLong() / 1000 : 0;
}

void SvcManagerService::readProcStats(ComponentInfo &info) const
{
    if (info.pid <= 0) {
        info.cpuPct = 0.0;
        info.memKb  = 0;
        return;
    }

    // Read memory from /proc/{pid}/status (VmRSS).
    QFile statusFile(QStringLiteral("/proc/%1/status").arg(info.pid));
    if (statusFile.open(QIODevice::ReadOnly)) {
        const QByteArray data = statusFile.readAll();
        for (const QByteArray &line : data.split('\n')) {
            if (line.startsWith("VmRSS:")) {
                const QByteArray val = line.mid(6).trimmed();
                info.memKb = val.split(' ').first().toLongLong();
                break;
            }
        }
    }
    // CPU% is expensive to compute precisely without tracking deltas.
    // We leave it at 0 for now; a future iteration can add delta tracking.
    info.cpuPct = 0.0;
}

void SvcManagerService::updateComponent(ComponentInfo &info)
{
    const QString prevStatus = info.status;
    info.status = systemdUnitState(info.unitName);
    info.pid    = systemdUnitPid(info.unitName);
    info.since  = unitActiveSince(info.unitName);
    readProcStats(info);

    if (info.status != prevStatus) {
        emit componentStateChanged(info.name, info.status);
        if (info.status == QStringLiteral("failed"))
            emit componentCrashed(info.name, info.lastExitCode);
    }
}

void SvcManagerService::pollAll()
{
    for (ComponentInfo &info : m_components)
        updateComponent(info);
}

void SvcManagerService::onSystemdUnitPropertiesChanged(const QString &interface,
                                                       const QVariantMap &changed,
                                                       const QStringList &invalidated)
{
    Q_UNUSED(interface);
    Q_UNUSED(changed);
    Q_UNUSED(invalidated);
    // Keep state consistent with systemd changes and avoid stale UI data.
    pollAll();
}

QVariantList SvcManagerService::listComponents() const
{
    QVariantList result;
    result.reserve(m_components.size());
    for (const ComponentInfo &info : m_components)
        result.append(info.toVariantMap());
    return result;
}

QVariantMap SvcManagerService::getComponent(const QString &name) const
{
    for (const ComponentInfo &info : m_components) {
        if (info.name == name)
            return info.toVariantMap();
    }
    return {};
}

QVariantMap SvcManagerService::restartComponent(const QString &name)
{
    const auto it = std::find_if(m_components.begin(), m_components.end(),
                                  [&name](const ComponentInfo &c) { return c.name == name; });
    if (it == m_components.end())
        return {{ QStringLiteral("ok"), false }, { QStringLiteral("error"), QStringLiteral("Component not found") }};

    QDBusInterface *mgr = systemdManager();
    if (!mgr->isValid())
        return {{ QStringLiteral("ok"), false }, { QStringLiteral("error"), QStringLiteral("systemd not available") }};

    QDBusReply<QDBusObjectPath> reply = mgr->call(
        QStringLiteral("RestartUnit"), it->unitName, QStringLiteral("replace"));

    if (!reply.isValid()) {
        return {{ QStringLiteral("ok"), false },
                { QStringLiteral("error"), mgr->lastError().message() }};
    }

    return {{ QStringLiteral("ok"), true }, { QStringLiteral("error"), QString{} }};
}

QVariantMap SvcManagerService::stopComponent(const QString &name)
{
    const auto it = std::find_if(m_components.begin(), m_components.end(),
                                  [&name](const ComponentInfo &c) { return c.name == name; });
    if (it == m_components.end())
        return {{ QStringLiteral("ok"), false }, { QStringLiteral("error"), QStringLiteral("Component not found") }};

    QDBusInterface *mgr = systemdManager();
    if (!mgr->isValid())
        return {{ QStringLiteral("ok"), false }, { QStringLiteral("error"), QStringLiteral("systemd not available") }};

    QDBusReply<QDBusObjectPath> reply = mgr->call(
        QStringLiteral("StopUnit"), it->unitName, QStringLiteral("replace"));

    if (!reply.isValid()) {
        return {{ QStringLiteral("ok"), false },
                { QStringLiteral("error"), mgr->lastError().message() }};
    }

    return {{ QStringLiteral("ok"), true }, { QStringLiteral("error"), QString{} }};
}
