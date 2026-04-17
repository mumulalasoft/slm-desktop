#include "storageattachnotifier.h"

#include "../notifications/notificationmanager.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDateTime>
#include <QMetaObject>
#include <QProcess>
#include <QSet>
#include <QUrl>

namespace {

constexpr const char kStorageService[] = "org.slm.Desktop.Storage";
constexpr const char kStoragePath[] = "/org/slm/Desktop/Storage";
constexpr const char kStorageIface[] = "org.slm.Desktop.Storage";

bool openUrlWithDesktopHandler(const QUrl &url)
{
    if (!url.isValid()) {
        return false;
    }
    const QString urlText = url.toString(QUrl::FullyEncoded);
    if (urlText.trimmed().isEmpty()) {
        return false;
    }
    return QProcess::startDetached(QStringLiteral("xdg-open"), QStringList{urlText});
}

} // namespace

StorageAttachNotifier::StorageAttachNotifier(NotificationManager *notificationManager, QObject *parent)
    : QObject(parent)
    , m_notificationManager(notificationManager)
{
    m_refreshDebounceTimer.setSingleShot(true);
    m_refreshDebounceTimer.setInterval(1200);
    connect(&m_refreshDebounceTimer, &QTimer::timeout,
            this, &StorageAttachNotifier::onRefreshTimerFired);

    if (QDBusConnection::sessionBus().isConnected()) {
        QDBusConnection::sessionBus().connect(QString::fromLatin1(kStorageService),
                                              QString::fromLatin1(kStoragePath),
                                              QString::fromLatin1(kStorageIface),
                                              QStringLiteral("StorageLocationsChanged"),
                                              this,
                                              SLOT(onStorageLocationsChanged()));
    }

    if (m_notificationManager) {
        connect(m_notificationManager,
                SIGNAL(ActionInvoked(uint,QString)),
                this,
                SLOT(onNotificationActionInvoked(uint,QString)));
        connect(m_notificationManager,
                SIGNAL(NotificationClosed(uint,uint)),
                this,
                SLOT(onNotificationClosed(uint,uint)));
    }

    refreshSnapshot(false);
}

void StorageAttachNotifier::onStorageLocationsChanged()
{
    m_refreshDebounceTimer.start();
}

void StorageAttachNotifier::onRefreshTimerFired()
{
    refreshSnapshot(true);
}

void StorageAttachNotifier::onNotificationActionInvoked(uint id, const QString &actionKey)
{
    const auto it = m_payloadByNotificationId.constFind(id);
    if (it == m_payloadByNotificationId.cend()) {
        return;
    }
    const DeviceGroupPayload payload = it.value();
    const QString action = actionKey.trimmed().toLower();
    if (action == QStringLiteral("eject")) {
        handleEjectAction(payload);
    } else {
        handleOpenAction(payload);
    }
}

void StorageAttachNotifier::onNotificationClosed(uint id, uint reason)
{
    Q_UNUSED(reason)
    m_payloadByNotificationId.remove(id);
}

QVariantList StorageAttachNotifier::fetchStorageRows() const
{
    QDBusInterface iface(QString::fromLatin1(kStorageService),
                         QString::fromLatin1(kStoragePath),
                         QString::fromLatin1(kStorageIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return {};
    }
    iface.setTimeout(5000);
    QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("GetStorageLocations"));
    if (!reply.isValid()) {
        return {};
    }
    const QVariantMap out = reply.value();
    if (!out.value(QStringLiteral("ok")).toBool()) {
        return {};
    }
    return out.value(QStringLiteral("rows")).toList();
}

QString StorageAttachNotifier::deviceGroupKeyFromRow(const QVariantMap &row) const
{
    const QString key = row.value(QStringLiteral("deviceGroupKey")).toString().trimmed();
    if (!key.isEmpty()) {
        return key;
    }
    const QString device = row.value(QStringLiteral("device")).toString().trimmed();
    if (!device.isEmpty()) {
        return device;
    }
    return row.value(QStringLiteral("path")).toString().trimmed();
}

QString StorageAttachNotifier::deviceGroupLabelFromRow(const QVariantMap &row) const
{
    const QString label = row.value(QStringLiteral("deviceGroupLabel")).toString().trimmed();
    if (!label.isEmpty()) {
        return label;
    }
    const QString fallback = row.value(QStringLiteral("label")).toString().trimmed();
    if (!fallback.isEmpty()) {
        return fallback;
    }
    return QStringLiteral("External Drive");
}

StorageAttachNotifier::VolumeRow StorageAttachNotifier::volumeRowFromVariant(const QVariantMap &row) const
{
    VolumeRow volume;
    volume.device = row.value(QStringLiteral("device")).toString().trimmed();
    volume.path = row.value(QStringLiteral("path")).toString().trimmed();
    volume.mounted = row.value(QStringLiteral("mounted")).toBool();
    return volume;
}

QString StorageAttachNotifier::actionTargetForVolume(const VolumeRow &volume) const
{
    if (!volume.device.isEmpty()) {
        return volume.device;
    }
    if (volume.path.startsWith(QStringLiteral("__mount__:"))) {
        return QUrl::fromPercentEncoding(volume.path.mid(10).toUtf8());
    }
    return volume.path;
}

bool StorageAttachNotifier::mountTarget(const QString &target, QString *mountedPathOut) const
{
    const QString t = target.trimmed();
    if (t.isEmpty()) {
        return false;
    }
    QDBusInterface iface(QString::fromLatin1(kStorageService),
                         QString::fromLatin1(kStoragePath),
                         QString::fromLatin1(kStorageIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return false;
    }
    iface.setTimeout(15000);
    QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("Mount"), t);
    if (!reply.isValid()) {
        return false;
    }
    const QVariantMap out = reply.value();
    if (!out.value(QStringLiteral("ok")).toBool()) {
        return false;
    }
    if (mountedPathOut) {
        QString p = out.value(QStringLiteral("path")).toString().trimmed();
        if (p.isEmpty()) {
            p = out.value(QStringLiteral("mountPath")).toString().trimmed();
        }
        *mountedPathOut = p;
    }
    return true;
}

bool StorageAttachNotifier::ejectTarget(const QString &target) const
{
    const QString t = target.trimmed();
    if (t.isEmpty()) {
        return false;
    }
    QDBusInterface iface(QString::fromLatin1(kStorageService),
                         QString::fromLatin1(kStoragePath),
                         QString::fromLatin1(kStorageIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return false;
    }
    iface.setTimeout(15000);
    QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("Eject"), t);
    return reply.isValid() && reply.value().value(QStringLiteral("ok")).toBool();
}

bool StorageAttachNotifier::openVolume(const VolumeRow &volume) const
{
    QString path = volume.path;
    if (volume.mounted && !path.isEmpty() && !path.startsWith(QStringLiteral("__mount__:"))) {
        const QUrl url = path.startsWith(QStringLiteral("/"))
                ? QUrl::fromLocalFile(path)
                : QUrl(path);
        return openUrlWithDesktopHandler(url);
    }

    QString mountedPath;
    if (!mountTarget(actionTargetForVolume(volume), &mountedPath)) {
        return false;
    }
    const QString candidate = mountedPath.trimmed().isEmpty() ? volume.path : mountedPath;
    if (candidate.trimmed().isEmpty()) {
        return true;
    }
    const QUrl url = candidate.startsWith(QStringLiteral("/"))
            ? QUrl::fromLocalFile(candidate)
            : QUrl(candidate);
    return openUrlWithDesktopHandler(url);
}

void StorageAttachNotifier::handleOpenAction(const DeviceGroupPayload &payload) const
{
    if (payload.volumes.isEmpty()) {
        return;
    }
    // Single-open behavior for multi-volume device:
    // pick a mounted volume first, otherwise mount+open the first one.
    for (const VolumeRow &volume : payload.volumes) {
        if (volume.mounted) {
            if (openVolume(volume)) {
                return;
            }
        }
    }
    openVolume(payload.volumes.first());
}

void StorageAttachNotifier::handleEjectAction(const DeviceGroupPayload &payload) const
{
    QSet<QString> seenTargets;
    for (const VolumeRow &volume : payload.volumes) {
        if (!volume.mounted) {
            continue;
        }
        const QString target = actionTargetForVolume(volume).trimmed();
        if (target.isEmpty() || seenTargets.contains(target)) {
            continue;
        }
        seenTargets.insert(target);
        ejectTarget(target);
    }
}

void StorageAttachNotifier::refreshSnapshot(bool notifyOnAttach)
{
    const QVariantList rows = fetchStorageRows();
    QHash<QString, DeviceGroupSnapshot> nextGroups;

    for (const QVariant &v : rows) {
        const QVariantMap row = v.toMap();
        if (row.value(QStringLiteral("isSystem")).toBool()) {
            continue;
        }
        const QString key = deviceGroupKeyFromRow(row);
        if (key.isEmpty()) {
            continue;
        }
        DeviceGroupSnapshot group = nextGroups.value(key);
        group.visibleCount += 1;
        if (group.label.trimmed().isEmpty()) {
            group.label = deviceGroupLabelFromRow(row);
        }
        group.volumes.push_back(volumeRowFromVariant(row));
        nextGroups.insert(key, group);
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (notifyOnAttach && m_snapshotReady && m_notificationManager) {
        for (auto it = nextGroups.constBegin(); it != nextGroups.constEnd(); ++it) {
            const QString &key = it.key();
            const DeviceGroupSnapshot &group = it.value();
            const int prevCount = m_prevGroups.value(key).visibleCount;
            if (prevCount > 0 || group.visibleCount <= 0) {
                continue;
            }
            const qint64 lastMs = m_lastNotifiedMsByGroup.value(key, 0);
            if (lastMs > 0 && (nowMs - lastMs) < m_attachCooldownMs) {
                continue;
            }
            const QStringList actions{QStringLiteral("open"), QStringLiteral("Open"), QStringLiteral("eject"), QStringLiteral("Eject")};
            uint id = 0;
            const bool invoked = QMetaObject::invokeMethod(
                m_notificationManager,
                "Notify",
                Qt::DirectConnection,
                Q_RETURN_ARG(uint, id),
                Q_ARG(QString, QStringLiteral("Storage")),
                Q_ARG(uint, 0u),
                Q_ARG(QString, QStringLiteral("drive-removable-media-symbolic")),
                Q_ARG(QString, tr("External Drive connected")),
                Q_ARG(QString, tr("%1 volume tersedia").arg(group.visibleCount)),
                Q_ARG(QStringList, actions),
                Q_ARG(QVariantMap, QVariantMap{}),
                Q_ARG(int, 7000));
            if (!invoked) {
                id = 0;
            }
            if (id > 0) {
                DeviceGroupPayload payload;
                payload.label = group.label;
                payload.volumes = group.volumes;
                m_payloadByNotificationId.insert(id, payload);
                m_lastNotifiedMsByGroup.insert(key, nowMs);
            }
        }
    }

    m_prevGroups = nextGroups;
    m_snapshotReady = true;
}
