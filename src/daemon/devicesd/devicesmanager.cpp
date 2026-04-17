#include "devicesmanager.h"

#include "../../services/storage/storagemanager.h"

#include <QProcess>
#include <QString>
#include <QStringList>
#include <QUrl>

#pragma push_macro("signals")
#undef signals
#include <gio/gio.h>
#pragma pop_macro("signals")

// ---------------------------------------------------------------------------
// GIO async helpers (used by Mount, Eject, Unlock)
// ---------------------------------------------------------------------------

namespace {

struct GioOpState {
    GMainLoop *loop = nullptr;
    bool finished = false;
    bool ok = false;
    bool timedOut = false;
    QString error;
    QString path;
    QString timeoutError;
    GCancellable *cancellable = nullptr;
    guint timeoutSourceId = 0;
};

struct GioUnlockState {
    GMainLoop *loop = nullptr;
    bool finished = false;
    bool ok = false;
    bool timedOut = false;
    QString error;
    QString path;
    QString passphrase;
    QString timeoutError;
    GCancellable *cancellable = nullptr;
    guint timeoutSourceId = 0;
};

constexpr int kGioMountTimeoutMs = 15000;
constexpr int kGioUnmountTimeoutMs = 15000;
constexpr int kGioUnlockTimeoutMs = 20000;

gboolean onGioOpTimeout(gpointer userData)
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

gboolean onGioUnlockTimeout(gpointer userData)
{
    auto *state = static_cast<GioUnlockState *>(userData);
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

void initGioOpState(GioOpState *state, int timeoutMs, const QString &timeoutError)
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
        state->timeoutSourceId = g_timeout_add(timeoutMs, onGioOpTimeout, state);
    }
}

void cleanupGioOpState(GioOpState *state)
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

void initGioUnlockState(GioUnlockState *state, int timeoutMs, const QString &timeoutError)
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
        state->timeoutSourceId = g_timeout_add(timeoutMs, onGioUnlockTimeout, state);
    }
}

void cleanupGioUnlockState(GioUnlockState *state)
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

// Map GIO error to a stable error code string.
static QString mapGioError(GError *err, const QString &fallback)
{
    if (!err) {
        return fallback;
    }
    if (err->domain == G_IO_ERROR) {
        switch (err->code) {
        case G_IO_ERROR_PERMISSION_DENIED:
            return QStringLiteral("permission-denied");
        case G_IO_ERROR_BUSY:
            return QStringLiteral("drive-busy");
        case G_IO_ERROR_NOT_SUPPORTED:
            return QStringLiteral("drive-unsupported");
        case G_IO_ERROR_ALREADY_MOUNTED:
            return QStringLiteral("already-mounted");
        case G_IO_ERROR_NOT_MOUNTED:
            return QStringLiteral("not-mounted");
        default:
            break;
        }
    }
    const QString msg = QString::fromUtf8(err->message ? err->message : "").toLower();
    if (msg.contains(QStringLiteral("locked")) || msg.contains(QStringLiteral("luks"))
            || msg.contains(QStringLiteral("passphrase")) || msg.contains(QStringLiteral("password"))) {
        return QStringLiteral("drive-locked");
    }
    if (msg.contains(QStringLiteral("busy")) || msg.contains(QStringLiteral("in use"))) {
        return QStringLiteral("drive-busy");
    }
    if (msg.contains(QStringLiteral("permission")) || msg.contains(QStringLiteral("denied"))) {
        return QStringLiteral("permission-denied");
    }
    if (msg.contains(QStringLiteral("not supported")) || msg.contains(QStringLiteral("unsupported"))) {
        return QStringLiteral("drive-unsupported");
    }
    return fallback;
}

static QString gfileToPathOrUri(GFile *file)
{
    if (!file) {
        return QString();
    }
    gchar *path = g_file_get_path(file);
    if (path) {
        const QString out = QString::fromUtf8(path);
        g_free(path);
        return out;
    }
    gchar *uri = g_file_get_uri(file);
    const QString out = uri ? QString::fromUtf8(uri) : QString();
    if (uri) {
        g_free(uri);
    }
    return out;
}

// --- Mount callbacks ---

void onVolumeMountFinished(GObject *sourceObject, GAsyncResult *result, gpointer userData)
{
    auto *state = static_cast<GioOpState *>(userData);
    GError *error = nullptr;
    const gboolean ok = g_volume_mount_finish(G_VOLUME(sourceObject), result, &error);
    state->ok = ok;
    if (ok) {
        GMount *mount = g_volume_get_mount(G_VOLUME(sourceObject));
        if (mount) {
            GFile *root = g_mount_get_root(mount);
            state->path = gfileToPathOrUri(root);
            if (root) {
                g_object_unref(root);
            }
            g_object_unref(mount);
        }
    } else {
        if (!state->timedOut) {
            state->error = mapGioError(error, QStringLiteral("mount-failed"));
        }
    }
    if (error) {
        g_error_free(error);
    }
    state->finished = true;
    if (state->loop) {
        g_main_loop_quit(state->loop);
    }
}

void onFileMountEnclosingFinished(GObject *sourceObject, GAsyncResult *result, gpointer userData)
{
    auto *state = static_cast<GioOpState *>(userData);
    GError *error = nullptr;
    const gboolean ok = g_file_mount_enclosing_volume_finish(G_FILE(sourceObject), result, &error);
    state->ok = ok;
    if (ok) {
        GError *findErr = nullptr;
        GMount *mount = g_file_find_enclosing_mount(G_FILE(sourceObject), nullptr, &findErr);
        if (mount) {
            GFile *root = g_mount_get_root(mount);
            state->path = gfileToPathOrUri(root);
            if (root) {
                g_object_unref(root);
            }
            g_object_unref(mount);
        } else if (findErr) {
            if (!state->timedOut) {
                state->error = mapGioError(findErr, QStringLiteral("mount-failed"));
            }
            state->ok = false;
            g_error_free(findErr);
        }
    } else {
        if (!state->timedOut) {
            state->error = mapGioError(error, QStringLiteral("mount-failed"));
        }
    }
    if (error) {
        g_error_free(error);
    }
    state->finished = true;
    if (state->loop) {
        g_main_loop_quit(state->loop);
    }
}

// --- Eject / unmount callbacks ---

void onMountUnmountFinished(GObject *sourceObject, GAsyncResult *result, gpointer userData)
{
    auto *state = static_cast<GioOpState *>(userData);
    GError *error = nullptr;
    const gboolean ok = g_mount_unmount_with_operation_finish(G_MOUNT(sourceObject), result, &error);
    state->ok = ok;
    if (!ok) {
        if (!state->timedOut) {
            state->error = mapGioError(error, QStringLiteral("eject-failed"));
        }
    }
    if (error) {
        g_error_free(error);
    }
    state->finished = true;
    if (state->loop) {
        g_main_loop_quit(state->loop);
    }
}

void onDriveEjectFinished(GObject *sourceObject, GAsyncResult *result, gpointer userData)
{
    auto *state = static_cast<GioOpState *>(userData);
    GError *error = nullptr;
    const gboolean ok = g_drive_eject_with_operation_finish(G_DRIVE(sourceObject), result, &error);
    state->ok = ok;
    if (!ok) {
        if (!state->timedOut) {
            state->error = mapGioError(error, QStringLiteral("eject-drive-failed"));
        }
    }
    if (error) {
        g_error_free(error);
    }
    state->finished = true;
    if (state->loop) {
        g_main_loop_quit(state->loop);
    }
}

// --- Unlock (mount with password) callbacks ---

void onAskPassword(GMountOperation *op,
                   const gchar * /*message*/,
                   const gchar * /*defaultUser*/,
                   const gchar * /*defaultDomain*/,
                   GAskPasswordFlags /*flags*/,
                   gpointer userData)
{
    auto *state = static_cast<GioUnlockState *>(userData);
    g_mount_operation_set_password(op, state->passphrase.toUtf8().constData());
    g_mount_operation_reply(op, G_MOUNT_OPERATION_HANDLED);
}

void onVolumeMountUnlockFinished(GObject *sourceObject, GAsyncResult *result, gpointer userData)
{
    auto *state = static_cast<GioUnlockState *>(userData);
    GError *error = nullptr;
    const gboolean ok = g_volume_mount_finish(G_VOLUME(sourceObject), result, &error);
    state->ok = ok;
    if (ok) {
        GMount *mount = g_volume_get_mount(G_VOLUME(sourceObject));
        if (mount) {
            GFile *root = g_mount_get_root(mount);
            state->path = gfileToPathOrUri(root);
            if (root) {
                g_object_unref(root);
            }
            g_object_unref(mount);
        }
    } else {
        if (!state->timedOut) {
            state->error = mapGioError(error, QStringLiteral("unlock-failed"));
        }
    }
    if (error) {
        g_error_free(error);
    }
    state->finished = true;
    if (state->loop) {
        g_main_loop_quit(state->loop);
    }
}

// --- Volume / mount matching helpers ---

static bool strEqI(const QString &a, const QString &b)
{
    return a.compare(b, Qt::CaseInsensitive) == 0;
}

static QString volumeId(GVolume *volume, const char *kind)
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

static bool volumeMatchesTarget(GVolume *volume, const QString &target)
{
    if (!volume || target.trimmed().isEmpty()) {
        return false;
    }
    const QString t = target.trimmed();
    if (strEqI(volumeId(volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE), t)
            || strEqI(volumeId(volume, G_VOLUME_IDENTIFIER_KIND_UUID), t)
            || strEqI(volumeId(volume, G_VOLUME_IDENTIFIER_KIND_LABEL), t)) {
        return true;
    }
    gchar *uuidRaw = g_volume_get_uuid(volume);
    const QString uuid = QString::fromUtf8(uuidRaw ? uuidRaw : "").trimmed();
    if (uuidRaw) {
        g_free(uuidRaw);
    }
    return strEqI(uuid, t);
}

static bool mountMatchesTarget(GMount *mount, const QString &target)
{
    if (!mount || target.trimmed().isEmpty()) {
        return false;
    }
    const QString t = target.trimmed();

    GFile *root = g_mount_get_root(mount);
    const QString rootPath = gfileToPathOrUri(root);
    if (root) {
        g_object_unref(root);
    }
    if (strEqI(rootPath, t)) {
        return true;
    }

    gchar *uuidRaw = g_mount_get_uuid(mount);
    const QString uuid = QString::fromUtf8(uuidRaw ? uuidRaw : "").trimmed();
    if (uuidRaw) {
        g_free(uuidRaw);
    }
    if (strEqI(uuid, t)) {
        return true;
    }

    GVolume *volume = g_mount_get_volume(mount);
    const bool match = volumeMatchesTarget(volume, t);
    if (volume) {
        g_object_unref(volume);
    }
    return match;
}

} // namespace

// ---------------------------------------------------------------------------
// DevicesManager
// ---------------------------------------------------------------------------

DevicesManager::DevicesManager(QObject *parent)
    : QObject(parent)
    , m_storageManager(new Slm::Storage::StorageManager(this))
{
}

QVariantMap DevicesManager::GetStorageLocations() const
{
    const QVariantList rows = m_storageManager->queryStorageLocationsSync();
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("rows"), rows},
    };
}

QVariantMap DevicesManager::StoragePolicyForPath(const QString &path) const
{
    if (!m_storageManager) {
        return makeResult(false, QStringLiteral("manager-unavailable"));
    }
    return m_storageManager->storagePolicyForPath(path);
}

QVariantMap DevicesManager::SetStoragePolicyForPath(const QString &path,
                                                    const QVariantMap &policyPatch,
                                                    const QString &scope)
{
    if (!m_storageManager) {
        return makeResult(false, QStringLiteral("manager-unavailable"));
    }
    const QVariantMap out = m_storageManager->setStoragePolicyForPath(path, policyPatch, scope);
    if (out.value(QStringLiteral("ok")).toBool()) {
        emit StorageChanged();
    }
    return out;
}

QVariantMap DevicesManager::Mount(const QString &target)
{
    const QString t = target.trimmed();
    if (t.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-target"));
    }

    // URI target: mount via g_file_mount_enclosing_volume
    if (t.contains(QStringLiteral("://"))) {
        GFile *file = g_file_new_for_uri(t.toUtf8().constData());
        if (!file) {
            return makeResult(false, QStringLiteral("invalid-uri"));
        }
        GioOpState state;
        initGioOpState(&state, kGioMountTimeoutMs, QStringLiteral("mount-timeout"));
        g_file_mount_enclosing_volume(file,
                                      G_MOUNT_MOUNT_NONE,
                                      nullptr,
                                      state.cancellable,
                                      onFileMountEnclosingFinished,
                                      &state);
        g_main_loop_run(state.loop);
        cleanupGioOpState(&state);
        g_object_unref(file);
        if (!state.ok) {
            return makeResult(false, state.error.isEmpty() ? QStringLiteral("mount-failed") : state.error);
        }
        emit StorageChanged();
        return {{QStringLiteral("ok"), true},
                {QStringLiteral("path"), state.path.isEmpty() ? t : state.path},
                {QStringLiteral("mountPath"), state.path.isEmpty() ? t : state.path}};
    }

    // Block device / UUID: find volume via GVolumeMonitor
    GVolumeMonitor *monitor = g_volume_monitor_get();
    if (!monitor) {
        return makeResult(false, QStringLiteral("gio-monitor-unavailable"));
    }

    GList *volumes = g_volume_monitor_get_volumes(monitor);
    GVolume *match = nullptr;
    for (GList *it = volumes; it != nullptr; it = it->next) {
        GVolume *vol = G_VOLUME(it->data);
        if (volumeMatchesTarget(vol, t)) {
            match = G_VOLUME(g_object_ref(vol));
            break;
        }
    }
    g_list_free_full(volumes, g_object_unref);

    if (!match) {
        return makeResult(false, QStringLiteral("volume-not-found"));
    }

    // Already mounted?
    GMount *alreadyMounted = g_volume_get_mount(match);
    if (alreadyMounted) {
        GFile *root = g_mount_get_root(alreadyMounted);
        const QString mountedPath = gfileToPathOrUri(root);
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
    initGioOpState(&state, kGioMountTimeoutMs, QStringLiteral("mount-timeout"));
    g_volume_mount(match,
                   G_MOUNT_MOUNT_NONE,
                   nullptr,
                   state.cancellable,
                   onVolumeMountFinished,
                   &state);
    g_main_loop_run(state.loop);
    cleanupGioOpState(&state);
    g_object_unref(match);

    if (!state.ok) {
        return makeResult(false, state.error.isEmpty() ? QStringLiteral("mount-failed") : state.error);
    }
    emit StorageChanged();
    return {{QStringLiteral("ok"), true},
            {QStringLiteral("path"), state.path.isEmpty() ? t : state.path},
            {QStringLiteral("mountPath"), state.path.isEmpty() ? t : state.path}};
}

QVariantMap DevicesManager::Eject(const QString &target)
{
    const QString t = target.trimmed();
    if (t.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-target"));
    }

    GVolumeMonitor *monitor = g_volume_monitor_get();
    if (!monitor) {
        return makeResult(false, QStringLiteral("gio-monitor-unavailable"));
    }

    // Find matching mount
    GList *mounts = g_volume_monitor_get_mounts(monitor);
    GMount *match = nullptr;
    for (GList *it = mounts; it != nullptr; it = it->next) {
        GMount *mount = G_MOUNT(it->data);
        if (mountMatchesTarget(mount, t)) {
            match = G_MOUNT(g_object_ref(mount));
            break;
        }
    }
    g_list_free_full(mounts, g_object_unref);

    if (!match) {
        return makeResult(false, QStringLiteral("mount-not-found"));
    }

    // Unmount first
    {
        GioOpState state;
        initGioOpState(&state, kGioUnmountTimeoutMs, QStringLiteral("eject-timeout"));
        g_mount_unmount_with_operation(match,
                                       G_MOUNT_UNMOUNT_NONE,
                                       nullptr,
                                       state.cancellable,
                                       onMountUnmountFinished,
                                       &state);
        g_main_loop_run(state.loop);
        cleanupGioOpState(&state);
        if (!state.ok) {
            g_object_unref(match);
            return makeResult(false, state.error.isEmpty() ? QStringLiteral("eject-failed") : state.error);
        }
    }

    // Power-off the drive if possible
    GVolume *volume = g_mount_get_volume(match);
    g_object_unref(match);

    if (volume) {
        GDrive *drive = g_volume_get_drive(volume);
        g_object_unref(volume);
        if (drive && g_drive_can_eject(drive)) {
            GioOpState driveState;
            initGioOpState(&driveState, kGioUnmountTimeoutMs, QStringLiteral("eject-timeout"));
            g_drive_eject_with_operation(drive,
                                         G_MOUNT_UNMOUNT_NONE,
                                         nullptr,
                                         driveState.cancellable,
                                         onDriveEjectFinished,
                                         &driveState);
            g_main_loop_run(driveState.loop);
            cleanupGioOpState(&driveState);
            g_object_unref(drive);
            // Drive eject failure is non-fatal — unmount succeeded
        } else if (drive) {
            g_object_unref(drive);
        }
    }

    emit StorageChanged();
    return makeResult(true, QString());
}

QVariantMap DevicesManager::Unlock(const QString &target, const QString &passphrase)
{
    const QString t = target.trimmed();
    if (t.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-target"));
    }
    if (passphrase.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-passphrase"));
    }

    GVolumeMonitor *monitor = g_volume_monitor_get();
    if (!monitor) {
        return makeResult(false, QStringLiteral("gio-monitor-unavailable"));
    }

    GList *volumes = g_volume_monitor_get_volumes(monitor);
    GVolume *match = nullptr;
    for (GList *it = volumes; it != nullptr; it = it->next) {
        GVolume *vol = G_VOLUME(it->data);
        if (volumeMatchesTarget(vol, t)) {
            match = G_VOLUME(g_object_ref(vol));
            break;
        }
    }
    g_list_free_full(volumes, g_object_unref);

    if (!match) {
        return makeResult(false, QStringLiteral("volume-not-found"));
    }

    GMountOperation *op = g_mount_operation_new();
    GioUnlockState state;
    state.passphrase = passphrase;
    initGioUnlockState(&state, kGioUnlockTimeoutMs, QStringLiteral("unlock-timeout"));
    g_signal_connect(op, "ask-password", G_CALLBACK(onAskPassword), &state);

    g_volume_mount(match,
                   G_MOUNT_MOUNT_NONE,
                   op,
                   state.cancellable,
                   onVolumeMountUnlockFinished,
                   &state);
    g_main_loop_run(state.loop);
    cleanupGioUnlockState(&state);
    g_object_unref(op);
    g_object_unref(match);

    if (!state.ok) {
        return makeResult(false, state.error.isEmpty() ? QStringLiteral("unlock-failed") : state.error);
    }
    emit StorageChanged();
    return {{QStringLiteral("ok"), true},
            {QStringLiteral("path"), state.path},
            {QStringLiteral("mountPath"), state.path}};
}

QVariantMap DevicesManager::Format(const QString &target, const QString &filesystem)
{
    const QString t = target.trimmed();
    QString fs = filesystem.trimmed().toLower();
    if (t.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-target"));
    }
    if (t.contains(QStringLiteral("://"))) {
        return makeResult(false, QStringLiteral("format-uri-not-supported"));
    }
    if (fs.isEmpty()) {
        fs = QStringLiteral("ext4");
    }
    const QVariantMap result = runCommand(QStringLiteral("udisksctl"),
                                          {QStringLiteral("format"),
                                           QStringLiteral("-b"), t,
                                           QStringLiteral("--type"), fs},
                                          120000);
    if (result.value(QStringLiteral("ok")).toBool()) {
        emit StorageChanged();
    }
    return result;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

QVariantMap DevicesManager::makeResult(bool ok, const QString &error)
{
    return {{QStringLiteral("ok"), ok}, {QStringLiteral("error"), error}};
}

QVariantMap DevicesManager::runCommand(const QString &program,
                                       const QStringList &args,
                                       int timeoutMs)
{
    QProcess p;
    p.start(program, args);
    if (!p.waitForStarted(5000)) {
        return makeResult(false, QStringLiteral("start-failed"));
    }
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(2000);
        return makeResult(false, QStringLiteral("timeout"));
    }
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        const QString err = QString::fromUtf8(p.readAllStandardError()).trimmed();
        return makeResult(false, err.isEmpty() ? QStringLiteral("command-failed") : err);
    }
    return makeResult(true, QString());
}
