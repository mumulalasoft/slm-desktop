#include "cleanerengine.h"
#include "cleanerscanner.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QStandardPaths>

#ifdef signals
#undef signals
#endif
#include <gio/gio.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>

namespace Slm::Cleaner {
namespace {

struct DeleteCandidate {
    QString path;
    quint64 sizeBytes = 0;
};

QString canonicalPathOrClean(const QString &path)
{
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    if (!canonical.isEmpty()) {
        return canonical;
    }
    return QDir::cleanPath(path);
}

bool isUnderRoot(const QString &root, const QString &path)
{
    const QString cleanRoot = canonicalPathOrClean(root);
    const QString cleanPath = canonicalPathOrClean(path);
    return cleanPath == cleanRoot || cleanPath.startsWith(cleanRoot + QLatin1Char('/'));
}

bool isProtectedPath(const QString &path)
{
    const QString cleaned = canonicalPathOrClean(path);
    const QString home = QDir::homePath();
    const QString config = canonicalPathOrClean(home + QStringLiteral("/.config"));
    const QString share = canonicalPathOrClean(home + QStringLiteral("/.local/share"));
    return cleaned == config
            || cleaned.startsWith(config + QLatin1Char('/'))
            || cleaned == share
            || cleaned.startsWith(share + QLatin1Char('/'));
}

bool isFileLikelyInUse(const QString &filePath)
{
    const QByteArray native = QFile::encodeName(filePath);
    const int fd = ::open(native.constData(), O_RDONLY);
    if (fd < 0) {
        return false;
    }
    const int lockRc = ::flock(fd, LOCK_EX | LOCK_NB);
    if (lockRc == 0) {
        ::flock(fd, LOCK_UN);
        ::close(fd);
        return false;
    }
    ::close(fd);
    return true;
}

bool isLikelyCorruptedCandidate(const QString &filePath,
                                const QString &cacheRoot,
                                bool isSymlink,
                                bool danglingSymlink,
                                goffset size)
{
    if (danglingSymlink) {
        return true;
    }
    if (size < 0) {
        return true;
    }
    const QString thumbRoot = canonicalPathOrClean(cacheRoot + QStringLiteral("/thumbnails"));
    const QString cleanPath = canonicalPathOrClean(filePath);
    const bool isThumb = isUnderRoot(thumbRoot, cleanPath);
    if (!isThumb) {
        return false;
    }
    const QFileInfo fi(cleanPath);
    const QString suffix = fi.suffix().toLower();
    if (suffix != QLatin1String("png")) {
        return true;
    }
    if (!isSymlink && size == 0) {
        return true;
    }
    return false;
}

bool shouldDelete(const QString &mode,
                  const QString &filePath,
                  const QString &cacheRoot,
                  bool isSymlink,
                  bool danglingSymlink,
                  gint64 atime,
                  gint64 mtime,
                  goffset size,
                  int days)
{
    if (mode == QLatin1String("full")) {
        return true;
    }
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const qint64 ts = atime > 0 ? atime : mtime;
    const qint64 ageDays = (ts > 0) ? (now - ts) / 86400 : (days + 1);
    if (mode == QLatin1String("age")) {
        return ageDays >= days;
    }
    // smart
    const bool oldEnough = ageDays >= days;
    const bool bigEnough = size >= (1ll * 1024ll * 1024ll); // >=1MB
    const bool corrupted = isLikelyCorruptedCandidate(filePath, cacheRoot, isSymlink, danglingSymlink, size);
    return (oldEnough && bigEnough) || corrupted;
}

void enumerateCandidates(const QString &cacheRoot,
                         const QString &basePath,
                         const QString &mode,
                         int days,
                         QVector<DeleteCandidate> &out)
{
    GError *gerr = nullptr;
    GFile *root = g_file_new_for_path(basePath.toUtf8().constData());
    GFileEnumerator *enumerator = g_file_enumerate_children(
                root,
                "standard::name,standard::type,standard::size,time::access,time::modified",
                G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                nullptr,
                &gerr);
    if (!enumerator) {
        if (gerr) {
            g_error_free(gerr);
        }
        g_object_unref(root);
        return;
    }

    while (true) {
        GFileInfo *info = g_file_enumerator_next_file(enumerator, nullptr, &gerr);
        if (!info) {
            break;
        }
        const GFileType type = g_file_info_get_file_type(info);
        const char *name = g_file_info_get_name(info);
        GFile *child = g_file_get_child(root, name);
        char *childPath = g_file_get_path(child);
        const QString qPath = QString::fromUtf8(childPath ? childPath : "");
        g_free(childPath);

        if (type == G_FILE_TYPE_DIRECTORY) {
            enumerateCandidates(cacheRoot, qPath, mode, days, out);
        } else if (type == G_FILE_TYPE_REGULAR || type == G_FILE_TYPE_SYMBOLIC_LINK) {
            const goffset size = g_file_info_get_size(info);
            const gint64 atime = g_file_info_get_attribute_uint64(info, G_FILE_ATTRIBUTE_TIME_ACCESS);
            const gint64 mtime = g_file_info_get_attribute_uint64(info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
            const bool isSymlink = (type == G_FILE_TYPE_SYMBOLIC_LINK);
            const QFileInfo qfi(qPath);
            const bool danglingSymlink = isSymlink && !qfi.exists();
            if (shouldDelete(mode, qPath, cacheRoot, isSymlink, danglingSymlink, atime, mtime, size, days)) {
                DeleteCandidate c;
                c.path = qPath;
                c.sizeBytes = static_cast<quint64>(qMax<qint64>(0, size));
                out.push_back(c);
            }
        }
        g_object_unref(child);
        g_object_unref(info);
    }
    if (gerr) {
        g_error_free(gerr);
    }
    g_object_unref(enumerator);
    g_object_unref(root);
}

QStringList collectTargets(const CleanRequest &request)
{
    const QString cacheRoot = CleanerScanner::resolveCacheHome();
    const QString thumbRoot = CleanerScanner::resolveThumbnailRoot();
    const QString failRoot = CleanerScanner::resolveFailThumbnailRoot();

    QStringList targets;
    if (request.clearThumbnail) {
        targets << thumbRoot;
    }
    if (request.clearFailedThumbnail && !targets.contains(failRoot)) {
        targets << failRoot;
    }
    for (const QString &selected : request.selectedAppPaths) {
        QString p = selected.trimmed();
        if (p.isEmpty()) {
            continue;
        }
        if (!p.startsWith(QLatin1Char('/'))) {
            p = QDir(cacheRoot).filePath(p);
        }
        if (!targets.contains(p)) {
            targets << p;
        }
    }
    return targets;
}

void removeEmptyDirectories(const QString &rootPath)
{
    if (!QFileInfo::exists(rootPath)) {
        return;
    }
    QDirIterator it(rootPath,
                    QDir::NoDotAndDotDot | QDir::Dirs | QDir::Hidden | QDir::System,
                    QDirIterator::Subdirectories);
    QStringList dirs;
    while (it.hasNext()) {
        dirs << it.next();
    }
    std::sort(dirs.begin(), dirs.end(), [](const QString &a, const QString &b) {
        return a.size() > b.size();
    });
    for (const QString &dirPath : dirs) {
        QDir dir(dirPath);
        dir.rmdir(QStringLiteral("."));
    }
}

} // namespace

CleanerEngine::CleanerEngine(QObject *parent)
    : QObject(parent)
{
}

QVariantMap CleanerEngine::preview(const CleanRequest &request)
{
    return run(request, true);
}

QVariantMap CleanerEngine::clean(const CleanRequest &request)
{
    return run(request, false);
}

QVariantMap CleanerEngine::run(const CleanRequest &request, bool dryRun)
{
    QVariantMap result;
    const QString cacheRoot = CleanerScanner::resolveCacheHome();
    const QStringList targets = collectTargets(request);
    if (targets.isEmpty()) {
        result.insert(QStringLiteral("ok"), true);
        result.insert(QStringLiteral("deletedFiles"), 0);
        result.insert(QStringLiteral("deletedBytes"), QVariant::fromValue<qulonglong>(0));
        result.insert(QStringLiteral("skippedFiles"), 0);
        result.insert(QStringLiteral("failedFiles"), 0);
        return result;
    }

    QVector<DeleteCandidate> candidates;
    for (const QString &target : targets) {
        const QString cleaned = canonicalPathOrClean(target);
        if (!isUnderRoot(cacheRoot, cleaned) || isProtectedPath(cleaned)) {
            continue;
        }
        if (!QFileInfo::exists(cleaned)) {
            continue;
        }
        enumerateCandidates(cacheRoot, cleaned, request.mode, request.deleteAfterDays, candidates);
    }

    qulonglong deletedBytes = 0;
    int deletedFiles = 0;
    int skippedFiles = 0;
    int failedFiles = 0;
    QVariantList skippedPaths;
    QVariantList failedPaths;

    const int total = candidates.size();
    int processed = 0;
    for (const DeleteCandidate &candidate : candidates) {
        ++processed;
        if (dryRun) {
            deletedBytes += candidate.sizeBytes;
            ++deletedFiles;
            continue;
        }
        if (!isUnderRoot(cacheRoot, candidate.path) || isProtectedPath(candidate.path)) {
            ++skippedFiles;
            skippedPaths.push_back(candidate.path);
            continue;
        }
        if (isFileLikelyInUse(candidate.path)) {
            ++skippedFiles;
            skippedPaths.push_back(candidate.path);
            continue;
        }
        const QFileInfo info(candidate.path);
        if (!info.exists()) {
            ++skippedFiles;
            continue;
        }
        if (!info.isWritable()) {
            ++failedFiles;
            failedPaths.push_back(candidate.path);
            continue;
        }
        if (QFile::remove(candidate.path)) {
            deletedBytes += candidate.sizeBytes;
            ++deletedFiles;
        } else {
            ++failedFiles;
            failedPaths.push_back(candidate.path);
        }
        if (processed % 64 == 0 || processed == total) {
            const int percent = total > 0 ? (processed * 100) / total : 100;
            emit progressChanged(percent,
                                 QStringLiteral("%1/%2").arg(processed).arg(total));
        }
    }

    if (!dryRun) {
        for (const QString &target : targets) {
            removeEmptyDirectories(target);
        }
    }

    result.insert(QStringLiteral("ok"), failedFiles == 0);
    result.insert(QStringLiteral("preview"), dryRun);
    result.insert(QStringLiteral("deletedFiles"), deletedFiles);
    result.insert(QStringLiteral("deletedBytes"), QVariant::fromValue<qulonglong>(deletedBytes));
    result.insert(QStringLiteral("skippedFiles"), skippedFiles);
    result.insert(QStringLiteral("failedFiles"), failedFiles);
    result.insert(QStringLiteral("skippedPaths"), skippedPaths);
    result.insert(QStringLiteral("failedPaths"), failedPaths);
    return result;
}

} // namespace Slm::Cleaner
