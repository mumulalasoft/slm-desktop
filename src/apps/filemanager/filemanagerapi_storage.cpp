#include "filemanagerapi.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDateTime>
#include <QFileInfo>
#include <QMetaObject>
#include <QMutexLocker>
#include <QPointer>
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
constexpr const char kStorageService[] = "org.slm.Desktop.Storage";
constexpr const char kStoragePath[] = "/org/slm/Desktop/Storage";
constexpr const char kStorageIface[] = "org.slm.Desktop.Storage";
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

static bool callStorageServiceStorage(const QString &method,
                                      const QVariantList &args,
                                      QVariantMap *resultOut,
                                      QString *failureCodeOut = nullptr)
{
    QDBusInterface iface(QString::fromLatin1(kStorageService),
                         QString::fromLatin1(kStoragePath),
                         QString::fromLatin1(kStorageIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        if (failureCodeOut) {
            *failureCodeOut = QString::fromLatin1(kErrDaemonUnavailable);
        }
        return false;
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

struct GioOpState {
    GMainLoop *loop = nullptr;
    bool ok = false;
    bool timedOut = false;
    QString error;
    QString path;
    QString timeoutError;
    GCancellable *cancellable = nullptr;
    guint timeoutSourceId = 0;
};

constexpr int kGioMountTimeoutMs = 15000;
constexpr int kGioUnmountTimeoutMs = 15000;

static gboolean onGioOperationTimeoutStorage(gpointer userData)
{
    auto *state = static_cast<GioOpState *>(userData);
    if (!state) {
        return G_SOURCE_REMOVE;
    }
    state->timedOut = true;
    state->error = state->timeoutError.isEmpty() ? QStringLiteral("operation-timeout")
                                                  : state->timeoutError;
    state->timeoutSourceId = 0;
    if (state->cancellable) {
        g_cancellable_cancel(state->cancellable);
    }
    if (state->loop) {
        g_main_loop_quit(state->loop);
    }
    return G_SOURCE_REMOVE;
}

static void initGioOpStateStorage(GioOpState *state, int timeoutMs, const QString &timeoutError)
{
    if (!state) {
        return;
    }
    state->loop = g_main_loop_new(nullptr, false);
    state->cancellable = g_cancellable_new();
    state->timedOut = false;
    state->error.clear();
    state->timeoutError = timeoutError.trimmed();
    if (timeoutMs > 0) {
        state->timeoutSourceId = g_timeout_add(timeoutMs, onGioOperationTimeoutStorage, state);
    }
}

static void cleanupGioOpStateStorage(GioOpState *state)
{
    if (!state) {
        return;
    }
    if (state->timeoutSourceId > 0) {
        g_source_remove(state->timeoutSourceId);
        state->timeoutSourceId = 0;
    }
    if (state->cancellable) {
        g_object_unref(state->cancellable);
        state->cancellable = nullptr;
    }
    if (state->loop) {
        g_main_loop_unref(state->loop);
        state->loop = nullptr;
    }
}

static bool stringEqualsInsensitiveStorage(const QString &a, const QString &b)
{
    return a.compare(b, Qt::CaseInsensitive) == 0;
}

static QString volumeIdStorage(GVolume *volume, const char *kind)
{
    if (!volume || !kind) {
        return QString();
    }
    gchar *raw = g_volume_get_identifier(volume, kind);
    const QString id = QString::fromUtf8(raw ? raw : "").trimmed();
    if (raw) {
        g_free(raw);
    }
    return id;
}

static bool volumeMatchesTargetStorage(GVolume *volume, const QString &target)
{
    if (!volume) {
        return false;
    }
    const QString t = target.trimmed();
    if (t.isEmpty()) {
        return false;
    }
    if (stringEqualsInsensitiveStorage(volumeIdStorage(volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE), t)
            || stringEqualsInsensitiveStorage(volumeIdStorage(volume, G_VOLUME_IDENTIFIER_KIND_UUID), t)
            || stringEqualsInsensitiveStorage(volumeIdStorage(volume, G_VOLUME_IDENTIFIER_KIND_LABEL), t)) {
        return true;
    }
    gchar *uuidRaw = g_volume_get_uuid(volume);
    const QString uuid = QString::fromUtf8(uuidRaw ? uuidRaw : "").trimmed();
    if (uuidRaw) {
        g_free(uuidRaw);
    }
    return stringEqualsInsensitiveStorage(uuid, t);
}

static bool mountMatchesTargetStorage(GMount *mount, const QString &target)
{
    if (!mount) {
        return false;
    }
    const QString t = target.trimmed();
    if (t.isEmpty()) {
        return false;
    }
    GFile *root = g_mount_get_root(mount);
    const QString rootPath = gfileToPathOrUriStorageLocal(root);
    if (root) {
        g_object_unref(root);
    }
    if (stringEqualsInsensitiveStorage(rootPath, t)) {
        return true;
    }
    gchar *uuidRaw = g_mount_get_uuid(mount);
    const QString uuid = QString::fromUtf8(uuidRaw ? uuidRaw : "").trimmed();
    if (uuidRaw) {
        g_free(uuidRaw);
    }
    if (stringEqualsInsensitiveStorage(uuid, t)) {
        return true;
    }
    GVolume *volume = g_mount_get_volume(mount);
    const bool matchByVolume = volumeMatchesTargetStorage(volume, t);
    if (volume) {
        g_object_unref(volume);
    }
    return matchByVolume;
}

static void onVolumeMountFinishedStorage(GObject *sourceObject,
                                         GAsyncResult *result,
                                         gpointer userData)
{
    auto *state = static_cast<GioOpState *>(userData);
    GError *error = nullptr;
    const gboolean ok = g_volume_mount_finish(G_VOLUME(sourceObject), result, &error);
    state->ok = ok;
    if (!ok) {
        if (!state->timedOut) {
            state->error = QString::fromUtf8(error && error->message ? error->message : "mount-failed");
        }
    } else {
        GMount *mount = g_volume_get_mount(G_VOLUME(sourceObject));
        if (mount) {
            GFile *root = g_mount_get_root(mount);
            state->path = gfileToPathOrUriStorageLocal(root);
            if (root) {
                g_object_unref(root);
            }
            g_object_unref(mount);
        }
    }
    if (error) {
        g_error_free(error);
    }
    if (state->loop) {
        g_main_loop_quit(state->loop);
    }
}

static void onFileMountEnclosingFinishedStorage(GObject *sourceObject,
                                                GAsyncResult *result,
                                                gpointer userData)
{
    auto *state = static_cast<GioOpState *>(userData);
    GError *error = nullptr;
    const gboolean ok = g_file_mount_enclosing_volume_finish(G_FILE(sourceObject), result, &error);
    state->ok = ok;
    if (!ok) {
        if (!state->timedOut) {
            state->error = QString::fromUtf8(error && error->message ? error->message : "mount-failed");
        }
    } else {
        GError *findErr = nullptr;
        GMount *mount = g_file_find_enclosing_mount(G_FILE(sourceObject), nullptr, &findErr);
        if (mount) {
            GFile *root = g_mount_get_root(mount);
            state->path = gfileToPathOrUriStorageLocal(root);
            if (root) {
                g_object_unref(root);
            }
            g_object_unref(mount);
        } else if (findErr) {
            if (!state->timedOut) {
                state->error = QString::fromUtf8(findErr->message ? findErr->message : "mount-failed");
            }
            state->ok = false;
            g_error_free(findErr);
        }
    }
    if (error) {
        g_error_free(error);
    }
    if (state->loop) {
        g_main_loop_quit(state->loop);
    }
}

static void onMountUnmountFinishedStorage(GObject *sourceObject,
                                          GAsyncResult *result,
                                          gpointer userData)
{
    auto *state = static_cast<GioOpState *>(userData);
    GError *error = nullptr;
    const gboolean ok = g_mount_unmount_with_operation_finish(G_MOUNT(sourceObject), result, &error);
    state->ok = ok;
    if (!ok) {
        if (!state->timedOut) {
            state->error = QString::fromUtf8(error && error->message ? error->message : "eject-failed");
        }
    }
    if (error) {
        g_error_free(error);
    }
    if (state->loop) {
        g_main_loop_quit(state->loop);
    }
}

static QVariantMap mountWithGioStorage(const QString &target)
{
    const QString t = target.trimmed();
    if (t.isEmpty()) {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("invalid-device")}};
    }
    if (t.contains(QStringLiteral("://"))) {
        GFile *file = g_file_new_for_uri(t.toUtf8().constData());
        if (!file) {
            return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("invalid-uri")}};
        }
        GioOpState state;
        initGioOpStateStorage(&state, kGioMountTimeoutMs, QStringLiteral("mount-timeout"));
        g_file_mount_enclosing_volume(file,
                                      G_MOUNT_MOUNT_NONE,
                                      nullptr,
                                      state.cancellable,
                                      onFileMountEnclosingFinishedStorage,
                                      &state);
        g_main_loop_run(state.loop);
        cleanupGioOpStateStorage(&state);
        g_object_unref(file);
        if (!state.ok) {
            return {{QStringLiteral("ok"), false},
                    {QStringLiteral("error"), state.error.isEmpty() ? QStringLiteral("mount-failed")
                                                                    : state.error}};
        }
        return {{QStringLiteral("ok"), true},
                {QStringLiteral("path"), state.path.isEmpty() ? t : state.path},
                {QStringLiteral("mountPath"), state.path.isEmpty() ? t : state.path}};
    }

    GVolumeMonitor *monitor = g_volume_monitor_get();
    if (!monitor) {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("gio-monitor-unavailable")}};
    }
    GList *volumes = g_volume_monitor_get_volumes(monitor);
    GVolume *match = nullptr;
    for (GList *it = volumes; it != nullptr; it = it->next) {
        GVolume *volume = G_VOLUME(it->data);
        if (volumeMatchesTargetStorage(volume, t)) {
            match = G_VOLUME(g_object_ref(volume));
            break;
        }
    }
    g_list_free_full(volumes, g_object_unref);
    if (!match) {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("volume-not-found")}};
    }

    GMount *alreadyMounted = g_volume_get_mount(match);
    if (alreadyMounted) {
        GFile *root = g_mount_get_root(alreadyMounted);
        const QString mountedPath = gfileToPathOrUriStorageLocal(root);
        if (root) {
            g_object_unref(root);
        }
        g_object_unref(alreadyMounted);
        g_object_unref(match);
        return {{QStringLiteral("ok"), true},
                {QStringLiteral("path"), mountedPath.isEmpty() ? t : mountedPath},
                {QStringLiteral("mountPath"), mountedPath.isEmpty() ? t : mountedPath}};
    }

    GioOpState state;
    initGioOpStateStorage(&state, kGioMountTimeoutMs, QStringLiteral("mount-timeout"));
    g_volume_mount(match,
                   G_MOUNT_MOUNT_NONE,
                   nullptr,
                   state.cancellable,
                   onVolumeMountFinishedStorage,
                   &state);
    g_main_loop_run(state.loop);
    cleanupGioOpStateStorage(&state);
    g_object_unref(match);
    if (!state.ok) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), state.error.isEmpty() ? QStringLiteral("mount-failed")
                                                                : state.error}};
    }
    return {{QStringLiteral("ok"), true},
            {QStringLiteral("path"), state.path.isEmpty() ? t : state.path},
            {QStringLiteral("mountPath"), state.path.isEmpty() ? t : state.path}};
}

static QVariantMap unmountWithGioStorage(const QString &target)
{
    const QString t = target.trimmed();
    if (t.isEmpty()) {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("invalid-device")}};
    }
    GVolumeMonitor *monitor = g_volume_monitor_get();
    if (!monitor) {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("gio-monitor-unavailable")}};
    }
    GList *mounts = g_volume_monitor_get_mounts(monitor);
    GMount *match = nullptr;
    for (GList *it = mounts; it != nullptr; it = it->next) {
        GMount *mount = G_MOUNT(it->data);
        if (mountMatchesTargetStorage(mount, t)) {
            match = G_MOUNT(g_object_ref(mount));
            break;
        }
    }
    g_list_free_full(mounts, g_object_unref);
    if (!match) {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("mount-not-found")}};
    }

    GioOpState state;
    initGioOpStateStorage(&state, kGioUnmountTimeoutMs, QStringLiteral("eject-timeout"));
    g_mount_unmount_with_operation(match,
                                   G_MOUNT_UNMOUNT_NONE,
                                   nullptr,
                                   state.cancellable,
                                   onMountUnmountFinishedStorage,
                                   &state);
    g_main_loop_run(state.loop);
    cleanupGioOpStateStorage(&state);
    g_object_unref(match);
    if (!state.ok) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), state.error.isEmpty() ? QStringLiteral("eject-failed")
                                                                : state.error}};
    }
    return {{QStringLiteral("ok"), true}};
}

} // namespace

QVariantList FileManagerApi::queryStorageLocationsSync(int lsblkTimeoutMs) const
{
    Q_UNUSED(lsblkTimeoutMs)
    QVariantMap serviceReply;
    if (callStorageServiceStorage(QStringLiteral("GetStorageLocations"), {}, &serviceReply, nullptr)) {
        if (serviceReply.value(QStringLiteral("ok")).toBool()) {
            return serviceReply.value(QStringLiteral("rows")).toList();
        }
    }
    QVariantMap devicesReply;
    if (callDevicesServiceStorage(QStringLiteral("GetStorageLocations"), {}, &devicesReply, nullptr)) {
        if (devicesReply.value(QStringLiteral("ok")).toBool()) {
            return devicesReply.value(QStringLiteral("rows")).toList();
        }
    }

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
    {
        QMutexLocker locker(&m_storageCacheMutex);
        if (m_storageRefreshPending) {
            return;
        }
        m_storageRefreshPending = true;
    }

    QPointer<FileManagerApi> self(this);
    std::thread([self]() {
        if (!self) {
            return;
        }
        const QVariantList rows = self->queryStorageLocationsSync(800);
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self.data(), [self, rows]() {
            if (!self) {
                return;
            }
            {
                QMutexLocker locker(&self->m_storageCacheMutex);
                self->m_storageLocationsCache = rows;
                self->m_storageLocationsCacheMs = QDateTime::currentMSecsSinceEpoch();
                self->m_storageRefreshPending = false;
            }
            emit self->storageLocationsUpdated(rows);
        }, Qt::QueuedConnection);
    }).detach();
}

QVariantMap FileManagerApi::mountStorageDevice(const QString &devicePath) const
{
    const QString dev = devicePath.trimmed();
    if (dev.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-device"));
    }

    QVariantMap storageReply;
    QString storageError;
    if (callStorageServiceStorage(QStringLiteral("Mount"), {dev}, &storageReply, &storageError)) {
        const bool ok = storageReply.value(QStringLiteral("ok")).toBool();
        const QString error = storageReply.value(QStringLiteral("error")).toString().trimmed();
        const QString mountedPath = storageReply.value(QStringLiteral("mountPath")).toString().trimmed();
        if (!ok) {
            return makeResult(false, error.isEmpty() ? QStringLiteral("mount-failed") : error);
        }
        return makeResult(true, QString(),
                          {{QStringLiteral("device"), dev},
                           {QStringLiteral("path"), mountedPath.isEmpty() ? dev : mountedPath}});
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

    const QVariantMap gioResult = mountWithGioStorage(dev);
    if (!gioResult.value(QStringLiteral("ok")).toBool()) {
        const QString err = gioResult.value(QStringLiteral("error")).toString().trimmed();
        const QString fallbackError = daemonError.isEmpty() ? storageError : daemonError;
        return makeResult(false,
                          err.isEmpty()
                              ? (fallbackError.isEmpty() ? QStringLiteral("mount-failed") : fallbackError)
                              : err);
    }
    const QString mountedPath = gioResult.value(QStringLiteral("path")).toString().trimmed();
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

    QVariantMap storageReply;
    QString storageError;
    if (callStorageServiceStorage(QStringLiteral("Eject"), {dev}, &storageReply, &storageError)) {
        const bool ok = storageReply.value(QStringLiteral("ok")).toBool();
        const QString error = storageReply.value(QStringLiteral("error")).toString().trimmed();
        if (!ok) {
            return makeResult(false, error.isEmpty() ? QStringLiteral("eject-failed") : error);
        }
        return makeResult(true, QString(), {{QStringLiteral("device"), dev}});
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

    const QVariantMap gioResult = unmountWithGioStorage(dev);
    if (!gioResult.value(QStringLiteral("ok")).toBool()) {
        const QString err = gioResult.value(QStringLiteral("error")).toString().trimmed();
        const QString fallbackError = daemonError.isEmpty() ? storageError : daemonError;
        return makeResult(false,
                          err.isEmpty()
                              ? (fallbackError.isEmpty() ? QStringLiteral("eject-failed") : fallbackError)
                              : err);
    }
    return makeResult(true, QString(), {{QStringLiteral("device"), dev}});
}

QVariantMap FileManagerApi::storagePolicyForPath(const QString &path) const
{
    const QString target = path.trimmed();
    if (target.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-path"));
    }

    QVariantMap reply;
    QString daemonError;
    if (!callStorageServiceStorage(QStringLiteral("StoragePolicyForPath"), {target}, &reply, &daemonError)) {
        return makeResult(false, daemonError.isEmpty() ? QStringLiteral("daemon-unavailable") : daemonError);
    }
    if (!reply.value(QStringLiteral("ok")).toBool()) {
        return makeResult(false,
                          reply.value(QStringLiteral("error")).toString().trimmed().isEmpty()
                              ? QStringLiteral("policy-query-failed")
                              : reply.value(QStringLiteral("error")).toString().trimmed());
    }
    return reply;
}

QVariantMap FileManagerApi::setStoragePolicyForPath(const QString &path,
                                                    const QVariantMap &policyPatch,
                                                    const QString &scope)
{
    const QString target = path.trimmed();
    if (target.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-path"));
    }

    QVariantMap reply;
    QString daemonError;
    if (!callStorageServiceStorage(QStringLiteral("SetStoragePolicyForPath"),
                                   {target, policyPatch, scope.trimmed()},
                                   &reply,
                                   &daemonError)) {
        return makeResult(false, daemonError.isEmpty() ? QStringLiteral("daemon-unavailable") : daemonError);
    }
    if (!reply.value(QStringLiteral("ok")).toBool()) {
        return makeResult(false,
                          reply.value(QStringLiteral("error")).toString().trimmed().isEmpty()
                              ? QStringLiteral("policy-update-failed")
                              : reply.value(QStringLiteral("error")).toString().trimmed());
    }
    refreshStorageLocationsAsync();
    return reply;
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
