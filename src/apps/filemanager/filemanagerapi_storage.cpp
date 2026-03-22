#include "filemanagerapi.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDateTime>
#include <QFileInfo>
#include <QMetaObject>
#include <QMutexLocker>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStorageInfo>
#include <QVariantMap>

#pragma push_macro("signals")
#undef signals
#include <gio/gio.h>
#pragma pop_macro("signals")

#include <thread>

namespace {

constexpr const char kDevicesService[] = "org.slm.Desktop.Devices";
constexpr const char kDevicesPath[] = "/org/slm/Desktop/Devices";
constexpr const char kDevicesIface[] = "org.slm.Desktop.Devices";
constexpr const char kLegacyDevicesService[] = "org.desktop_shell.Desktop.Devices";
constexpr const char kLegacyDevicesPath[] = "/org/desktop_shell/Desktop/Devices";
constexpr const char kLegacyDevicesIface[] = "org.desktop_shell.Desktop.Devices";
constexpr const char kErrDaemonUnavailable[] = "daemon-unavailable";
constexpr const char kErrDaemonTimeout[] = "daemon-timeout";
constexpr const char kErrDaemonDbusError[] = "daemon-dbus-error";
constexpr int kDbusTimeoutMs = 5000;

enum class DbusFailure {
    None,
    Unavailable,
    Timeout,
    Error,
};

template<typename ReplyT>
static DbusFailure classifyDbusReplyErrorStorage(const ReplyT &reply)
{
    if (reply.isValid()) {
        return DbusFailure::None;
    }
    const QDBusError err = reply.error();
    const QString name = err.name();
    if (err.type() == QDBusError::NoReply || name.endsWith(QStringLiteral(".Timeout"))) {
        return DbusFailure::Timeout;
    }
    if (err.type() == QDBusError::ServiceUnknown
            || err.type() == QDBusError::UnknownObject
            || err.type() == QDBusError::UnknownInterface
            || name.endsWith(QStringLiteral(".ServiceUnknown"))
            || name.endsWith(QStringLiteral(".UnknownObject"))
            || name.endsWith(QStringLiteral(".UnknownInterface"))) {
        return DbusFailure::Unavailable;
    }
    return DbusFailure::Error;
}

static QString dbusFailureCodeStorage(DbusFailure failure)
{
    switch (failure) {
    case DbusFailure::None:
        return QString();
    case DbusFailure::Unavailable:
        return QString::fromLatin1(kErrDaemonUnavailable);
    case DbusFailure::Timeout:
        return QString::fromLatin1(kErrDaemonTimeout);
    case DbusFailure::Error:
    default:
        return QString::fromLatin1(kErrDaemonDbusError);
    }
}

static bool callDevicesServiceStorage(const QString &method,
                                      const QVariantList &args,
                                      QVariantMap *resultOut,
                                      QString *failureCodeOut = nullptr)
{
    const auto tryCall = [&](const QString &service,
                             const QString &path,
                             const QString &ifaceName,
                             bool *callableOut) -> bool {
        QDBusInterface iface(service, path, ifaceName, QDBusConnection::sessionBus());
        if (!iface.isValid()) {
            if (callableOut) {
                *callableOut = false;
            }
            return false;
        }
        if (callableOut) {
            *callableOut = true;
        }
        iface.setTimeout(kDbusTimeoutMs);
        QDBusReply<QVariantMap> reply = iface.callWithArgumentList(QDBus::Block, method, args);
        if (!reply.isValid()) {
            if (failureCodeOut) {
                *failureCodeOut = dbusFailureCodeStorage(classifyDbusReplyErrorStorage(reply));
            }
            return false;
        }
        if (resultOut) {
            *resultOut = reply.value();
        }
        return true;
    };

    bool callable = false;
    if (tryCall(QString::fromLatin1(kDevicesService),
                QString::fromLatin1(kDevicesPath),
                QString::fromLatin1(kDevicesIface),
                &callable)) {
        return true;
    }
    if (callable) {
        return false;
    }
    const bool legacyOk = tryCall(QString::fromLatin1(kLegacyDevicesService),
                                  QString::fromLatin1(kLegacyDevicesPath),
                                  QString::fromLatin1(kLegacyDevicesIface),
                                  nullptr);
    if (!legacyOk && failureCodeOut && failureCodeOut->isEmpty()) {
        *failureCodeOut = QString::fromLatin1(kErrDaemonUnavailable);
    }
    return legacyOk;
}

static QString iconNameFromGIconStorageLocal(GIcon *icon)
{
    if (icon == nullptr) {
        return QString();
    }
    if (G_IS_THEMED_ICON(icon)) {
        const gchar * const *names = g_themed_icon_get_names(G_THEMED_ICON(icon));
        if (names != nullptr && names[0] != nullptr) {
            return QString::fromUtf8(names[0]);
        }
    }
    return QString();
}

static QString gfileToPathOrUriStorageLocal(GFile *file)
{
    if (file == nullptr) {
        return QString();
    }
    gchar *path = g_file_get_path(file);
    if (path != nullptr) {
        const QString out = QString::fromUtf8(path);
        g_free(path);
        return out;
    }
    gchar *uri = g_file_get_uri(file);
    const QString out = uri ? QString::fromUtf8(uri) : QString();
    if (uri != nullptr) {
        g_free(uri);
    }
    return out;
}

} // namespace

QVariantList FileManagerApi::queryStorageLocationsSync(int lsblkTimeoutMs) const
{
    Q_UNUSED(lsblkTimeoutMs)
    QVariantList out;
    QSet<QString> seenKeys;
    QSet<QString> seenMountPaths;
    GVolumeMonitor *monitor = g_volume_monitor_get();

    GList *mounts = g_volume_monitor_get_mounts(monitor);
    for (GList *it = mounts; it != nullptr; it = it->next) {
        GMount *mount = G_MOUNT(it->data);
        if (!mount) {
            continue;
        }
        GFile *root = g_mount_get_root(mount);
        const QString pathOrUri = gfileToPathOrUriStorageLocal(root);
        if (root) {
            g_object_unref(root);
        }
        if (pathOrUri.isEmpty()) {
            continue;
        }

        gchar *nameRaw = g_mount_get_name(mount);
        gchar *uuidRaw = g_mount_get_uuid(mount);
        QString name = QString::fromUtf8(nameRaw ? nameRaw : "");
        QString device = QString::fromUtf8(uuidRaw ? uuidRaw : "");
        if (nameRaw) {
            g_free(nameRaw);
        }
        if (uuidRaw) {
            g_free(uuidRaw);
        }
        if (name.trimmed().isEmpty()) {
            name = pathOrUri;
        }
        if (device.trimmed().isEmpty()) {
            device = pathOrUri;
        }

        if (pathOrUri.startsWith(QStringLiteral("/"))) {
            const QStorageInfo localStorage(pathOrUri);
            const QString unixDevice = QString::fromUtf8(localStorage.device()).trimmed();
            if (!unixDevice.isEmpty()) {
                device = unixDevice;
            }
        }
        const QString dedupeKey = pathOrUri + QStringLiteral("|") + device;
        if (seenKeys.contains(dedupeKey)) {
            continue;
        }
        seenKeys.insert(dedupeKey);
        if (pathOrUri.startsWith(QStringLiteral("/"))) {
            seenMountPaths.insert(pathOrUri);
        }

        GIcon *icon = g_mount_get_icon(mount);
        const QString iconName = iconNameFromGIconStorageLocal(icon);
        if (icon) {
            g_object_unref(icon);
        }

        QVariantMap row;
        row.insert(QStringLiteral("name"), name);
        row.insert(QStringLiteral("label"), name);
        row.insert(QStringLiteral("path"), pathOrUri);
        row.insert(QStringLiteral("rootPath"), pathOrUri);
        row.insert(QStringLiteral("device"), device);
        row.insert(QStringLiteral("mounted"), true);
        row.insert(QStringLiteral("browsable"), true);
        row.insert(QStringLiteral("isRoot"), pathOrUri == QStringLiteral("/"));
        row.insert(QStringLiteral("icon"), iconName.isEmpty() ? QStringLiteral("drive-harddisk-symbolic")
                                                               : iconName);
        if (pathOrUri.startsWith(QStringLiteral("/"))) {
            const QStorageInfo v(pathOrUri);
            if (v.isValid() && v.isReady()) {
                row.insert(QStringLiteral("bytesTotal"), static_cast<qlonglong>(v.bytesTotal()));
                row.insert(QStringLiteral("bytesAvailable"), static_cast<qlonglong>(v.bytesAvailable()));
                row.insert(QStringLiteral("bytesFree"), static_cast<qlonglong>(v.bytesFree()));
                row.insert(QStringLiteral("filesystemType"), QString::fromUtf8(v.fileSystemType()));
            } else {
                row.insert(QStringLiteral("bytesTotal"), static_cast<qlonglong>(-1));
                row.insert(QStringLiteral("bytesAvailable"), static_cast<qlonglong>(-1));
                row.insert(QStringLiteral("bytesFree"), static_cast<qlonglong>(-1));
                row.insert(QStringLiteral("filesystemType"), QString());
            }
        } else {
            row.insert(QStringLiteral("bytesTotal"), static_cast<qlonglong>(-1));
            row.insert(QStringLiteral("bytesAvailable"), static_cast<qlonglong>(-1));
            row.insert(QStringLiteral("bytesFree"), static_cast<qlonglong>(-1));
            row.insert(QStringLiteral("filesystemType"), QString());
        }
        out.push_back(row);
    }
    g_list_free_full(mounts, g_object_unref);

    GList *volumes = g_volume_monitor_get_volumes(monitor);
    for (GList *it = volumes; it != nullptr; it = it->next) {
        GVolume *volume = G_VOLUME(it->data);
        if (!volume) {
            continue;
        }
        GMount *mounted = g_volume_get_mount(volume);
        if (mounted) {
            g_object_unref(mounted);
            continue;
        }

        gchar *nameRaw = g_volume_get_name(volume);
        gchar *devRaw = g_volume_get_identifier(volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
        if (devRaw == nullptr) {
            devRaw = g_volume_get_uuid(volume);
        }
        QString name = QString::fromUtf8(nameRaw ? nameRaw : "");
        QString device = QString::fromUtf8(devRaw ? devRaw : "");
        if (nameRaw) {
            g_free(nameRaw);
        }
        if (devRaw) {
            g_free(devRaw);
        }
        if (name.trimmed().isEmpty()) {
            name = QStringLiteral("Volume");
        }
        if (device.trimmed().isEmpty()) {
            device = name;
        }

        QString pathOrUri;
        GFile *activationRoot = g_volume_get_activation_root(volume);
        if (activationRoot) {
            pathOrUri = gfileToPathOrUriStorageLocal(activationRoot);
            g_object_unref(activationRoot);
        }

        const QString dedupeKey = (pathOrUri.isEmpty() ? name : pathOrUri) + QStringLiteral("|") + device;
        if (seenKeys.contains(dedupeKey)) {
            continue;
        }
        seenKeys.insert(dedupeKey);

        GIcon *icon = g_volume_get_icon(volume);
        const QString iconName = iconNameFromGIconStorageLocal(icon);
        if (icon) {
            g_object_unref(icon);
        }

        QVariantMap row;
        row.insert(QStringLiteral("name"), name);
        row.insert(QStringLiteral("label"), name);
        row.insert(QStringLiteral("path"), pathOrUri);
        row.insert(QStringLiteral("rootPath"), pathOrUri);
        row.insert(QStringLiteral("device"), device);
        row.insert(QStringLiteral("mounted"), false);
        row.insert(QStringLiteral("browsable"), !pathOrUri.isEmpty());
        row.insert(QStringLiteral("isRoot"), false);
        row.insert(QStringLiteral("icon"), iconName.isEmpty() ? QStringLiteral("drive-harddisk-symbolic")
                                                               : iconName);
        row.insert(QStringLiteral("bytesTotal"), static_cast<qlonglong>(-1));
        row.insert(QStringLiteral("bytesAvailable"), static_cast<qlonglong>(-1));
        row.insert(QStringLiteral("bytesFree"), static_cast<qlonglong>(-1));
        row.insert(QStringLiteral("filesystemType"), QString());
        out.push_back(row);
    }
    g_list_free_full(volumes, g_object_unref);

    const QList<QStorageInfo> mounted = QStorageInfo::mountedVolumes();
    for (const QStorageInfo &v : mounted) {
        if (!v.isValid() || !v.isReady()) {
            continue;
        }
        const QString rootPath = v.rootPath();
        if (rootPath.isEmpty()) {
            continue;
        }
        if (seenMountPaths.contains(rootPath)) {
            continue;
        }
        seenMountPaths.insert(rootPath);
        QString device = QString::fromUtf8(v.device());
        if (device.trimmed().isEmpty()) {
            device = rootPath;
        }
        const QString dedupeKey = rootPath + QStringLiteral("|") + device;
        if (seenKeys.contains(dedupeKey)) {
            continue;
        }
        seenKeys.insert(dedupeKey);

        QVariantMap row;
        const QString name = v.displayName().isEmpty() ? rootPath : v.displayName();
        row.insert(QStringLiteral("name"), name);
        row.insert(QStringLiteral("label"), name);
        row.insert(QStringLiteral("path"), rootPath);
        row.insert(QStringLiteral("rootPath"), rootPath);
        row.insert(QStringLiteral("device"), device);
        row.insert(QStringLiteral("mounted"), true);
        row.insert(QStringLiteral("browsable"), true);
        row.insert(QStringLiteral("isRoot"), rootPath == QStringLiteral("/"));
        row.insert(QStringLiteral("icon"), QStringLiteral("drive-harddisk-symbolic"));
        row.insert(QStringLiteral("bytesTotal"), static_cast<qlonglong>(v.bytesTotal()));
        row.insert(QStringLiteral("bytesAvailable"), static_cast<qlonglong>(v.bytesAvailable()));
        row.insert(QStringLiteral("bytesFree"), static_cast<qlonglong>(v.bytesFree()));
        row.insert(QStringLiteral("filesystemType"), QString::fromUtf8(v.fileSystemType()));
        out.push_back(row);
    }

    return out;
}

QVariantList FileManagerApi::storageLocations() const
{
    {
        QMutexLocker locker(&m_storageCacheMutex);
        if (!m_storageLocationsCache.isEmpty()) {
            return m_storageLocationsCache;
        }
    }
    const QVariantList rows = queryStorageLocationsSync(400);
    {
        QMutexLocker locker(&m_storageCacheMutex);
        m_storageLocationsCache = rows;
        m_storageLocationsCacheMs = QDateTime::currentMSecsSinceEpoch();
    }
    return rows;
}

void FileManagerApi::refreshStorageLocationsAsync()
{
    QVariantList rows = queryStorageLocationsSync(800);
    {
        QMutexLocker locker(&m_storageCacheMutex);
        m_storageLocationsCache = rows;
        m_storageLocationsCacheMs = QDateTime::currentMSecsSinceEpoch();
    }
    emit storageLocationsUpdated(rows);
}

QVariantMap FileManagerApi::mountStorageDevice(const QString &devicePath) const
{
    const QString dev = devicePath.trimmed();
    if (dev.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-device"));
    }

    QVariantMap devicesReply;
    QString daemonError;
    if (callDevicesServiceStorage(QStringLiteral("Mount"), {dev}, &devicesReply, &daemonError)) {
        const bool ok = devicesReply.value(QStringLiteral("ok")).toBool();
        const QString error = devicesReply.value(QStringLiteral("error")).toString().trimmed();
        const QString mountedPath = devicesReply.value(QStringLiteral("mountPath")).toString().trimmed();
        if (!ok) {
            return makeResult(false, error.isEmpty() ? QStringLiteral("mount-failed") : error);
        }
        return makeResult(true, QString(),
                          {{QStringLiteral("device"), dev},
                           {QStringLiteral("path"), mountedPath.isEmpty() ? dev : mountedPath}});
    }

    QProcess p;
    if (dev.contains(QStringLiteral("://"))) {
        p.start(QStringLiteral("gio"), QStringList{QStringLiteral("mount"), dev});
    } else {
        p.start(QStringLiteral("udisksctl"), QStringList{QStringLiteral("mount"), QStringLiteral("-b"), dev});
    }
    p.waitForFinished(15000);
    const QString stdoutText = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    const QString stderrText = QString::fromUtf8(p.readAllStandardError()).trimmed();
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        return makeResult(false,
                          stderrText.isEmpty()
                              ? (daemonError.isEmpty() ? QStringLiteral("mount-failed") : daemonError)
                              : stderrText);
    }
    QString mountedPath;
    QRegularExpression re(QStringLiteral("\\bat\\s+(.+?)(?:\\.|$)"));
    QRegularExpressionMatch m = re.match(stdoutText);
    if (m.hasMatch()) {
        mountedPath = m.captured(1).trimmed();
    }
    if (mountedPath.isEmpty() && dev.contains(QStringLiteral("://"))) {
        mountedPath = dev;
    }
    return makeResult(true, QString(),
                      {{QStringLiteral("device"), dev},
                       {QStringLiteral("path"), mountedPath}});
}

QVariantMap FileManagerApi::unmountStorageDevice(const QString &devicePath) const
{
    const QString dev = devicePath.trimmed();
    if (dev.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-device"));
    }

    QVariantMap devicesReply;
    QString daemonError;
    if (callDevicesServiceStorage(QStringLiteral("Eject"), {dev}, &devicesReply, &daemonError)) {
        const bool ok = devicesReply.value(QStringLiteral("ok")).toBool();
        const QString error = devicesReply.value(QStringLiteral("error")).toString().trimmed();
        if (!ok) {
            return makeResult(false, error.isEmpty() ? QStringLiteral("eject-failed") : error);
        }
        return makeResult(true, QString(), {{QStringLiteral("device"), dev}});
    }

    QProcess p;
    if (dev.contains(QStringLiteral("://"))) {
        p.start(QStringLiteral("gio"), QStringList{QStringLiteral("mount"), QStringLiteral("-u"), dev});
    } else {
        p.start(QStringLiteral("udisksctl"), QStringList{QStringLiteral("unmount"), QStringLiteral("-b"), dev});
    }
    p.waitForFinished(15000);
    const QString stderrText = QString::fromUtf8(p.readAllStandardError()).trimmed();
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        return makeResult(false,
                          stderrText.isEmpty()
                              ? (daemonError.isEmpty() ? QStringLiteral("eject-failed") : daemonError)
                              : stderrText);
    }
    return makeResult(true, QString(), {{QStringLiteral("device"), dev}});
}

QVariantMap FileManagerApi::startMountStorageDevice(const QString &devicePath)
{
    const QString dev = devicePath.trimmed();
    if (dev.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-device"));
    }
    std::thread([this, dev]() {
        const QVariantMap result = mountStorageDevice(dev);
        const bool ok = result.value(QStringLiteral("ok")).toBool();
        const QString mountedPath = result.value(QStringLiteral("path")).toString();
        const QString error = result.value(QStringLiteral("error")).toString();
        QMetaObject::invokeMethod(this, [this, dev, ok, mountedPath, error]() {
            refreshStorageLocationsAsync();
            emit storageMountFinished(dev, ok, mountedPath, error);
        }, Qt::QueuedConnection);
    }).detach();
    return makeResult(true, QString(), {{QStringLiteral("device"), dev}, {QStringLiteral("async"), true}});
}

QVariantMap FileManagerApi::startUnmountStorageDevice(const QString &devicePath)
{
    const QString dev = devicePath.trimmed();
    if (dev.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-device"));
    }
    std::thread([this, dev]() {
        const QVariantMap result = unmountStorageDevice(dev);
        const bool ok = result.value(QStringLiteral("ok")).toBool();
        const QString error = result.value(QStringLiteral("error")).toString();
        QMetaObject::invokeMethod(this, [this, dev, ok, error]() {
            refreshStorageLocationsAsync();
            emit storageUnmountFinished(dev, ok, error);
        }, Qt::QueuedConnection);
    }).detach();
    return makeResult(true, QString(), {{QStringLiteral("device"), dev}, {QStringLiteral("async"), true}});
}

QVariantMap FileManagerApi::startConnectServer(const QString &serverUri)
{
    QString uri = serverUri.trimmed();
    if (uri.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-uri"));
    }
    if (!uri.contains(QStringLiteral("://"))) {
        uri.prepend(QStringLiteral("smb://"));
    }
    std::thread([this, uri]() {
        const QVariantMap result = mountStorageDevice(uri);
        const bool ok = result.value(QStringLiteral("ok")).toBool();
        const QString mountedPath = result.value(QStringLiteral("path")).toString();
        const QString error = result.value(QStringLiteral("error")).toString();
        QMetaObject::invokeMethod(this, [this, uri, ok, mountedPath, error]() {
            refreshStorageLocationsAsync();
            emit connectServerFinished(uri, ok, mountedPath, error);
            emit storageMountFinished(uri, ok, mountedPath, error);
        }, Qt::QueuedConnection);
    }).detach();
    return makeResult(true, QString(),
                      {{QStringLiteral("uri"), uri}, {QStringLiteral("async"), true}});
}
