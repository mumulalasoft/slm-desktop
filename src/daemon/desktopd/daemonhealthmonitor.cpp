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
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace {
constexpr int kBaseDelayMs = 2000;
constexpr int kMaxDelayMs = 30000;
constexpr int kDefaultTimelineLimit = 200;
}

struct DaemonHealthMonitor::Peer
{
    QString key;
    QString service;
    QString path;
    QString iface;
    int failures = 0;
    QTimer *timer = nullptr;
    qint64 lastCheckMs = 0;
    qint64 lastOkMs = 0;
    QString lastError;
    bool registered = false;
    QString lastTimelineCode;
};

DaemonHealthMonitor::DaemonHealthMonitor(QObject *parent)
    : QObject(parent)
{
    auto *busObj = new QDBusConnection(QDBusConnection::sessionBus());
    m_bus = busObj;

    m_fileOps = new Peer{
        QStringLiteral("fileOperations"),
        QStringLiteral("org.slm.Desktop.FileOperations"),
        QStringLiteral("/org/slm/Desktop/FileOperations"),
        QStringLiteral("org.slm.Desktop.FileOperations"),
    };
    m_devices = new Peer{
        QStringLiteral("devices"),
        QStringLiteral("org.slm.Desktop.Devices"),
        QStringLiteral("/org/slm/Desktop/Devices"),
        QStringLiteral("org.slm.Desktop.Devices"),
    };

    m_timelineLimit = qMax(32, qEnvironmentVariableIntValue("SLM_DAEMON_HEALTH_TIMELINE_LIMIT"));
    if (!qEnvironmentVariableIsSet("SLM_DAEMON_HEALTH_TIMELINE_LIMIT")) {
        m_timelineLimit = kDefaultTimelineLimit;
    }
    m_timelineFilePath = resolveTimelineFilePath();
    loadTimeline();
    appendTimelineEvent(QStringLiteral("monitor"),
                        nullptr,
                        QStringLiteral("monitor-started"),
                        QStringLiteral("info"),
                        QStringLiteral("Desktop daemon health monitor started."));

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
        if (peer.lastTimelineCode != peer.lastError) {
            appendTimelineEvent(peer.key,
                                &peer,
                                peer.lastError,
                                QStringLiteral("critical"),
                                QStringLiteral("Session bus unavailable while checking peer."),
                                {{QStringLiteral("service"), peer.service}});
            peer.lastTimelineCode = peer.lastError;
        }
        schedule(peer, nextDelayMs(peer.failures));
        return;
    }

    QDBusReply<bool> isReg = m_bus->interface()->isServiceRegistered(peer.service);
    if (!isReg.isValid() || !isReg.value()) {
        peer.failures += 1;
        peer.registered = false;
        peer.lastError = QStringLiteral("service-not-registered");
        if (peer.lastTimelineCode != peer.lastError) {
            appendTimelineEvent(peer.key,
                                &peer,
                                peer.lastError,
                                QStringLiteral("error"),
                                QStringLiteral("Service is not registered on session bus."),
                                {{QStringLiteral("service"), peer.service}});
            peer.lastTimelineCode = peer.lastError;
        }
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
        if (peer.lastTimelineCode != peer.lastError) {
            appendTimelineEvent(peer.key,
                                &peer,
                                peer.lastError,
                                QStringLiteral("error"),
                                QStringLiteral("Health ping failed."),
                                {{QStringLiteral("service"), peer.service}});
            peer.lastTimelineCode = peer.lastError;
        }
        qWarning().noquote() << "[desktopd][watchdog] ping failed service=" << peer.service
                             << "failures=" << peer.failures;
        schedule(peer, nextDelayMs(peer.failures));
        return;
    }

    if (peer.failures > 0) {
        qInfo().noquote() << "[desktopd][watchdog] recovered service=" << peer.service;
        appendTimelineEvent(peer.key,
                            &peer,
                            QStringLiteral("recovered"),
                            QStringLiteral("info"),
                            QStringLiteral("Peer service recovered."),
                            {{QStringLiteral("service"), peer.service}});
    }
    peer.failures = 0;
    peer.lastOkMs = QDateTime::currentMSecsSinceEpoch();
    peer.lastError.clear();
    peer.lastTimelineCode.clear();
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
    const bool degraded = (m_fileOps && m_fileOps->failures > 0)
                       || (m_devices && m_devices->failures > 0);
    out.insert(QStringLiteral("degraded"), degraded);
    QVariantList reasonCodes;
    if (m_fileOps && !m_fileOps->lastError.isEmpty()) {
        reasonCodes.push_back(m_fileOps->lastError);
    }
    if (m_devices && !m_devices->lastError.isEmpty()) {
        const QString code = m_devices->lastError;
        if (!reasonCodes.contains(code)) {
            reasonCodes.push_back(code);
        }
    }
    out.insert(QStringLiteral("reasonCodes"), reasonCodes);
    out.insert(QStringLiteral("timeline"), m_timeline);
    out.insert(QStringLiteral("timelineSize"), m_timeline.size());
    out.insert(QStringLiteral("timelineFile"), m_timelineFilePath);
    return out;
}

void DaemonHealthMonitor::appendTimelineEvent(const QString &peerKey,
                                              const Peer *peer,
                                              const QString &code,
                                              const QString &severity,
                                              const QString &message,
                                              const QVariantMap &details)
{
    QVariantMap row;
    row.insert(QStringLiteral("tsMs"), QDateTime::currentMSecsSinceEpoch());
    row.insert(QStringLiteral("peer"), peerKey);
    row.insert(QStringLiteral("code"), code.trimmed().toLower());
    row.insert(QStringLiteral("severity"), severity.trimmed().isEmpty() ? QStringLiteral("info")
                                                                        : severity.trimmed().toLower());
    row.insert(QStringLiteral("message"), message);
    if (peer) {
        row.insert(QStringLiteral("service"), peer->service);
        row.insert(QStringLiteral("failures"), peer->failures);
        row.insert(QStringLiteral("registered"), peer->registered);
    }
    if (!details.isEmpty()) {
        row.insert(QStringLiteral("details"), details);
    }
    m_timeline.push_back(row);
    while (m_timeline.size() > m_timelineLimit) {
        m_timeline.removeFirst();
    }
    saveTimeline();
}

QString DaemonHealthMonitor::resolveTimelineFilePath() const
{
    const QString overridePath = qEnvironmentVariable("SLM_DAEMON_HEALTH_TIMELINE_FILE").trimmed();
    if (!overridePath.isEmpty()) {
        return overridePath;
    }

    const QString stateRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (stateRoot.isEmpty()) {
        return QDir::temp().filePath(QStringLiteral("slm-daemon-health-timeline.json"));
    }
    return QDir(stateRoot).filePath(QStringLiteral("daemon-health-timeline.json"));
}

void DaemonHealthMonitor::loadTimeline()
{
    m_timeline.clear();
    if (m_timelineFilePath.isEmpty()) {
        return;
    }
    QFile file(m_timelineFilePath);
    if (!file.exists()) {
        return;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) {
        return;
    }
    const QJsonArray arr = doc.array();
    for (const QJsonValue &item : arr) {
        if (!item.isObject()) {
            continue;
        }
        m_timeline.push_back(item.toObject().toVariantMap());
    }
    while (m_timeline.size() > m_timelineLimit) {
        m_timeline.removeFirst();
    }
}

void DaemonHealthMonitor::saveTimeline() const
{
    if (m_timelineFilePath.isEmpty()) {
        return;
    }
    const QFileInfo info(m_timelineFilePath);
    QDir dir(info.path());
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        return;
    }

    QJsonArray arr;
    for (const QVariant &item : m_timeline) {
        arr.push_back(QJsonObject::fromVariantMap(item.toMap()));
    }
    QSaveFile file(m_timelineFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return;
    }
    file.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    file.commit();
}
