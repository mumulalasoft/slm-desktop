#include "cleanerscanner.h"

#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>

#ifdef signals
#undef signals
#endif
#include <gio/gio.h>

namespace Slm::Cleaner {
namespace {

struct Aggregate {
    quint64 sizeBytes = 0;
    quint64 fileCount = 0;
    qint64 lastAccessEpoch = 0;
};

void updateLastAccess(Aggregate &agg, qint64 epoch)
{
    if (epoch > agg.lastAccessEpoch) {
        agg.lastAccessEpoch = epoch;
    }
}

void scanRecursive(GFile *file, Aggregate &agg, GError **error)
{
    GFileEnumerator *enumerator = g_file_enumerate_children(
                file,
                "standard::name,standard::type,standard::size,time::access,time::modified",
                G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                nullptr,
                error);
    if (!enumerator) {
        return;
    }

    while (true) {
        GFileInfo *info = g_file_enumerator_next_file(enumerator, nullptr, error);
        if (!info) {
            break;
        }

        const GFileType type = g_file_info_get_file_type(info);
        const goffset fileSize = g_file_info_get_size(info);
        const gint64 atime = g_file_info_get_attribute_uint64(info, G_FILE_ATTRIBUTE_TIME_ACCESS);
        const gint64 mtime = g_file_info_get_attribute_uint64(info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
        updateLastAccess(agg, atime > 0 ? atime : mtime);

        if (type == G_FILE_TYPE_DIRECTORY) {
            GFile *child = g_file_get_child(file, g_file_info_get_name(info));
            scanRecursive(child, agg, error);
            g_object_unref(child);
        } else if (type == G_FILE_TYPE_REGULAR || type == G_FILE_TYPE_SYMBOLIC_LINK) {
            agg.sizeBytes += static_cast<quint64>(qMax<qint64>(0, fileSize));
            agg.fileCount += 1;
        }
        g_object_unref(info);
    }
    g_object_unref(enumerator);
}

QString ensureCanonicalOrOriginal(const QString &path)
{
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    if (!canonical.isEmpty()) {
        return canonical;
    }
    return QDir::cleanPath(path);
}

} // namespace

QString CleanerScanner::resolveCacheHome()
{
    const QString xdgCache = qEnvironmentVariable("XDG_CACHE_HOME").trimmed();
    if (!xdgCache.isEmpty()) {
        return ensureCanonicalOrOriginal(xdgCache);
    }
    return ensureCanonicalOrOriginal(QDir::homePath() + QStringLiteral("/.cache"));
}

QString CleanerScanner::resolveThumbnailRoot()
{
    return ensureCanonicalOrOriginal(resolveCacheHome() + QStringLiteral("/thumbnails"));
}

QString CleanerScanner::resolveFailThumbnailRoot()
{
    return ensureCanonicalOrOriginal(resolveThumbnailRoot() + QStringLiteral("/fail"));
}

QVector<CacheNodeStat> CleanerScanner::scanTopLevel(QString *errorMessage) const
{
    if (errorMessage) {
        errorMessage->clear();
    }

    QVector<CacheNodeStat> out;
    const QString cacheHome = resolveCacheHome();
    QDir root(cacheHome);
    if (!root.exists()) {
        return out;
    }

    const QFileInfoList entries = root.entryInfoList(
                QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Files | QDir::System | QDir::Hidden,
                QDir::Name | QDir::IgnoreCase);

    out.reserve(entries.size());
    for (const QFileInfo &entry : entries) {
        QString localError;
        const CacheNodeStat stat = scanPathRecursive(entry.absoluteFilePath(),
                                                     entry.fileName(),
                                                     &localError);
        if (!localError.isEmpty() && errorMessage && errorMessage->isEmpty()) {
            *errorMessage = localError;
        }
        out.push_back(stat);
    }
    return out;
}

CacheNodeStat CleanerScanner::scanPathRecursive(const QString &path, const QString &name,
                                                QString *errorMessage) const
{
    if (errorMessage) {
        errorMessage->clear();
    }

    CacheNodeStat stat;
    stat.path = ensureCanonicalOrOriginal(path);
    stat.name = name;

    GError *gerr = nullptr;
    GFile *root = g_file_new_for_path(stat.path.toUtf8().constData());
    Aggregate agg;
    scanRecursive(root, agg, &gerr);
    g_object_unref(root);

    stat.sizeBytes = agg.sizeBytes;
    stat.fileCount = agg.fileCount;
    stat.lastAccessEpoch = agg.lastAccessEpoch;

    if (gerr) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(gerr->message);
        }
        g_error_free(gerr);
    }
    return stat;
}

} // namespace Slm::Cleaner
