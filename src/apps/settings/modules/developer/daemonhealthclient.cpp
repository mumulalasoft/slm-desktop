#include "daemonhealthclient.h"

#include <QDBusConnection>
#include <QDBusReply>

namespace {
constexpr auto kService = "org.slm.WorkspaceManager";
constexpr auto kPath = "/org/slm/WorkspaceManager";
constexpr auto kInterface = "org.slm.WorkspaceManager1";
}

DaemonHealthClient::DaemonHealthClient(QObject *parent)
    : QObject(parent)
{
    QDBusConnection::sessionBus().connect(
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("NameOwnerChanged"),
        this,
        SLOT(onNameOwnerChanged(QString, QString, QString)));

    ensureIface();
    refresh();
}

bool DaemonHealthClient::serviceAvailable() const
{
    return m_available;
}

QVariantMap DaemonHealthClient::snapshot() const
{
    return m_snapshot;
}

bool DaemonHealthClient::ensureIface()
{
    if (m_iface && m_iface->isValid()) {
        return true;
    }

    delete m_iface;
    m_iface = new QDBusInterface(QLatin1String(kService),
                                 QLatin1String(kPath),
                                 QLatin1String(kInterface),
                                 QDBusConnection::sessionBus(),
                                 this);

    const bool nowAvailable = m_iface->isValid();
    if (nowAvailable != m_available) {
        m_available = nowAvailable;
        emit serviceAvailableChanged();
    }
    return nowAvailable;
}

void DaemonHealthClient::refresh()
{
    QVariantMap nextSnapshot;
    if (ensureIface()) {
        const QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("DaemonHealthSnapshot"));
        if (reply.isValid()) {
            nextSnapshot = reply.value();
        }
    }

    if (nextSnapshot != m_snapshot) {
        m_snapshot = nextSnapshot;
        emit snapshotChanged();
    }
}

void DaemonHealthClient::onNameOwnerChanged(const QString &name,
                                            const QString &oldOwner,
                                            const QString &newOwner)
{
    if (name != QLatin1String(kService)) {
        return;
    }
    Q_UNUSED(oldOwner)

    if (!newOwner.isEmpty()) {
        ensureIface();
        refresh();
        return;
    }

    delete m_iface;
    m_iface = nullptr;
    if (m_available) {
        m_available = false;
        emit serviceAvailableChanged();
    }
    if (!m_snapshot.isEmpty()) {
        m_snapshot.clear();
        emit snapshotChanged();
    }
}
