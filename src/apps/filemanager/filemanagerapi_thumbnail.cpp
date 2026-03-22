#include "filemanagerapi.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QThread>
#include <QUrl>

#pragma push_macro("signals")
#undef signals
#include <gio/gio.h>
#pragma pop_macro("signals")

#include <thread>

namespace {

static QString thumbnailRequestKey(const QString &path, int size)
{
    const QString p = QFileInfo(path).absoluteFilePath();
    const int s = qBound(64, size, 1024);
    return p + QStringLiteral("|") + QString::number(s);
}

static QString contentTypeForThumbnailPath(const QString &path)
{
    const QByteArray local = QFile::encodeName(path);
    GFile *file = g_file_new_for_path(local.constData());
    GError *gerr = nullptr;
    GFileInfo *info = g_file_query_info(file,
                                        G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                        G_FILE_QUERY_INFO_NONE,
                                        nullptr,
                                        &gerr);
    QString contentType;
    if (info != nullptr) {
        const char *ct = g_file_info_get_content_type(info);
        if (ct != nullptr) {
            contentType = QString::fromUtf8(ct).trimmed();
        }
        g_object_unref(info);
    }
    if (gerr != nullptr) {
        g_error_free(gerr);
    }
    g_object_unref(file);
    return contentType;
}

static QString gioThumbnailPathForFile(const QString &path, bool *failed)
{
    if (failed) {
        *failed = false;
    }
    const QByteArray local = QFile::encodeName(path);
    GFile *file = g_file_new_for_path(local.constData());
    GError *gerr = nullptr;
    GFileInfo *info = g_file_query_info(file,
                                        G_FILE_ATTRIBUTE_THUMBNAIL_PATH ","
                                        G_FILE_ATTRIBUTE_THUMBNAIL_IS_VALID ","
                                        G_FILE_ATTRIBUTE_THUMBNAILING_FAILED,
                                        G_FILE_QUERY_INFO_NONE,
                                        nullptr,
                                        &gerr);
    QString thumbPath;
    if (info != nullptr) {
        const bool isValid = g_file_info_get_attribute_boolean(info, G_FILE_ATTRIBUTE_THUMBNAIL_IS_VALID);
        const bool isFailed = g_file_info_get_attribute_boolean(info, G_FILE_ATTRIBUTE_THUMBNAILING_FAILED);
        const char *thumb = g_file_info_get_attribute_byte_string(info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);
        if (failed) {
            *failed = isFailed;
        }
        if (isValid && thumb != nullptr) {
            const QString candidate = QFileInfo(QString::fromUtf8(thumb)).absoluteFilePath();
            if (QFileInfo::exists(candidate)) {
                thumbPath = candidate;
            }
        }
        g_object_unref(info);
    }
    if (gerr != nullptr) {
        g_error_free(gerr);
    }
    g_object_unref(file);
    return thumbPath;
}

static QString freedesktopThumbnailFlavor(int size)
{
    return (size > 128) ? QStringLiteral("large") : QStringLiteral("normal");
}

static QString freedesktopThumbnailCachePathForFile(const QString &path, int size)
{
    const QString absolute = QFileInfo(path).absoluteFilePath();
    if (absolute.isEmpty()) {
        return QString();
    }
    const QString uri = QUrl::fromLocalFile(absolute).toString(QUrl::FullyEncoded);
    if (uri.isEmpty()) {
        return QString();
    }
    const QByteArray digest = QCryptographicHash::hash(uri.toUtf8(), QCryptographicHash::Md5).toHex();
    if (digest.isEmpty()) {
        return QString();
    }
    const QString flavor = freedesktopThumbnailFlavor(size);
    const QString genericCache = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    const QString thumbnailsBase = !genericCache.isEmpty()
            ? QDir(genericCache).absoluteFilePath(QStringLiteral("thumbnails"))
            : QDir::home().absoluteFilePath(QStringLiteral(".cache/thumbnails"));
    return QDir(thumbnailsBase).absoluteFilePath(flavor + QStringLiteral("/") + QString::fromLatin1(digest) + QStringLiteral(".png"));
}

static QString lookupFreedesktopThumbnailCache(const QString &path, int size)
{
    const QString candidate = freedesktopThumbnailCachePathForFile(path, size);
    if (!candidate.isEmpty() && QFileInfo::exists(candidate)) {
        return QFileInfo(candidate).absoluteFilePath();
    }
    return QString();
}

static bool queueFreedesktopThumbnailerRequest(const QString &path, int size, QString *error)
{
    if (error) {
        error->clear();
    }
    const QString absolute = QFileInfo(path).absoluteFilePath();
    if (absolute.isEmpty() || !QFileInfo::exists(absolute)) {
        if (error) {
            *error = QStringLiteral("invalid-path");
        }
        return false;
    }

    const QString uri = QUrl::fromLocalFile(absolute).toString(QUrl::FullyEncoded);
    const QString mime = contentTypeForThumbnailPath(absolute);
    if (mime.isEmpty()) {
        if (error) {
            *error = QStringLiteral("mime-unknown");
        }
        return false;
    }

    const QByteArray uriUtf8 = uri.toUtf8();
    const QByteArray mimeUtf8 = mime.toUtf8();
    const gchar *uriItems[] = { uriUtf8.constData(), nullptr };
    const gchar *mimeItems[] = { mimeUtf8.constData(), nullptr };

    GError *gerr = nullptr;
    GDBusConnection *conn = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &gerr);
    if (conn == nullptr) {
        if (error) {
            *error = gerr ? QString::fromUtf8(gerr->message) : QStringLiteral("dbus-unavailable");
        }
        if (gerr) {
            g_error_free(gerr);
        }
        return false;
    }

    const char *flavor = (size > 128) ? "large" : "normal";
    GVariant *params = g_variant_new("(^as^asssu)",
                                     uriItems,
                                     mimeItems,
                                     flavor,
                                     "foreground",
                                     static_cast<guint32>(0));
    GVariant *reply = g_dbus_connection_call_sync(conn,
                                                  "org.freedesktop.thumbnails.Thumbnailer1",
                                                  "/org/freedesktop/thumbnails/Thumbnailer1",
                                                  "org.freedesktop.thumbnails.Thumbnailer1",
                                                  "Queue",
                                                  params,
                                                  G_VARIANT_TYPE("(u)"),
                                                  G_DBUS_CALL_FLAGS_NONE,
                                                  4000,
                                                  nullptr,
                                                  &gerr);
    bool ok = false;
    if (reply != nullptr) {
        guint32 handle = 0;
        g_variant_get(reply, "(u)", &handle);
        ok = (handle > 0);
        g_variant_unref(reply);
    }
    if (!ok && error) {
        *error = gerr ? QString::fromUtf8(gerr->message) : QStringLiteral("thumbnailer-queue-failed");
    }
    if (gerr) {
        g_error_free(gerr);
    }
    g_object_unref(conn);
    return ok;
}

} // namespace

QVariantMap FileManagerApi::requestThumbnailAsync(const QString &path, int size)
{
    const QString p = expandPath(path);
    const int requestedSize = qBound(64, size, 1024);
    QFileInfo sourceInfo(p);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        return makeResult(false, QStringLiteral("invalid-path"));
    }
    const QString sourcePath = sourceInfo.absoluteFilePath();
    const QString key = thumbnailRequestKey(sourcePath, requestedSize);
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    {
        QMutexLocker locker(&m_thumbnailCacheMutex);
        const qint64 failUntilMs = m_thumbnailMemoFailUntilMs.value(key, 0);
        if (failUntilMs <= nowMs) {
            const QString memoPath = m_thumbnailMemoPath.value(key);
            const qint64 memoExpiryMs = m_thumbnailMemoExpiryMs.value(key, 0);
            if (!memoPath.isEmpty() && memoExpiryMs > nowMs && QFileInfo::exists(memoPath)) {
                emit thumbnailReady(sourcePath, requestedSize, memoPath, true, QString());
                return makeResult(true, QString(),
                                  {{QStringLiteral("path"), memoPath},
                                   {QStringLiteral("queued"), false}});
            }
        }
    }

    {
        QMutexLocker locker(&m_thumbnailCacheMutex);
        if (m_thumbnailPending.contains(key)) {
            return makeResult(true, QString(),
                              {{QStringLiteral("path"), sourcePath},
                               {QStringLiteral("queued"), true}});
        }
        m_thumbnailPending.insert(key);
    }

    std::thread([this, sourcePath, requestedSize, key]() {
        const QString generated = ensureThumbnail(sourcePath, requestedSize);
        const bool ok = !generated.isEmpty();
        const QString error = ok ? QString() : QStringLiteral("thumbnail-unavailable");
        QMetaObject::invokeMethod(this, [this, sourcePath, requestedSize, key, generated, ok, error]() {
            {
                QMutexLocker locker(&m_thumbnailCacheMutex);
                m_thumbnailPending.remove(key);
            }
            emit thumbnailReady(sourcePath, requestedSize, generated, ok, error);
        }, Qt::QueuedConnection);
    }).detach();

    return makeResult(true, QString(),
                      {{QStringLiteral("path"), sourcePath},
                       {QStringLiteral("queued"), true}});
}

QString FileManagerApi::ensureThumbnail(const QString &path, int size) const
{
    const QString sourcePath = expandPath(path);
    QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        return QString();
    }

    const int thumbSize = qBound(64, size, 1024);
    const QString requestKey = thumbnailRequestKey(sourcePath, thumbSize);
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    {
        QMutexLocker locker(&m_thumbnailCacheMutex);
        const qint64 failUntilMs = m_thumbnailMemoFailUntilMs.value(requestKey, 0);
        if (failUntilMs > nowMs) {
            return QString();
        }

        const QString memoPath = m_thumbnailMemoPath.value(requestKey);
        const qint64 memoExpiryMs = m_thumbnailMemoExpiryMs.value(requestKey, 0);
        if (!memoPath.isEmpty() && memoExpiryMs > nowMs && QFileInfo::exists(memoPath)) {
            return memoPath;
        }
    }

    bool gioFailed = false;
    const QString gioPath = gioThumbnailPathForFile(sourcePath, &gioFailed);
    if (!gioPath.isEmpty()) {
        QMutexLocker locker(&m_thumbnailCacheMutex);
        m_thumbnailMemoPath.insert(requestKey, gioPath);
        m_thumbnailMemoExpiryMs.insert(requestKey, nowMs + 120000);
        m_thumbnailMemoFailUntilMs.remove(requestKey);
        return gioPath;
    }
    const QString cachedPath = lookupFreedesktopThumbnailCache(sourcePath, thumbSize);
    if (!cachedPath.isEmpty()) {
        QMutexLocker locker(&m_thumbnailCacheMutex);
        m_thumbnailMemoPath.insert(requestKey, cachedPath);
        m_thumbnailMemoExpiryMs.insert(requestKey, nowMs + 120000);
        m_thumbnailMemoFailUntilMs.remove(requestKey);
        return cachedPath;
    }

    if (!gioFailed) {
        QString gioError;
        if (queueFreedesktopThumbnailerRequest(sourcePath, thumbSize, &gioError)) {
            QThread::msleep(80);
            for (int attempt = 0; attempt < 5; ++attempt) {
                bool failedNow = false;
                const QString queuedPath = gioThumbnailPathForFile(sourcePath, &failedNow);
                if (!queuedPath.isEmpty()) {
                    QMutexLocker locker(&m_thumbnailCacheMutex);
                    m_thumbnailMemoPath.insert(requestKey, queuedPath);
                    m_thumbnailMemoExpiryMs.insert(requestKey, nowMs + 120000);
                    m_thumbnailMemoFailUntilMs.remove(requestKey);
                    return queuedPath;
                }
                const QString queuedCachePath = lookupFreedesktopThumbnailCache(sourcePath, thumbSize);
                if (!queuedCachePath.isEmpty()) {
                    QMutexLocker locker(&m_thumbnailCacheMutex);
                    m_thumbnailMemoPath.insert(requestKey, queuedCachePath);
                    m_thumbnailMemoExpiryMs.insert(requestKey, nowMs + 120000);
                    m_thumbnailMemoFailUntilMs.remove(requestKey);
                    return queuedCachePath;
                }
                if (failedNow) {
                    break;
                }
                QThread::msleep(120);
            }
        }
    }

    {
        QMutexLocker locker(&m_thumbnailCacheMutex);
        m_thumbnailMemoFailUntilMs.insert(requestKey, nowMs + 10000);
        m_thumbnailMemoPath.remove(requestKey);
        m_thumbnailMemoExpiryMs.remove(requestKey);
    }
    return QString();
}

QString FileManagerApi::ensureVideoThumbnail(const QString &path, int size) const
{
    return ensureThumbnail(path, size);
}

bool FileManagerApi::queueFreedesktopThumbnailer(const QString &path, int size)
{
    QString error;
    return queueFreedesktopThumbnailerRequest(expandPath(path), size, &error);
}

void FileManagerApi::onFreedesktopThumbnailReady(uint handle, const QStringList &uris)
{
    Q_UNUSED(handle)
    Q_UNUSED(uris)
}

void FileManagerApi::onFreedesktopThumbnailError(uint handle, const QStringList &uris, int code, const QString &message)
{
    Q_UNUSED(handle)
    Q_UNUSED(uris)
    Q_UNUSED(code)
    Q_UNUSED(message)
}

void FileManagerApi::onFreedesktopThumbnailFinished(uint handle)
{
    Q_UNUSED(handle)
}
