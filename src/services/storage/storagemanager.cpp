#include "storagemanager.h"
#include "storagepolicystore.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QMetaObject>
#include <QRegularExpression>
#include <QSet>
#include <QStorageInfo>
#include <QStringList>
#include <QUrl>

#pragma push_macro("signals")
#undef signals
#include <gio/gio.h>
#pragma pop_macro("signals")

namespace Slm::Storage {

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

void onStorageMonitorSignal(GVolumeMonitor *, gpointer, gpointer userData)
{
    auto *self = static_cast<StorageManager *>(userData);
    if (!self) {
        return;
    }
    QMetaObject::invokeMethod(self, "queueStorageChanged", Qt::QueuedConnection);
}

static QString gfileToPathOrUriStorageLocal(GFile *file);

enum class DbusFailure {
    None,
    Unavailable,
    Timeout,
    Error,
};

template<typename ReplyT>
DbusFailure classifyDbusReplyErrorStorage(const ReplyT &reply)
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

QString dbusFailureCodeStorage(DbusFailure failure)
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

bool callDevicesServiceStorage(const QString &method,
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

constexpr int kGioMountTimeoutMs = 15000;
constexpr int kGioUnmountTimeoutMs = 15000;

gboolean onGioOperationTimeoutStorage(gpointer userData)
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

void initGioOpStateStorage(GioOpState *state, int timeoutMs, const QString &timeoutError)
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

void cleanupGioOpStateStorage(GioOpState *state)
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

void onVolumeMountFinishedStorage(GObject *sourceObject,
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
    state->finished = true;
    if (state->loop) {
        g_main_loop_quit(state->loop);
    }
}

void onFileMountEnclosingFinishedStorage(GObject *sourceObject,
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
    state->finished = true;
    if (state->loop) {
        g_main_loop_quit(state->loop);
    }
}

void onMountUnmountFinishedStorage(GObject *sourceObject,
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
    state->finished = true;
    if (state->loop) {
        g_main_loop_quit(state->loop);
    }
}

struct StoragePolicyDecision {
    QString action;      // mount | ignore | ask
    bool autoOpen = false;
    bool visible = true;
    bool readOnly = false;
    bool exec = false;
    bool isSystem = false;
};

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

static bool stringContainsInsensitiveStorage(const QString &value, const QString &needle)
{
    return value.contains(needle, Qt::CaseInsensitive);
}

static bool isSystemFsTypeStorage(const QString &fsType)
{
    const QString fs = fsType.trimmed().toLower();
    if (fs.isEmpty()) {
        return false;
    }
    return fs == QStringLiteral("swap")
        || fs == QStringLiteral("linux_raid_member")
        || fs == QStringLiteral("lvm2_member")
        || fs == QStringLiteral("zfs_member")
        || fs == QStringLiteral("iso9660");
}

static bool looksSystemPartitionStorage(const QString &mountPath,
                                        const QString &label,
                                        const QString &fsType)
{
    const QString p = mountPath.trimmed();
    const QString l = label.trimmed();
    if (p == QStringLiteral("/")) {
        return true;
    }
    if (p.startsWith(QStringLiteral("/boot/efi"))
            || p.startsWith(QStringLiteral("/efi"))
            || p.startsWith(QStringLiteral("/recovery"))) {
        return true;
    }
    if (stringContainsInsensitiveStorage(l, QStringLiteral("EFI"))
            || stringContainsInsensitiveStorage(l, QStringLiteral("RECOVERY"))
            || stringContainsInsensitiveStorage(l, QStringLiteral("MSR"))) {
        return true;
    }
    const QString fs = fsType.trimmed().toLower();
    if (fs == QStringLiteral("vfat") && stringContainsInsensitiveStorage(l, QStringLiteral("EFI"))) {
        return true;
    }
    return isSystemFsTypeStorage(fs);
}

static StoragePolicyDecision decideStoragePolicy(const QString &uuid,
                                                 const QString &partuuid,
                                                 const QString &fsType,
                                                 bool isRemovable,
                                                 bool isSystem,
                                                 bool isEncrypted)
{
    Q_UNUSED(uuid)
    Q_UNUSED(partuuid)

    StoragePolicyDecision d;
    d.action = QStringLiteral("mount");
    d.autoOpen = false;
    d.visible = true;
    d.readOnly = false;
    d.exec = false;
    d.isSystem = isSystem;

    if (isSystem) {
        d.action = QStringLiteral("ignore");
        d.visible = false;
        return d;
    }
    if (isEncrypted) {
        d.action = QStringLiteral("ask");
        d.visible = true;
        return d;
    }

    const QString fs = fsType.trimmed().toLower();
    if (fs.isEmpty() || fs == QStringLiteral("unknown")) {
        d.action = QStringLiteral("ask");
        d.autoOpen = false;
        return d;
    }
    if (isSystemFsTypeStorage(fs)) {
        d.action = QStringLiteral("ignore");
        d.visible = false;
        d.isSystem = true;
        return d;
    }
    if (isRemovable) {
        d.action = QStringLiteral("mount");
        d.autoOpen = false;
        return d;
    }
    d.action = QStringLiteral("mount");
    d.autoOpen = false;
    return d;
}

static QString volumeIdStorage(GVolume *volume, const char *kind)
{
    if (!volume || !kind) {
        return QString();
    }
    gchar *idRaw = g_volume_get_identifier(volume, kind);
    const QString id = QString::fromUtf8(idRaw ? idRaw : "");
    if (idRaw) {
        g_free(idRaw);
    }
    return id.trimmed();
}

static QString canonicalPathStorage(const QString &path)
{
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    if (!canonical.isEmpty()) {
        return canonical;
    }
    return info.absoluteFilePath();
}

static QString resolvePartUuidForDeviceStorage(const QString &devicePath)
{
    const QString device = canonicalPathStorage(devicePath.trimmed());
    if (device.isEmpty() || !device.startsWith(QStringLiteral("/dev/"))) {
        return QString();
    }
    const QDir byPartUuidDir(QStringLiteral("/dev/disk/by-partuuid"));
    if (!byPartUuidDir.exists()) {
        return QString();
    }
    const QFileInfoList entries = byPartUuidDir.entryInfoList(QDir::Files | QDir::System | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        if (!entry.isSymLink()) {
            continue;
        }
        QString targetPath = entry.symLinkTarget();
        if (targetPath.isEmpty()) {
            continue;
        }
        if (QDir::isRelativePath(targetPath)) {
            targetPath = byPartUuidDir.absoluteFilePath(targetPath);
        }
        const QString targetCanonical = canonicalPathStorage(targetPath);
        if (targetCanonical == device) {
            return entry.fileName().trimmed();
        }
    }
    return QString();
}

static QString bestUuidForVolumeStorage(GVolume *volume)
{
    QString uuid = volumeIdStorage(volume, G_VOLUME_IDENTIFIER_KIND_UUID);
    if (!uuid.isEmpty()) {
        return uuid;
    }
    if (!volume) {
        return QString();
    }
    gchar *uuidRaw = g_volume_get_uuid(volume);
    uuid = QString::fromUtf8(uuidRaw ? uuidRaw : "").trimmed();
    if (uuidRaw) {
        g_free(uuidRaw);
    }
    return uuid;
}

static QString deviceBaseKeyStorage(const QString &device);

static int partitionIndexFromDeviceStorage(const QString &devicePath)
{
    const QString canonical = canonicalPathStorage(devicePath.trimmed());
    if (!canonical.startsWith(QStringLiteral("/dev/"))) {
        return -1;
    }
    const QString leaf = QFileInfo(canonical).fileName();
    if (leaf.startsWith(QStringLiteral("sr")) || leaf.startsWith(QStringLiteral("loop"))) {
        return -1;
    }

    const QRegularExpression pSuffix(QStringLiteral("p(\\d+)$"));
    QRegularExpressionMatch match = pSuffix.match(leaf);
    if (match.hasMatch()) {
        bool ok = false;
        const int out = match.captured(1).toInt(&ok);
        return ok ? out : -1;
    }

    const QRegularExpression trailingDigits(QStringLiteral("(\\d+)$"));
    match = trailingDigits.match(leaf);
    if (match.hasMatch()) {
        bool ok = false;
        const int out = match.captured(1).toInt(&ok);
        return ok ? out : -1;
    }
    return -1;
}

static QString trimPartitionSuffixFromDiskIdStorage(const QString &diskId)
{
    const QString in = diskId.trimmed();
    if (in.isEmpty()) {
        return QString();
    }
    const QRegularExpression partSuffix(QStringLiteral("-part\\d+$"));
    const QRegularExpressionMatch match = partSuffix.match(in);
    if (match.hasMatch()) {
        return in.left(match.capturedStart());
    }
    return in;
}

static QString resolveSerialForDeviceStorage(const QString &devicePath)
{
    static QHash<QString, QString> serialByDevice;
    static QSet<QString> unresolvedDevices;

    const QString canonical = canonicalPathStorage(devicePath.trimmed());
    if (!canonical.startsWith(QStringLiteral("/dev/"))) {
        return QString();
    }
    if (serialByDevice.contains(canonical)) {
        return serialByDevice.value(canonical);
    }
    if (unresolvedDevices.contains(canonical)) {
        return QString();
    }

    const QString baseDevice = canonicalPathStorage(deviceBaseKeyStorage(canonical));
    const QDir byIdDir(QStringLiteral("/dev/disk/by-id"));
    if (!byIdDir.exists()) {
        unresolvedDevices.insert(canonical);
        return QString();
    }

    QString serialCandidate;
    QString baseCandidate;
    const QFileInfoList entries = byIdDir.entryInfoList(QDir::Files | QDir::System | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        if (!entry.isSymLink()) {
            continue;
        }
        QString targetPath = entry.symLinkTarget();
        if (targetPath.isEmpty()) {
            continue;
        }
        if (QDir::isRelativePath(targetPath)) {
            targetPath = byIdDir.absoluteFilePath(targetPath);
        }
        const QString targetCanonical = canonicalPathStorage(targetPath);
        if (targetCanonical.isEmpty()) {
            continue;
        }

        const QString candidate = trimPartitionSuffixFromDiskIdStorage(entry.fileName());
        if (candidate.isEmpty()) {
            continue;
        }

        if (targetCanonical == canonical && serialCandidate.isEmpty()) {
            serialCandidate = candidate;
        }
        if (!baseDevice.isEmpty() && targetCanonical == baseDevice && baseCandidate.isEmpty()) {
            baseCandidate = candidate;
        }
    }

    const QString resolved = serialCandidate.isEmpty() ? baseCandidate : serialCandidate;
    if (resolved.isEmpty()) {
        unresolvedDevices.insert(canonical);
        return QString();
    }
    serialByDevice.insert(canonical, resolved);
    return resolved;
}

static bool isRemovableVolumeStorage(GVolume *volume, const QString &pathOrUri)
{
    if (volume) {
        GDrive *drive = g_volume_get_drive(volume);
        if (drive) {
            const bool removable = g_drive_is_removable(drive);
            g_object_unref(drive);
            return removable;
        }
    }
    const QString p = pathOrUri.trimmed();
    return p.startsWith(QStringLiteral("/media/")) || p.startsWith(QStringLiteral("/run/media/"));
}

static QString deviceBaseKeyStorage(const QString &device)
{
    QString d = device.trimmed();
    if (d.startsWith(QStringLiteral("/dev/")) && d.size() > 5) {
        if (d.contains(QStringLiteral("nvme")) && d.contains(QStringLiteral("p"))) {
            const int pIdx = d.lastIndexOf(QStringLiteral("p"));
            if (pIdx > 0) {
                bool ok = false;
                d.mid(pIdx + 1).toInt(&ok);
                if (ok) {
                    return d.left(pIdx);
                }
            }
        }
        int i = d.size() - 1;
        while (i >= 0 && d.at(i).isDigit()) {
            --i;
        }
        if (i >= 0 && i < d.size() - 1) {
            return d.left(i + 1);
        }
    }
    return d;
}

static QString extractHostFromUriStorage(const QString &uri)
{
    const QUrl u(uri);
    if (!u.isValid()) {
        return QString();
    }
    if (!u.host().trimmed().isEmpty()) {
        return u.host().trimmed();
    }
    return QString();
}

static QString deviceGroupKeyStorage(const QString &device,
                                     const QString &pathOrUri,
                                     const QString &label)
{
    const QString baseDev = deviceBaseKeyStorage(device);
    if (!baseDev.isEmpty()) {
        return baseDev;
    }
    const QString host = extractHostFromUriStorage(pathOrUri);
    if (!host.isEmpty()) {
        return QStringLiteral("net:") + host;
    }
    if (!label.trimmed().isEmpty()) {
        return QStringLiteral("label:") + label.trimmed().toLower();
    }
    if (!pathOrUri.trimmed().isEmpty()) {
        return QStringLiteral("path:") + pathOrUri.trimmed();
    }
    return QStringLiteral("unknown-device");
}

static QString deviceGroupLabelStorage(const QString &label, const QString &groupKey)
{
    if (!label.trimmed().isEmpty()) {
        return label.trimmed();
    }
    if (groupKey.startsWith(QStringLiteral("net:"))) {
        return groupKey.mid(4);
    }
    return QStringLiteral("External Drive");
}

static QStringList policyKeyCandidatesFromRowStorage(const QVariantMap &row)
{
    QStringList candidates;
    auto addUnique = [&candidates](const QString &key) {
        const QString normalized = key.trimmed();
        if (normalized.isEmpty() || candidates.contains(normalized)) {
            return;
        }
        candidates.push_back(normalized);
    };

    const QString uuid = row.value(QStringLiteral("uuid")).toString().trimmed();
    if (!uuid.isEmpty()) {
        addUnique(QStringLiteral("uuid:") + uuid);
        addUnique(QStringLiteral("uuid:") + uuid.toLower());
    }
    const QString partuuid = row.value(QStringLiteral("partuuid")).toString().trimmed();
    if (!partuuid.isEmpty()) {
        addUnique(QStringLiteral("partuuid:") + partuuid);
        addUnique(QStringLiteral("partuuid:") + partuuid.toLower());
    }
    const QString serial = row.value(QStringLiteral("deviceSerial")).toString().trimmed();
    const int partitionIndex = row.value(QStringLiteral("partitionIndex")).toInt();
    if (!serial.isEmpty() && partitionIndex > 0) {
        addUnique(QStringLiteral("serial-index:%1#%2").arg(serial, QString::number(partitionIndex)));
        addUnique(QStringLiteral("serial-index:%1#%2").arg(serial.toLower(), QString::number(partitionIndex)));
    }
    const QString device = row.value(QStringLiteral("device")).toString().trimmed();
    if (!device.isEmpty()) {
        addUnique(QStringLiteral("device:") + device);
    }
    const QString groupKey = row.value(QStringLiteral("deviceGroupKey")).toString().trimmed();
    if (!groupKey.isEmpty()) {
        addUnique(QStringLiteral("group:") + groupKey);
    }
    return candidates;
}

static QString policyKeyFromRowStorage(const QVariantMap &row, const QVariantMap &policyMap)
{
    const QStringList candidates = policyKeyCandidatesFromRowStorage(row);
    for (const QString &key : candidates) {
        if (policyMap.contains(key)) {
            return key;
        }
    }
    return candidates.isEmpty() ? QString() : candidates.first();
}

static QString policyScopeFromPolicyKeyStorage(const QString &policyKey)
{
    const QString key = policyKey.trimmed();
    if (key.startsWith(QStringLiteral("device:")) || key.startsWith(QStringLiteral("group:"))) {
        return QStringLiteral("device");
    }
    if (key.startsWith(QStringLiteral("uuid:"))
            || key.startsWith(QStringLiteral("partuuid:"))
            || key.startsWith(QStringLiteral("serial-index:"))) {
        return QStringLiteral("partition");
    }
    return QStringLiteral("partition");
}

static QString normalizedPathForPolicyStorage(const QString &path)
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }
    const QUrl maybeUrl(trimmed);
    if (maybeUrl.isValid() && !maybeUrl.scheme().isEmpty()) {
        return maybeUrl.toString(QUrl::StripTrailingSlash);
    }
    const QFileInfo info(trimmed);
    const QString canonical = info.canonicalFilePath();
    if (!canonical.isEmpty()) {
        return canonical;
    }
    return info.absoluteFilePath();
}

static QString choosePolicyKeyForScopeStorage(const QVariantMap &row, const QString &scope)
{
    const QString s = scope.trimmed().toLower();
    const QStringList candidates = policyKeyCandidatesFromRowStorage(row);
    if (candidates.isEmpty()) {
        return QString();
    }

    const auto isPartitionKey = [](const QString &key) {
        return key.startsWith(QStringLiteral("uuid:"))
                || key.startsWith(QStringLiteral("partuuid:"))
                || key.startsWith(QStringLiteral("serial-index:"));
    };
    const auto isDeviceKey = [](const QString &key) {
        return key.startsWith(QStringLiteral("device:"))
                || key.startsWith(QStringLiteral("group:"));
    };

    if (s == QStringLiteral("device")) {
        for (const QString &key : candidates) {
            if (isDeviceKey(key)) {
                return key;
            }
        }
    }

    for (const QString &key : candidates) {
        if (isPartitionKey(key)) {
            return key;
        }
    }
    return candidates.first();
}

static QString serialIndexPolicyKeyFromRowStorage(const QVariantMap &row)
{
    const QString serial = row.value(QStringLiteral("deviceSerial")).toString().trimmed();
    const int partitionIndex = row.value(QStringLiteral("partitionIndex")).toInt();
    if (serial.isEmpty() || partitionIndex <= 0) {
        return QString();
    }
    return QStringLiteral("serial-index:%1#%2").arg(serial.toLower(), QString::number(partitionIndex));
}

static bool promoteLegacyPolicyKeyToSerialIndexStorage(const QVariantMap &row, QVariantMap *policyMap)
{
    if (!policyMap) {
        return false;
    }
    const QString serialKey = serialIndexPolicyKeyFromRowStorage(row);
    if (serialKey.isEmpty() || policyMap->contains(serialKey)) {
        return false;
    }

    const QStringList candidates = policyKeyCandidatesFromRowStorage(row);
    if (candidates.isEmpty()) {
        return false;
    }

    for (const QString &key : candidates) {
        if (key.startsWith(QStringLiteral("uuid:")) || key.startsWith(QStringLiteral("partuuid:"))) {
            if (policyMap->contains(key)) {
                return false;
            }
            continue;
        }
        if (!key.startsWith(QStringLiteral("device:")) && !key.startsWith(QStringLiteral("group:"))) {
            continue;
        }
        if (!policyMap->contains(key)) {
            continue;
        }
        const QVariantMap legacy = policyMap->value(key).toMap();
        if (legacy.isEmpty()) {
            continue;
        }
        policyMap->insert(serialKey, legacy);
        return true;
    }
    return false;
}

static bool pruneLegacyPolicyKeysEnabledStorage()
{
    static const bool enabled = []() {
        const QByteArray raw = qgetenv("SLM_STORAGE_POLICY_PRUNE_LEGACY_KEYS").trimmed().toLower();
        return raw == "1" || raw == "true" || raw == "yes" || raw == "on";
    }();
    return enabled;
}

static int pruneLegacyPolicyKeysForSerialIndexStorage(const QVariantMap &row, QVariantMap *policyMap)
{
    if (!policyMap || !pruneLegacyPolicyKeysEnabledStorage()) {
        return 0;
    }
    const QString serialKey = serialIndexPolicyKeyFromRowStorage(row);
    if (serialKey.isEmpty() || !policyMap->contains(serialKey)) {
        return 0;
    }
    const QVariantMap serialPolicy = policyMap->value(serialKey).toMap();
    if (serialPolicy.isEmpty()) {
        return 0;
    }

    int removed = 0;
    const QStringList candidates = policyKeyCandidatesFromRowStorage(row);
    for (const QString &key : candidates) {
        if (!key.startsWith(QStringLiteral("device:"))
                && !key.startsWith(QStringLiteral("group:"))) {
            continue;
        }
        if (!policyMap->contains(key)) {
            continue;
        }
        const QVariantMap legacyPolicy = policyMap->value(key).toMap();
        if (legacyPolicy != serialPolicy) {
            continue;
        }
        policyMap->remove(key);
        ++removed;
    }
    return removed;
}

static QVariantMap filteredPolicyPatchStorage(const QVariantMap &patch)
{
    QVariantMap out;
    if (patch.contains(QStringLiteral("action"))) {
        const QString action = patch.value(QStringLiteral("action")).toString().trimmed().toLower();
        if (action == QStringLiteral("mount")
                || action == QStringLiteral("ask")
                || action == QStringLiteral("ignore")) {
            out.insert(QStringLiteral("action"), action);
        }
    }
    auto copyBool = [&out, &patch](const QString &key) {
        if (patch.contains(key)) {
            out.insert(key, patch.value(key).toBool());
        }
    };
    copyBool(QStringLiteral("automount"));
    copyBool(QStringLiteral("auto_open"));
    copyBool(QStringLiteral("visible"));
    copyBool(QStringLiteral("read_only"));
    copyBool(QStringLiteral("exec"));
    return out;
}

static QVariantMap effectivePolicyMapFromRowStorage(const QVariantMap &row)
{
    QVariantMap policy;
    const QString action = row.value(QStringLiteral("policyAction")).toString().trimmed();
    policy.insert(QStringLiteral("action"), action.isEmpty() ? QStringLiteral("mount") : action);
    policy.insert(QStringLiteral("automount"), action == QStringLiteral("mount"));
    policy.insert(QStringLiteral("auto_open"),
                  (action == QStringLiteral("mount"))
                  ? row.value(QStringLiteral("policyAutoOpen")).toBool()
                  : false);
    policy.insert(QStringLiteral("visible"), row.value(QStringLiteral("policyVisible"), true).toBool());
    policy.insert(QStringLiteral("read_only"), row.value(QStringLiteral("policyReadOnly")).toBool());
    policy.insert(QStringLiteral("exec"), row.value(QStringLiteral("policyExec")).toBool());
    return policy;
}

static QVariantMap policyInputMapFromRowStorage(const QVariantMap &row)
{
    return QVariantMap{
        {QStringLiteral("uuid"), row.value(QStringLiteral("uuid")).toString().trimmed()},
        {QStringLiteral("partuuid"), row.value(QStringLiteral("partuuid")).toString().trimmed()},
        {QStringLiteral("fstype"), row.value(QStringLiteral("filesystemType")).toString().trimmed()},
        {QStringLiteral("is_removable"), row.value(QStringLiteral("isRemovable")).toBool()},
        {QStringLiteral("is_system"), row.value(QStringLiteral("isSystem")).toBool()},
        {QStringLiteral("is_encrypted"), row.value(QStringLiteral("isEncrypted")).toBool()},
    };
}

static QVariantMap policyOutputMapFromRowStorage(const QVariantMap &row)
{
    const QVariantMap effective = effectivePolicyMapFromRowStorage(row);
    return QVariantMap{
        {QStringLiteral("action"), effective.value(QStringLiteral("action")).toString()},
        {QStringLiteral("auto_open"), effective.value(QStringLiteral("auto_open")).toBool()},
        {QStringLiteral("visible"), effective.value(QStringLiteral("visible")).toBool()},
        {QStringLiteral("read_only"), effective.value(QStringLiteral("read_only")).toBool()},
        {QStringLiteral("exec"), effective.value(QStringLiteral("exec")).toBool()},
    };
}

static void syncPolicyIoOnRowStorage(QVariantMap *row)
{
    if (!row) {
        return;
    }
    row->insert(QStringLiteral("policyInput"), policyInputMapFromRowStorage(*row));
    row->insert(QStringLiteral("policyOutput"), policyOutputMapFromRowStorage(*row));
}

static void attachDeviceIdentityFieldsStorage(QVariantMap *row)
{
    if (!row) {
        return;
    }
    const QString device = row->value(QStringLiteral("device")).toString().trimmed();
    if (device.isEmpty() || !device.startsWith(QStringLiteral("/dev/"))) {
        return;
    }
    const QString serial = resolveSerialForDeviceStorage(device);
    if (!serial.isEmpty()) {
        row->insert(QStringLiteral("deviceSerial"), serial);
    }
    const int partitionIndex = partitionIndexFromDeviceStorage(device);
    if (partitionIndex > 0) {
        row->insert(QStringLiteral("partitionIndex"), partitionIndex);
    }
}

static void applyPolicyOverrideToRowStorage(QVariantMap *row, const QVariantMap &overridePolicy)
{
    if (!row || overridePolicy.isEmpty()) {
        return;
    }
    bool actionSet = false;
    if (overridePolicy.contains(QStringLiteral("action"))) {
        const QString action = overridePolicy.value(QStringLiteral("action")).toString().trimmed().toLower();
        if (action == QStringLiteral("mount")
                || action == QStringLiteral("ask")
                || action == QStringLiteral("ignore")) {
            row->insert(QStringLiteral("policyAction"), action);
            actionSet = true;
        }
    }
    if (!actionSet && overridePolicy.contains(QStringLiteral("automount"))) {
        const bool automount = overridePolicy.value(QStringLiteral("automount")).toBool();
        row->insert(QStringLiteral("policyAction"), automount ? QStringLiteral("mount")
                                                              : QStringLiteral("ask"));
    }
    if (overridePolicy.contains(QStringLiteral("auto_open"))) {
        row->insert(QStringLiteral("policyAutoOpen"),
                    overridePolicy.value(QStringLiteral("auto_open")).toBool());
    }
    if (overridePolicy.contains(QStringLiteral("visible"))) {
        row->insert(QStringLiteral("policyVisible"),
                    overridePolicy.value(QStringLiteral("visible")).toBool());
    }
    if (overridePolicy.contains(QStringLiteral("read_only"))) {
        row->insert(QStringLiteral("policyReadOnly"),
                    overridePolicy.value(QStringLiteral("read_only")).toBool());
    }
    if (overridePolicy.contains(QStringLiteral("exec"))) {
        row->insert(QStringLiteral("policyExec"),
                    overridePolicy.value(QStringLiteral("exec")).toBool());
    }
    const QString effectiveAction = row->value(QStringLiteral("policyAction")).toString().trimmed().toLower();
    if (effectiveAction != QStringLiteral("mount")) {
        row->insert(QStringLiteral("policyAutoOpen"), false);
    }
}

bool stringEqualsInsensitiveStorage(const QString &a, const QString &b)
{
    return a.compare(b, Qt::CaseInsensitive) == 0;
}

bool volumeMatchesTargetStorage(GVolume *volume, const QString &target)
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
    if (stringEqualsInsensitiveStorage(uuid, t)) {
        return true;
    }
    return false;
}

bool mountMatchesTargetStorage(GMount *mount, const QString &target)
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

QVariantMap mountWithGioStorage(const QString &target)
{
    const QString t = target.trimmed();
    if (t.isEmpty()) {
        return QVariantMap{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("invalid-device")}};
    }

    if (t.contains(QStringLiteral("://"))) {
        GFile *file = g_file_new_for_uri(t.toUtf8().constData());
        if (!file) {
            return QVariantMap{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("invalid-uri")}};
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
            return QVariantMap{{QStringLiteral("ok"), false},
                               {QStringLiteral("error"), state.error.isEmpty() ? QStringLiteral("mount-failed") : state.error}};
        }
        return QVariantMap{{QStringLiteral("ok"), true},
                           {QStringLiteral("path"), state.path.isEmpty() ? t : state.path},
                           {QStringLiteral("mountPath"), state.path.isEmpty() ? t : state.path}};
    }

    GVolumeMonitor *monitor = g_volume_monitor_get();
    if (!monitor) {
        return QVariantMap{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("gio-monitor-unavailable")}};
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
        return QVariantMap{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("volume-not-found")}};
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
        return QVariantMap{{QStringLiteral("ok"), true},
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
        return QVariantMap{{QStringLiteral("ok"), false},
                           {QStringLiteral("error"), state.error.isEmpty() ? QStringLiteral("mount-failed") : state.error}};
    }
    return QVariantMap{{QStringLiteral("ok"), true},
                       {QStringLiteral("path"), state.path.isEmpty() ? t : state.path},
                       {QStringLiteral("mountPath"), state.path.isEmpty() ? t : state.path}};
}

QVariantMap unmountWithGioStorage(const QString &target)
{
    const QString t = target.trimmed();
    if (t.isEmpty()) {
        return QVariantMap{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("invalid-device")}};
    }

    GVolumeMonitor *monitor = g_volume_monitor_get();
    if (!monitor) {
        return QVariantMap{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("gio-monitor-unavailable")}};
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
        return QVariantMap{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("mount-not-found")}};
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
        return QVariantMap{{QStringLiteral("ok"), false},
                           {QStringLiteral("error"), state.error.isEmpty() ? QStringLiteral("eject-failed") : state.error}};
    }
    return QVariantMap{{QStringLiteral("ok"), true}};
}

} // namespace

StorageManager::StorageManager(QObject *parent)
    : QObject(parent)
{
    m_storageChangedDebounceTimer.setSingleShot(true);
    m_storageChangedDebounceTimer.setInterval(4000);
    connect(&m_storageChangedDebounceTimer, &QTimer::timeout, this, [this]() {
        emit storageChanged();
    });
}

StorageManager::~StorageManager()
{
    stop();
}

void StorageManager::start()
{
    if (m_monitor) {
        return;
    }
    GVolumeMonitor *monitor = g_volume_monitor_get();
    if (!monitor) {
        return;
    }
    m_monitor = monitor;
    m_sigVolumeAdded = static_cast<qulonglong>(g_signal_connect(
        monitor, "volume-added", G_CALLBACK(onStorageMonitorSignal), this));
    m_sigVolumeRemoved = static_cast<qulonglong>(g_signal_connect(
        monitor, "volume-removed", G_CALLBACK(onStorageMonitorSignal), this));
    m_sigMountAdded = static_cast<qulonglong>(g_signal_connect(
        monitor, "mount-added", G_CALLBACK(onStorageMonitorSignal), this));
    m_sigMountRemoved = static_cast<qulonglong>(g_signal_connect(
        monitor, "mount-removed", G_CALLBACK(onStorageMonitorSignal), this));
}

void StorageManager::stop()
{
    GVolumeMonitor *monitor = static_cast<GVolumeMonitor *>(m_monitor);
    if (!monitor) {
        return;
    }
    if (m_sigVolumeAdded > 0) {
        g_signal_handler_disconnect(monitor, static_cast<gulong>(m_sigVolumeAdded));
    }
    if (m_sigVolumeRemoved > 0) {
        g_signal_handler_disconnect(monitor, static_cast<gulong>(m_sigVolumeRemoved));
    }
    if (m_sigMountAdded > 0) {
        g_signal_handler_disconnect(monitor, static_cast<gulong>(m_sigMountAdded));
    }
    if (m_sigMountRemoved > 0) {
        g_signal_handler_disconnect(monitor, static_cast<gulong>(m_sigMountRemoved));
    }
    m_sigVolumeAdded = 0;
    m_sigVolumeRemoved = 0;
    m_sigMountAdded = 0;
    m_sigMountRemoved = 0;
    m_monitor = nullptr;
}

bool StorageManager::isRunning() const
{
    return m_monitor != nullptr;
}

void StorageManager::queueStorageChanged()
{
    m_storageChangedDebounceTimer.start();
}

QVariantList StorageManager::queryStorageLocationsSync(int lsblkTimeoutMs) const
{
    Q_UNUSED(lsblkTimeoutMs)
    QVariantList out;
    QSet<QString> seenKeys;
    QSet<QString> seenMountPaths;
    QVariantMap policyMap = StoragePolicyStore::loadPolicyMap();
    bool policyMapDirty = false;
    int policyPromotionCount = 0;
    int policyLegacyPruneCount = 0;
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

        QString uuid;
        QString partuuid;
        bool removable = !pathOrUri.startsWith(QStringLiteral("/"));
        GVolume *volume = g_mount_get_volume(mount);
        if (volume) {
            uuid = bestUuidForVolumeStorage(volume);
            const QString unixDevice = volumeIdStorage(volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
            if (!unixDevice.isEmpty()) {
                partuuid = resolvePartUuidForDeviceStorage(unixDevice);
                if (device.trimmed().isEmpty()) {
                    device = unixDevice;
                }
            }
            removable = isRemovableVolumeStorage(volume, pathOrUri);
            g_object_unref(volume);
        } else if (device.startsWith(QStringLiteral("/dev/"))) {
            partuuid = resolvePartUuidForDeviceStorage(device);
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
        const QString fsType = pathOrUri.startsWith(QStringLiteral("/"))
                ? QString::fromUtf8(QStorageInfo(pathOrUri).fileSystemType())
                : QString();
        const bool isSystem = looksSystemPartitionStorage(pathOrUri, name, fsType);
        const bool isEncrypted = stringContainsInsensitiveStorage(fsType, QStringLiteral("luks"));
        const StoragePolicyDecision policy = decideStoragePolicy(
                    uuid, partuuid, fsType, removable, isSystem, isEncrypted);
        if (!policy.visible) {
            continue;
        }
        row.insert(QStringLiteral("name"), name);
        row.insert(QStringLiteral("label"), name);
        row.insert(QStringLiteral("path"), pathOrUri);
        row.insert(QStringLiteral("rootPath"), pathOrUri);
        row.insert(QStringLiteral("device"), device);
        attachDeviceIdentityFieldsStorage(&row);
        const QString groupKey = deviceGroupKeyStorage(device, pathOrUri, name);
        row.insert(QStringLiteral("deviceGroupKey"), groupKey);
        row.insert(QStringLiteral("deviceGroupLabel"), deviceGroupLabelStorage(name, groupKey));
        row.insert(QStringLiteral("uuid"), uuid);
        row.insert(QStringLiteral("partuuid"), partuuid);
        row.insert(QStringLiteral("isRemovable"), removable);
        row.insert(QStringLiteral("isSystem"), policy.isSystem);
        row.insert(QStringLiteral("isEncrypted"), isEncrypted);
        row.insert(QStringLiteral("policyAction"), policy.action);
        row.insert(QStringLiteral("policyAutoOpen"), policy.autoOpen);
        row.insert(QStringLiteral("policyVisible"), policy.visible);
        row.insert(QStringLiteral("policyReadOnly"), policy.readOnly);
        row.insert(QStringLiteral("policyExec"), policy.exec);
        if (promoteLegacyPolicyKeyToSerialIndexStorage(row, &policyMap)) {
            policyMapDirty = true;
            ++policyPromotionCount;
        }
        const int prunedLegacyMount = pruneLegacyPolicyKeysForSerialIndexStorage(row, &policyMap);
        if (prunedLegacyMount > 0) {
            policyMapDirty = true;
            policyLegacyPruneCount += prunedLegacyMount;
        }
        const QString policyKey = policyKeyFromRowStorage(row, policyMap);
        if (!policyKey.isEmpty()) {
            row.insert(QStringLiteral("policyKey"), policyKey);
            row.insert(QStringLiteral("policyScope"), policyScopeFromPolicyKeyStorage(policyKey));
            applyPolicyOverrideToRowStorage(&row, policyMap.value(policyKey).toMap());
            if (!row.value(QStringLiteral("policyVisible"), true).toBool()) {
                continue;
            }
        } else {
            row.insert(QStringLiteral("policyScope"), QStringLiteral("partition"));
        }
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
        syncPolicyIoOnRowStorage(&row);
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
        const QString fsType;
        const bool isSystem = looksSystemPartitionStorage(pathOrUri, name, fsType);
        const bool isEncrypted = false;
        const bool removable = true;
        const QString uuid = bestUuidForVolumeStorage(volume);
        const QString unixDevice = volumeIdStorage(volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
        const QString partuuid = resolvePartUuidForDeviceStorage(unixDevice);
        const StoragePolicyDecision policy = decideStoragePolicy(
                    uuid, partuuid, fsType, removable, isSystem, isEncrypted);
        if (!policy.visible) {
            continue;
        }
        row.insert(QStringLiteral("name"), name);
        row.insert(QStringLiteral("label"), name);
        row.insert(QStringLiteral("path"), pathOrUri);
        row.insert(QStringLiteral("rootPath"), pathOrUri);
        row.insert(QStringLiteral("device"), device);
        attachDeviceIdentityFieldsStorage(&row);
        const QString groupKey = deviceGroupKeyStorage(device, pathOrUri, name);
        row.insert(QStringLiteral("deviceGroupKey"), groupKey);
        row.insert(QStringLiteral("deviceGroupLabel"), deviceGroupLabelStorage(name, groupKey));
        row.insert(QStringLiteral("uuid"), uuid);
        row.insert(QStringLiteral("partuuid"), partuuid);
        row.insert(QStringLiteral("isRemovable"), removable);
        row.insert(QStringLiteral("isSystem"), policy.isSystem);
        row.insert(QStringLiteral("isEncrypted"), isEncrypted);
        row.insert(QStringLiteral("policyAction"), policy.action);
        row.insert(QStringLiteral("policyAutoOpen"), policy.autoOpen);
        row.insert(QStringLiteral("policyVisible"), policy.visible);
        row.insert(QStringLiteral("policyReadOnly"), policy.readOnly);
        row.insert(QStringLiteral("policyExec"), policy.exec);
        if (promoteLegacyPolicyKeyToSerialIndexStorage(row, &policyMap)) {
            policyMapDirty = true;
            ++policyPromotionCount;
        }
        const int prunedLegacyVolume = pruneLegacyPolicyKeysForSerialIndexStorage(row, &policyMap);
        if (prunedLegacyVolume > 0) {
            policyMapDirty = true;
            policyLegacyPruneCount += prunedLegacyVolume;
        }
        const QString policyKey = policyKeyFromRowStorage(row, policyMap);
        if (!policyKey.isEmpty()) {
            row.insert(QStringLiteral("policyKey"), policyKey);
            row.insert(QStringLiteral("policyScope"), policyScopeFromPolicyKeyStorage(policyKey));
            applyPolicyOverrideToRowStorage(&row, policyMap.value(policyKey).toMap());
            if (!row.value(QStringLiteral("policyVisible"), true).toBool()) {
                continue;
            }
        } else {
            row.insert(QStringLiteral("policyScope"), QStringLiteral("partition"));
        }
        row.insert(QStringLiteral("mounted"), false);
        row.insert(QStringLiteral("browsable"), !pathOrUri.isEmpty());
        row.insert(QStringLiteral("isRoot"), false);
        row.insert(QStringLiteral("icon"), iconName.isEmpty() ? QStringLiteral("drive-harddisk-symbolic")
                                                               : iconName);
        row.insert(QStringLiteral("bytesTotal"), static_cast<qlonglong>(-1));
        row.insert(QStringLiteral("bytesAvailable"), static_cast<qlonglong>(-1));
        row.insert(QStringLiteral("bytesFree"), static_cast<qlonglong>(-1));
        row.insert(QStringLiteral("filesystemType"), QString());
        syncPolicyIoOnRowStorage(&row);
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
        const QString fsType = QString::fromUtf8(v.fileSystemType());
        const bool isSystem = looksSystemPartitionStorage(rootPath, name, fsType);
        const bool isEncrypted = stringContainsInsensitiveStorage(fsType, QStringLiteral("luks"));
        const bool removable = rootPath.startsWith(QStringLiteral("/media/"))
                || rootPath.startsWith(QStringLiteral("/run/media/"));
        const QString partuuid = resolvePartUuidForDeviceStorage(device);
        const StoragePolicyDecision policy = decideStoragePolicy(
                    QString(), partuuid, fsType, removable, isSystem, isEncrypted);
        if (!policy.visible) {
            continue;
        }
        row.insert(QStringLiteral("name"), name);
        row.insert(QStringLiteral("label"), name);
        row.insert(QStringLiteral("path"), rootPath);
        row.insert(QStringLiteral("rootPath"), rootPath);
        row.insert(QStringLiteral("device"), device);
        attachDeviceIdentityFieldsStorage(&row);
        const QString groupKey = deviceGroupKeyStorage(device, rootPath, name);
        row.insert(QStringLiteral("deviceGroupKey"), groupKey);
        row.insert(QStringLiteral("deviceGroupLabel"), deviceGroupLabelStorage(name, groupKey));
        row.insert(QStringLiteral("uuid"), QString());
        row.insert(QStringLiteral("partuuid"), partuuid);
        row.insert(QStringLiteral("isRemovable"), removable);
        row.insert(QStringLiteral("isSystem"), policy.isSystem);
        row.insert(QStringLiteral("isEncrypted"), isEncrypted);
        row.insert(QStringLiteral("policyAction"), policy.action);
        row.insert(QStringLiteral("policyAutoOpen"), policy.autoOpen);
        row.insert(QStringLiteral("policyVisible"), policy.visible);
        row.insert(QStringLiteral("policyReadOnly"), policy.readOnly);
        row.insert(QStringLiteral("policyExec"), policy.exec);
        if (promoteLegacyPolicyKeyToSerialIndexStorage(row, &policyMap)) {
            policyMapDirty = true;
            ++policyPromotionCount;
        }
        const int prunedLegacyStorageInfo = pruneLegacyPolicyKeysForSerialIndexStorage(row, &policyMap);
        if (prunedLegacyStorageInfo > 0) {
            policyMapDirty = true;
            policyLegacyPruneCount += prunedLegacyStorageInfo;
        }
        const QString policyKey = policyKeyFromRowStorage(row, policyMap);
        if (!policyKey.isEmpty()) {
            row.insert(QStringLiteral("policyKey"), policyKey);
            row.insert(QStringLiteral("policyScope"), policyScopeFromPolicyKeyStorage(policyKey));
            applyPolicyOverrideToRowStorage(&row, policyMap.value(policyKey).toMap());
            if (!row.value(QStringLiteral("policyVisible"), true).toBool()) {
                continue;
            }
        } else {
            row.insert(QStringLiteral("policyScope"), QStringLiteral("partition"));
        }
        row.insert(QStringLiteral("mounted"), true);
        row.insert(QStringLiteral("browsable"), true);
        row.insert(QStringLiteral("isRoot"), rootPath == QStringLiteral("/"));
        row.insert(QStringLiteral("icon"), QStringLiteral("drive-harddisk-symbolic"));
        row.insert(QStringLiteral("bytesTotal"), static_cast<qlonglong>(v.bytesTotal()));
        row.insert(QStringLiteral("bytesAvailable"), static_cast<qlonglong>(v.bytesAvailable()));
        row.insert(QStringLiteral("bytesFree"), static_cast<qlonglong>(v.bytesFree()));
        row.insert(QStringLiteral("filesystemType"), QString::fromUtf8(v.fileSystemType()));
        syncPolicyIoOnRowStorage(&row);
        out.push_back(row);
    }

    if (policyMapDirty) {
        QString saveErr;
        if (!StoragePolicyStore::savePolicyMap(policyMap, &saveErr)) {
            qWarning() << "storage policy serial-index promotion save failed:" << saveErr;
        } else {
            if (policyPromotionCount > 0) {
                qInfo() << "storage policy serial-index promotion entries:" << policyPromotionCount;
            }
            if (policyLegacyPruneCount > 0) {
                qInfo() << "storage policy legacy key prune entries:" << policyLegacyPruneCount;
            }
        }
    }

    return out;
}

QVariantMap StorageManager::mountStorageDevice(const QString &devicePath) const
{
    const QString dev = devicePath.trimmed();
    if (dev.isEmpty()) {
        return QVariantMap{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("invalid-device")}
        };
    }

    QVariantMap devicesReply;
    QString daemonError;
    if (callDevicesServiceStorage(QStringLiteral("Mount"), {dev}, &devicesReply, &daemonError)) {
        const bool ok = devicesReply.value(QStringLiteral("ok")).toBool();
        const QString error = devicesReply.value(QStringLiteral("error")).toString().trimmed();
        const QString mountedPath = devicesReply.value(QStringLiteral("mountPath")).toString().trimmed();
        if (!ok) {
            return QVariantMap{
                {QStringLiteral("ok"), false},
                {QStringLiteral("error"), error.isEmpty() ? QStringLiteral("mount-failed") : error}
            };
        }
        return QVariantMap{
            {QStringLiteral("ok"), true},
            {QStringLiteral("device"), dev},
            {QStringLiteral("path"), mountedPath.isEmpty() ? dev : mountedPath}
        };
    }

    const QVariantMap gioResult = mountWithGioStorage(dev);
    if (!gioResult.value(QStringLiteral("ok")).toBool()) {
        const QString err = gioResult.value(QStringLiteral("error")).toString().trimmed();
        return QVariantMap{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), err.isEmpty() ? (daemonError.isEmpty() ? QStringLiteral("mount-failed")
                                                                              : daemonError)
                                                    : err}
        };
    }
    return QVariantMap{
        {QStringLiteral("ok"), true},
        {QStringLiteral("device"), dev},
        {QStringLiteral("path"), gioResult.value(QStringLiteral("path")).toString().trimmed()}
    };
}

QVariantMap StorageManager::unmountStorageDevice(const QString &devicePath) const
{
    const QString dev = devicePath.trimmed();
    if (dev.isEmpty()) {
        return QVariantMap{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("invalid-device")}
        };
    }

    QVariantMap devicesReply;
    QString daemonError;
    if (callDevicesServiceStorage(QStringLiteral("Eject"), {dev}, &devicesReply, &daemonError)) {
        const bool ok = devicesReply.value(QStringLiteral("ok")).toBool();
        const QString error = devicesReply.value(QStringLiteral("error")).toString().trimmed();
        if (!ok) {
            return QVariantMap{
                {QStringLiteral("ok"), false},
                {QStringLiteral("error"), error.isEmpty() ? QStringLiteral("eject-failed") : error}
            };
        }
        return QVariantMap{
            {QStringLiteral("ok"), true},
            {QStringLiteral("device"), dev}
        };
    }

    const QVariantMap gioResult = unmountWithGioStorage(dev);
    if (!gioResult.value(QStringLiteral("ok")).toBool()) {
        const QString err = gioResult.value(QStringLiteral("error")).toString().trimmed();
        return QVariantMap{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), err.isEmpty() ? (daemonError.isEmpty() ? QStringLiteral("eject-failed")
                                                                              : daemonError)
                                                    : err}
        };
    }
    return QVariantMap{
        {QStringLiteral("ok"), true},
        {QStringLiteral("device"), dev}
    };
}

QVariantMap StorageManager::storagePolicyForPath(const QString &path) const
{
    const QString target = path.trimmed();
    if (target.isEmpty()) {
        return QVariantMap{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("invalid-path")}
        };
    }

    const QString normalizedTarget = normalizedPathForPolicyStorage(target);
    const QVariantList rows = queryStorageLocationsSync();
    QVariantMap matchedRow;
    for (const QVariant &entry : rows) {
        const QVariantMap row = entry.toMap();
        if (row.isEmpty()) {
            continue;
        }

        const QString rowPath = row.value(QStringLiteral("path")).toString().trimmed();
        const QString rowRoot = row.value(QStringLiteral("rootPath")).toString().trimmed();
        const QString rowDevice = row.value(QStringLiteral("device")).toString().trimmed();
        const QString rowUuid = row.value(QStringLiteral("uuid")).toString().trimmed();
        const QString rowPartUuid = row.value(QStringLiteral("partuuid")).toString().trimmed();

        const bool pathMatch = !normalizedTarget.isEmpty() && (
                    normalizedPathForPolicyStorage(rowPath).compare(normalizedTarget, Qt::CaseInsensitive) == 0
                    || normalizedPathForPolicyStorage(rowRoot).compare(normalizedTarget, Qt::CaseInsensitive) == 0);
        const bool idMatch = rowDevice.compare(target, Qt::CaseInsensitive) == 0
                || rowUuid.compare(target, Qt::CaseInsensitive) == 0
                || rowPartUuid.compare(target, Qt::CaseInsensitive) == 0;
        if (pathMatch || idMatch) {
            matchedRow = row;
            break;
        }
    }

    if (matchedRow.isEmpty()) {
        return QVariantMap{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("path-not-found")}
        };
    }

    const QVariantMap policyMap = StoragePolicyStore::loadPolicyMap();
    const QString policyKey = policyKeyFromRowStorage(matchedRow, policyMap);
    const QString scope = policyKey.isEmpty()
            ? QStringLiteral("partition")
            : policyScopeFromPolicyKeyStorage(policyKey);

    if (!policyKey.isEmpty()) {
        applyPolicyOverrideToRowStorage(&matchedRow, policyMap.value(policyKey).toMap());
    }
    syncPolicyIoOnRowStorage(&matchedRow);

    QVariantMap out{
        {QStringLiteral("ok"), true},
        {QStringLiteral("path"), matchedRow.value(QStringLiteral("path")).toString()},
        {QStringLiteral("rootPath"), matchedRow.value(QStringLiteral("rootPath")).toString()},
        {QStringLiteral("device"), matchedRow.value(QStringLiteral("device")).toString()},
        {QStringLiteral("policyKey"), policyKey},
        {QStringLiteral("policyScope"), scope},
        {QStringLiteral("policy"), effectivePolicyMapFromRowStorage(matchedRow)},
        {QStringLiteral("policyInput"), matchedRow.value(QStringLiteral("policyInput")).toMap()},
        {QStringLiteral("policyOutput"), matchedRow.value(QStringLiteral("policyOutput")).toMap()},
    };
    if (matchedRow.contains(QStringLiteral("deviceGroupKey"))) {
        out.insert(QStringLiteral("deviceGroupKey"), matchedRow.value(QStringLiteral("deviceGroupKey")));
    }
    if (matchedRow.contains(QStringLiteral("uuid"))) {
        out.insert(QStringLiteral("uuid"), matchedRow.value(QStringLiteral("uuid")));
    }
    if (matchedRow.contains(QStringLiteral("partuuid"))) {
        out.insert(QStringLiteral("partuuid"), matchedRow.value(QStringLiteral("partuuid")));
    }
    if (matchedRow.contains(QStringLiteral("deviceSerial"))) {
        out.insert(QStringLiteral("deviceSerial"), matchedRow.value(QStringLiteral("deviceSerial")));
    }
    if (matchedRow.contains(QStringLiteral("partitionIndex"))) {
        out.insert(QStringLiteral("partitionIndex"), matchedRow.value(QStringLiteral("partitionIndex")));
    }
    return out;
}

QVariantMap StorageManager::setStoragePolicyForPath(const QString &path,
                                                    const QVariantMap &policyPatch,
                                                    const QString &scope) const
{
    const QString target = path.trimmed();
    if (target.isEmpty()) {
        return QVariantMap{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("invalid-path")}
        };
    }

    const QVariantMap filteredPatch = filteredPolicyPatchStorage(policyPatch);
    if (filteredPatch.isEmpty()) {
        return QVariantMap{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("invalid-policy")}
        };
    }

    const QString normalizedTarget = normalizedPathForPolicyStorage(target);
    const QVariantList rows = queryStorageLocationsSync();
    QVariantMap matchedRow;
    for (const QVariant &entry : rows) {
        const QVariantMap row = entry.toMap();
        if (row.isEmpty()) {
            continue;
        }
        const QString rowPath = row.value(QStringLiteral("path")).toString().trimmed();
        const QString rowRoot = row.value(QStringLiteral("rootPath")).toString().trimmed();
        const QString rowDevice = row.value(QStringLiteral("device")).toString().trimmed();
        const QString rowUuid = row.value(QStringLiteral("uuid")).toString().trimmed();
        const QString rowPartUuid = row.value(QStringLiteral("partuuid")).toString().trimmed();

        const bool pathMatch = !normalizedTarget.isEmpty() && (
                    normalizedPathForPolicyStorage(rowPath).compare(normalizedTarget, Qt::CaseInsensitive) == 0
                    || normalizedPathForPolicyStorage(rowRoot).compare(normalizedTarget, Qt::CaseInsensitive) == 0);
        const bool idMatch = rowDevice.compare(target, Qt::CaseInsensitive) == 0
                || rowUuid.compare(target, Qt::CaseInsensitive) == 0
                || rowPartUuid.compare(target, Qt::CaseInsensitive) == 0;
        if (pathMatch || idMatch) {
            matchedRow = row;
            break;
        }
    }

    if (matchedRow.isEmpty()) {
        return QVariantMap{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("path-not-found")}
        };
    }

    const QString normalizedScope = scope.trimmed().toLower();
    const QString effectiveScope = normalizedScope.isEmpty()
            ? matchedRow.value(QStringLiteral("policyScope"), QStringLiteral("partition")).toString()
            : normalizedScope;
    const QString key = choosePolicyKeyForScopeStorage(matchedRow, effectiveScope);
    if (key.isEmpty()) {
        return QVariantMap{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("policy-key-unresolved")}
        };
    }

    QVariantMap policyMap = StoragePolicyStore::loadPolicyMap();
    QVariantMap merged = policyMap.value(key).toMap();
    for (auto it = filteredPatch.constBegin(); it != filteredPatch.constEnd(); ++it) {
        merged.insert(it.key(), it.value());
    }
    policyMap.insert(key, merged);

    QString saveError;
    if (!StoragePolicyStore::savePolicyMap(policyMap, &saveError)) {
        return QVariantMap{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), saveError.isEmpty() ? QStringLiteral("policy-save-failed") : saveError}
        };
    }

    const_cast<StorageManager *>(this)->queueStorageChanged();
    QVariantMap out = storagePolicyForPath(target);
    if (!out.value(QStringLiteral("ok")).toBool()) {
        return QVariantMap{
            {QStringLiteral("ok"), true},
            {QStringLiteral("path"), target},
            {QStringLiteral("policyKey"), key},
            {QStringLiteral("policyScope"), policyScopeFromPolicyKeyStorage(key)},
            {QStringLiteral("policy"), merged}
        };
    }
    out.insert(QStringLiteral("policyKey"), key);
    out.insert(QStringLiteral("policyScope"), policyScopeFromPolicyKeyStorage(key));
    return out;
}

QVariantMap StorageManager::connectServer(const QString &serverUri) const
{
    QString uri = serverUri.trimmed();
    if (uri.isEmpty()) {
        return QVariantMap{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("invalid-uri")}
        };
    }
    if (!uri.contains(QStringLiteral("://"))) {
        uri.prepend(QStringLiteral("smb://"));
    }
    return mountStorageDevice(uri);
}

} // namespace Slm::Storage
