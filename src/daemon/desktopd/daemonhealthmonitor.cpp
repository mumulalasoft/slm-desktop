#include "daemonhealthmonitor.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QTimer>
#include <QVariantMap>
#include <QtGlobal>
#include <QDebug>
#include <QDateTime>

namespace {
constexpr int kBaseDelayMs = 2000;
constexpr int kMaxDelayMs = 30000;
}

struct DaemonHealthMonitor::Peer
{
    QString service;
    QString path;
    QString iface;
    int failures = 0;
    QTimer *timer = nullptr;
    qint64 lastCheckMs = 0;
    qint64 lastOkMs = 0;
    QString lastError;
    bool registered = false;
};

DaemonHealthMonitor::DaemonHealthMonitor(QObject *parent)
    : QObject(parent)
{
    auto *busObj = new QDBusConnection(QDBusConnection::sessionBus());
    m_bus = busObj;

    m_fileOps = new Peer{
        QStringLiteral("org.slm.Desktop.FileOperations"),
        QStringLiteral("/org/slm/Desktop/FileOperations"),
        QStringLiteral("org.slm.Desktop.FileOperations"),
    };
    m_devices = new Peer{
        QStringLiteral("org.slm.Desktop.Devices"),
        QStringLiteral("/org/slm/Desktop/Devices"),
        QStringLiteral("org.slm.Desktop.Devices"),
    };

    setupPeer(*m_fileOps);
    setupPeer(*m_devices);

    if (m_bus->isConnected()) {
        m_bus->connect(QStringLiteral("org.freedesktop.DBus"),
                       QStringLiteral("/org/freedesktop/DBus"),
                       QStringLiteral("org.freedesktop.DBus"),
                       QStringLiteral("NameOwnerChanged"),
                       this,
                       SLOT(onNameOwnerChanged(QString,QString,QString)));
    }
}

DaemonHealthMonitor::~DaemonHealthMonitor()
{
    delete m_fileOps;
    delete m_devices;
    delete m_bus;
}

void DaemonHealthMonitor::setupPeer(Peer &peer)
{
    peer.timer = new QTimer(this);
    peer.timer->setSingleShot(true);
    connect(peer.timer, &QTimer::timeout, this, [this, &peer]() {
        checkPeer(peer);
    });
    schedule(peer, 0);
}

void DaemonHealthMonitor::schedule(Peer &peer, int delayMs)
{
    if (!peer.timer) {
        return;
    }
    peer.timer->start(qMax(0, delayMs));
}

void DaemonHealthMonitor::checkPeer(Peer &peer)
{
    peer.lastCheckMs = QDateTime::currentMSecsSinceEpoch();
    if (!m_bus || !m_bus->isConnected() || !m_bus->interface()) {
        peer.failures += 1;
        peer.registered = false;
        peer.lastError = QStringLiteral("session-bus-unavailable");
        schedule(peer, nextDelayMs(peer.failures));
        return;
    }

    QDBusReply<bool> isReg = m_bus->interface()->isServiceRegistered(peer.service);
    if (!isReg.isValid() || !isReg.value()) {
        peer.failures += 1;
        peer.registered = false;
        peer.lastError = QStringLiteral("service-not-registered");
        qWarning().noquote() << "[desktopd][watchdog] missing service=" << peer.service
                             << "failures=" << peer.failures;
        schedule(peer, nextDelayMs(peer.failures));
        return;
    }
    peer.registered = true;

    QDBusInterface iface(peer.service, peer.path, peer.iface, *m_bus);
    QDBusReply<QVariantMap> pingReply = iface.call(QStringLiteral("Ping"));
    const bool ok = pingReply.isValid() && pingReply.value().value(QStringLiteral("ok")).toBool();
    if (!ok) {
        peer.failures += 1;
        peer.lastError = QStringLiteral("ping-failed");
        qWarning().noquote() << "[desktopd][watchdog] ping failed service=" << peer.service
                             << "failures=" << peer.failures;
        schedule(peer, nextDelayMs(peer.failures));
        return;
    }

    if (peer.failures > 0) {
        qInfo().noquote() << "[desktopd][watchdog] recovered service=" << peer.service;
    }
    peer.failures = 0;
    peer.lastOkMs = QDateTime::currentMSecsSinceEpoch();
    peer.lastError.clear();
    schedule(peer, kBaseDelayMs);
}

int DaemonHealthMonitor::nextDelayMs(int failures)
{
    const int shift = qBound(0, failures, 6);
    const int delay = kBaseDelayMs * (1 << shift);
    return qMin(delay, kMaxDelayMs);
}

void DaemonHealthMonitor::onNameOwnerChanged(const QString &name,
                                             const QString &oldOwner,
                                             const QString &newOwner)
{
    Q_UNUSED(oldOwner)
    Q_UNUSED(newOwner)
    if (m_fileOps && name == m_fileOps->service) {
        m_fileOps->registered = !newOwner.isEmpty();
        schedule(*m_fileOps, 0);
    } else if (m_devices && name == m_devices->service) {
        m_devices->registered = !newOwner.isEmpty();
        schedule(*m_devices, 0);
    }
}

QVariantMap DaemonHealthMonitor::snapshot() const
{
    auto peerToMap = [](const Peer *peer) -> QVariantMap {
        if (!peer) {
            return {};
        }
        QVariantMap out;
        out.insert(QStringLiteral("service"), peer->service);
        out.insert(QStringLiteral("path"), peer->path);
        out.insert(QStringLiteral("iface"), peer->iface);
        out.insert(QStringLiteral("registered"), peer->registered);
        out.insert(QStringLiteral("failures"), peer->failures);
        out.insert(QStringLiteral("lastCheckMs"), peer->lastCheckMs);
        out.insert(QStringLiteral("lastOkMs"), peer->lastOkMs);
        out.insert(QStringLiteral("lastError"), peer->lastError);
        out.insert(QStringLiteral("nextCheckInMs"),
                   peer->timer ? peer->timer->remainingTime() : -1);
        return out;
    };

    QVariantMap out;
    out.insert(QStringLiteral("fileOperations"), peerToMap(m_fileOps));
    out.insert(QStringLiteral("devices"), peerToMap(m_devices));
    out.insert(QStringLiteral("baseDelayMs"), kBaseDelayMs);
    out.insert(QStringLiteral("maxDelayMs"), kMaxDelayMs);
    return out;
}
