#include "archivebackend.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStringList>
#include <QTimeZone>

extern "C" {
#include <archive.h>
#include <archive_entry.h>
}

namespace Slm::Archive {
namespace {

constexpr qint64 kDefaultMaxExpandedBytes = 8LL * 1024LL * 1024LL * 1024LL; // 8 GiB
constexpr qint64 kDefaultMaxCompressInputBytes = 12LL * 1024LL * 1024LL * 1024LL; // 12 GiB
constexpr int kDefaultMaxEntries = 200000;
constexpr int kDefaultTimeoutSeconds = 300;
constexpr double kDefaultMaxExpandRatio = 120.0;

QVariantMap makeError(const QString &code,
                      const QString &userMessage,
                      const QString &detail = QString())
{
    QVariantMap out;
    out.insert(QStringLiteral("ok"), false);
    out.insert(QStringLiteral("errorCode"), code);
    out.insert(QStringLiteral("message"), userMessage);
    if (!detail.trimmed().isEmpty()) {
        out.insert(QStringLiteral("detail"), detail);
    }
    return out;
}

QString detectLayout(const QVariantList &entries)
{
    if (entries.isEmpty()) {
        return QStringLiteral("multi_root");
    }
    if (entries.size() == 1) {
        const QVariantMap row = entries.first().toMap();
        if (!row.value(QStringLiteral("isDir")).toBool()) {
            return QStringLiteral("single_file");
        }
    }

    QSet<QString> roots;
    bool hasNested = false;
    for (const QVariant &v : entries) {
        const QVariantMap row = v.toMap();
        QString p = row.value(QStringLiteral("path")).toString().trimmed();
        if (p.isEmpty()) {
            continue;
        }
        while (p.startsWith(QStringLiteral("./"))) {
            p.remove(0, 2);
        }
        if (p.endsWith(QLatin1Char('/'))) {
            p.chop(1);
        }
        if (p.isEmpty()) {
            continue;
        }
        const int slash = p.indexOf(QLatin1Char('/'));
        if (slash > 0) {
            hasNested = true;
            roots.insert(p.left(slash));
        } else {
            roots.insert(p);
        }
    }

    if (roots.size() == 1 && hasNested) {
        return QStringLiteral("single_root_folder");
    }
    return QStringLiteral("multi_root");
}

bool isCancelled(std::atomic_bool *flag)
{
    return flag && flag->load();
}

qint64 envMaxExpandedBytes()
{
    const qint64 v = qEnvironmentVariableIntValue("SLM_ARCHIVE_MAX_EXPANDED_BYTES");
    if (v > 0) {
        return v;
    }
    return kDefaultMaxExpandedBytes;
}

int envMaxEntries()
{
    const int v = qEnvironmentVariableIntValue("SLM_ARCHIVE_MAX_ENTRIES");
    if (v > 0) {
        return v;
    }
    return kDefaultMaxEntries;
}

qint64 envMaxCompressInputBytes()
{
    const qint64 v = qEnvironmentVariableIntValue("SLM_ARCHIVE_MAX_COMPRESS_INPUT_BYTES");
    if (v > 0) {
        return v;
    }
    return kDefaultMaxCompressInputBytes;
}

int envTimeoutSeconds()
{
    const int v = qEnvironmentVariableIntValue("SLM_ARCHIVE_TIMEOUT_SECONDS");
    if (v > 0) {
        return v;
    }
    return kDefaultTimeoutSeconds;
}

double envMaxExpandRatio()
{
    const QByteArray raw = qgetenv("SLM_ARCHIVE_MAX_EXPAND_RATIO");
    if (!raw.isEmpty()) {
        bool ok = false;
        const double v = QString::fromUtf8(raw).toDouble(&ok);
        if (ok && v > 1.0) {
            return v;
        }
    }
    return kDefaultMaxExpandRatio;
}

} // namespace

QString ArchiveBackend::normalizeArchiveEntryPath(const QString &rawPath)
{
    QString p = rawPath;
    p.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (p.startsWith(QStringLiteral("./"))) {
        p.remove(0, 2);
    }
    p = QDir::cleanPath(p);
    while (p.startsWith(QLatin1Char('/'))) {
        p.remove(0, 1);
    }
    return p.trimmed();
}

bool ArchiveBackend::isUnsafeArchiveEntryPath(const QString &relPath)
{
    const QString p = normalizeArchiveEntryPath(relPath);
    if (p.isEmpty() || p == QStringLiteral(".")) {
        return true;
    }
    if (p == QStringLiteral("..") || p.startsWith(QStringLiteral("../"))) {
        return true;
    }
    if (p.contains(QStringLiteral("/../"))) {
        return true;
    }
    return false;
}

QString ArchiveBackend::safeExtractTargetPath(const QString &destRoot, const QString &relPath)
{
    const QString cleanRel = normalizeArchiveEntryPath(relPath);
    const QString rootAbs = QFileInfo(destRoot).absoluteFilePath();
    const QString target = QDir(rootAbs).absoluteFilePath(cleanRel);
    const QString normRoot = QDir(rootAbs).absolutePath();
    const QString normTarget = QDir(target).absolutePath();
    if (normTarget == normRoot || normTarget.startsWith(normRoot + QLatin1Char('/'))) {
        return target;
    }
    return QString();
}

QString ArchiveBackend::uniqueDestinationPath(const QString &basePath)
{
    QFileInfo fi(basePath);
    if (!fi.exists()) {
        return fi.absoluteFilePath();
    }

    const QString abs = fi.absoluteFilePath();
    const QString dir = fi.absolutePath();
    const QString name = fi.completeBaseName().isEmpty() ? fi.fileName() : fi.completeBaseName();
    const QString suffix = fi.suffix();
    for (int i = 2; i < 10000; ++i) {
        QString candidate = QStringLiteral("%1/%2 %3").arg(dir, name).arg(i);
        if (!suffix.isEmpty()) {
            candidate += QStringLiteral(".%1").arg(suffix);
        }
        if (!QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return abs;
}

QVariantMap ArchiveBackend::listArchive(const QString &archivePath, int maxEntries)
{
    const QString archiveFilePath = QFileInfo(archivePath).absoluteFilePath();
    if (!QFileInfo::exists(archiveFilePath)) {
        return makeError(QStringLiteral("ERR_NOT_FOUND"),
                         QStringLiteral("Archive tidak ditemukan."),
                         archiveFilePath);
    }

    archive *a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);

    if (archive_read_open_filename(a, archiveFilePath.toUtf8().constData(), 10240) != ARCHIVE_OK) {
        const QString detail = QString::fromUtf8(archive_error_string(a));
        archive_read_free(a);
        return makeError(QStringLiteral("ERR_UNSUPPORTED_OR_CORRUPT"),
                         QStringLiteral("Arsip tidak bisa dibuka."),
                         detail);
    }

    QVariantList entries;
    entries.reserve(qBound(1, maxEntries, envMaxEntries()));

    int count = 0;
    qint64 totalUncompressed = 0;
    archive_entry *entry = nullptr;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char *nameRaw = archive_entry_pathname(entry);
        const QString relPath = normalizeArchiveEntryPath(QString::fromUtf8(nameRaw ? nameRaw : ""));
        if (relPath.isEmpty()) {
            archive_read_data_skip(a);
            continue;
        }

        const qint64 size = static_cast<qint64>(archive_entry_size(entry));
        totalUncompressed += qMax<qint64>(0, size);

        QVariantMap row;
        row.insert(QStringLiteral("path"), relPath);
        row.insert(QStringLiteral("name"), QFileInfo(relPath).fileName());
        row.insert(QStringLiteral("size"), size);
        row.insert(QStringLiteral("isDir"), archive_entry_filetype(entry) == AE_IFDIR);
        row.insert(QStringLiteral("isSymlink"), archive_entry_filetype(entry) == AE_IFLNK);
        row.insert(QStringLiteral("mtime"),
                   QDateTime::fromSecsSinceEpoch(static_cast<qint64>(archive_entry_mtime(entry)),
                                                 QTimeZone::UTC)
                       .toString(Qt::ISODate));
        entries.push_back(row);

        ++count;
        archive_read_data_skip(a);
        if (count >= qBound(1, maxEntries, envMaxEntries())) {
            break;
        }
    }

    const int rc = archive_read_close(a);
    Q_UNUSED(rc)
    archive_read_free(a);

    QVariantMap out;
    out.insert(QStringLiteral("ok"), true);
    out.insert(QStringLiteral("entries"), entries);
    out.insert(QStringLiteral("entryCount"), count);
    out.insert(QStringLiteral("totalUncompressedBytes"), totalUncompressed);
    out.insert(QStringLiteral("layout"), detectLayout(entries));
    return out;
}

QVariantMap ArchiveBackend::testArchive(const QString &archivePath)
{
    const QVariantMap listed = listArchive(archivePath, qBound(1, envMaxEntries(), 500000));
    if (!listed.value(QStringLiteral("ok")).toBool()) {
        return listed;
    }

    QVariantMap out;
    out.insert(QStringLiteral("ok"), true);
    out.insert(QStringLiteral("message"), QStringLiteral("Arsip valid."));
    out.insert(QStringLiteral("entryCount"), listed.value(QStringLiteral("entryCount")).toInt());
    out.insert(QStringLiteral("layout"), listed.value(QStringLiteral("layout")).toString());
    return out;
}

QVariantMap ArchiveBackend::extractArchive(const QString &archivePath,
                                           const QString &destinationDir,
                                           const QString &mode,
                                           const QString &conflictPolicy,
                                           std::atomic_bool *cancelFlag,
                                           const ProgressCallback &progress)
{
    Q_UNUSED(mode)

    const QString archiveFilePath = QFileInfo(archivePath).absoluteFilePath();
    if (!QFileInfo::exists(archiveFilePath)) {
        return makeError(QStringLiteral("ERR_NOT_FOUND"), QStringLiteral("Arsip tidak ditemukan."));
    }

    QString destinationRoot = QFileInfo(destinationDir).absoluteFilePath();
    if (destinationRoot.trimmed().isEmpty()) {
        destinationRoot = QFileInfo(archiveFilePath).absolutePath();
    }
    QDir().mkpath(destinationRoot);

    QString baseName = QFileInfo(archiveFilePath).completeBaseName();
    if (baseName.isEmpty()) {
        baseName = QFileInfo(archiveFilePath).baseName();
    }
    if (baseName.isEmpty()) {
        baseName = QStringLiteral("Extracted");
    }

    QString targetFolder = QDir(destinationRoot).absoluteFilePath(baseName);
    const QString policy = conflictPolicy.trimmed().isEmpty()
                               ? QStringLiteral("auto_rename")
                               : conflictPolicy.trimmed().toLower();
    if (QFileInfo::exists(targetFolder)) {
        if (policy == QStringLiteral("replace")) {
            QDir(targetFolder).removeRecursively();
        } else if (policy == QStringLiteral("skip")) {
            return makeError(QStringLiteral("ERR_NAME_CONFLICT"),
                             QStringLiteral("Folder tujuan sudah ada."));
        } else {
            targetFolder = uniqueDestinationPath(targetFolder);
        }
    }
    if (!QDir().mkpath(targetFolder)) {
        return makeError(QStringLiteral("ERR_EXTRACT_FAILED"),
                         QStringLiteral("Tidak bisa membuat folder tujuan ekstraksi."));
    }

    const auto failExtract = [&](const QString &errorCode,
                                 const QString &message,
                                 const QString &detail = QString()) -> QVariantMap {
        QDir(targetFolder).removeRecursively();
        return makeError(errorCode, message, detail);
    };

    archive *a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);
    if (archive_read_open_filename(a, archiveFilePath.toUtf8().constData(), 10240) != ARCHIVE_OK) {
        const QString detail = QString::fromUtf8(archive_error_string(a));
        archive_read_free(a);
        return failExtract(QStringLiteral("ERR_UNSUPPORTED_OR_CORRUPT"),
                           QStringLiteral("Arsip tidak bisa dibuka."),
                           detail);
    }

    archive *ext = archive_write_disk_new();
    archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME);
    archive_write_disk_set_standard_lookup(ext);

    const int maxEntries = envMaxEntries();
    const qint64 maxExpanded = envMaxExpandedBytes();
    const int timeoutMs = qMax(1, envTimeoutSeconds()) * 1000;
    const double maxRatio = envMaxExpandRatio();
    const qint64 archiveCompressedBytes = qMax<qint64>(1, QFileInfo(archiveFilePath).size());
    QElapsedTimer timer;
    timer.start();

    int entryCount = 0;
    qint64 extractedBytes = 0;

    archive_entry *entry = nullptr;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        if (timer.elapsed() > timeoutMs) {
            archive_write_free(ext);
            archive_read_free(a);
            return failExtract(QStringLiteral("ERR_TIMEOUT"),
                               QStringLiteral("Ekstraksi melebihi batas waktu."));
        }
        if (isCancelled(cancelFlag)) {
            archive_write_free(ext);
            archive_read_free(a);
            return failExtract(QStringLiteral("ERR_CANCELLED"),
                               QStringLiteral("Operasi dibatalkan."));
        }

        const QString original = QString::fromUtf8(archive_entry_pathname(entry));
        const QString relPath = normalizeArchiveEntryPath(original);
        if (isUnsafeArchiveEntryPath(relPath)) {
            archive_read_data_skip(a);
            continue;
        }

        if (archive_entry_filetype(entry) == AE_IFLNK || archive_entry_hardlink(entry) != nullptr) {
            archive_read_data_skip(a);
            continue;
        }

        const QString outPath = safeExtractTargetPath(targetFolder, relPath);
        if (outPath.isEmpty()) {
            archive_read_data_skip(a);
            continue;
        }

        archive_entry_set_pathname(entry, outPath.toUtf8().constData());
        archive_entry_set_uid(entry, 0);
        archive_entry_set_gid(entry, 0);

        const int wr = archive_write_header(ext, entry);
        if (wr != ARCHIVE_OK) {
            const QString detail = QString::fromUtf8(archive_error_string(ext));
            archive_write_free(ext);
            archive_read_free(a);
            return failExtract(QStringLiteral("ERR_EXTRACT_FAILED"),
                               QStringLiteral("Ekstraksi gagal."),
                               detail);
        }

        const qint64 expected = qMax<qint64>(0, static_cast<qint64>(archive_entry_size(entry)));
        if (expected > maxExpanded) {
            archive_write_free(ext);
            archive_read_free(a);
            return failExtract(QStringLiteral("ERR_RESOURCE_LIMIT"),
                               QStringLiteral("Arsip melebihi batas aman ekstraksi."));
        }

        const void *buff = nullptr;
        size_t size = 0;
        la_int64_t offset = 0;
        while (true) {
            if (timer.elapsed() > timeoutMs) {
                archive_write_free(ext);
                archive_read_free(a);
                return failExtract(QStringLiteral("ERR_TIMEOUT"),
                                   QStringLiteral("Ekstraksi melebihi batas waktu."));
            }
            const int rr = archive_read_data_block(a, &buff, &size, &offset);
            if (rr == ARCHIVE_EOF) {
                break;
            }
            if (rr != ARCHIVE_OK) {
                const QString detail = QString::fromUtf8(archive_error_string(a));
                archive_write_free(ext);
                archive_read_free(a);
                return failExtract(QStringLiteral("ERR_EXTRACT_FAILED"),
                                   QStringLiteral("Ekstraksi gagal."),
                                   detail);
            }
            if (archive_write_data_block(ext, buff, size, offset) != ARCHIVE_OK) {
                const QString detail = QString::fromUtf8(archive_error_string(ext));
                archive_write_free(ext);
                archive_read_free(a);
                return failExtract(QStringLiteral("ERR_EXTRACT_FAILED"),
                                   QStringLiteral("Ekstraksi gagal."),
                                   detail);
            }
            extractedBytes += static_cast<qint64>(size);
            if (extractedBytes > maxExpanded) {
                archive_write_free(ext);
                archive_read_free(a);
                return failExtract(QStringLiteral("ERR_RESOURCE_LIMIT"),
                                   QStringLiteral("Arsip melebihi batas aman ekstraksi."));
            }
            const double ratio = static_cast<double>(extractedBytes) / static_cast<double>(archiveCompressedBytes);
            if (ratio > maxRatio) {
                archive_write_free(ext);
                archive_read_free(a);
                return failExtract(QStringLiteral("ERR_RESOURCE_LIMIT"),
                                   QStringLiteral("Rasio ekstraksi arsip tidak aman."));
            }
        }

        ++entryCount;
        if (entryCount > maxEntries) {
            archive_write_free(ext);
            archive_read_free(a);
            return failExtract(QStringLiteral("ERR_RESOURCE_LIMIT"),
                               QStringLiteral("Jumlah file arsip melebihi batas aman."));
        }

        if (progress && (entryCount % 32 == 0)) {
            progress(qMin(95, (entryCount % 95) + 1));
        }
    }

    archive_write_close(ext);
    archive_write_free(ext);
    archive_read_close(a);
    archive_read_free(a);

    if (progress) {
        progress(100);
    }

    QVariantMap out;
    out.insert(QStringLiteral("ok"), true);
    out.insert(QStringLiteral("destination"), targetFolder);
    out.insert(QStringLiteral("entryCount"), entryCount);
    return out;
}

QVariantMap ArchiveBackend::compressPaths(const QStringList &paths,
                                          const QString &destinationPath,
                                          const QString &format,
                                          std::atomic_bool *cancelFlag,
                                          const ProgressCallback &progress)
{
    const QString fmt = format.trimmed().isEmpty() ? QStringLiteral("zip") : format.trimmed().toLower();
    if (fmt != QStringLiteral("zip")) {
        return makeError(QStringLiteral("ERR_UNSUPPORTED_FORMAT"),
                         QStringLiteral("Format kompresi belum didukung."));
    }

    QStringList sourcePaths;
    for (const QString &raw : paths) {
        const QString abs = QFileInfo(raw).absoluteFilePath();
        if (QFileInfo::exists(abs)) {
            sourcePaths.push_back(abs);
        }
    }
    sourcePaths.removeDuplicates();
    if (sourcePaths.isEmpty()) {
        return makeError(QStringLiteral("ERR_INVALID_INPUT"),
                         QStringLiteral("Tidak ada file yang bisa dikompres."));
    }

    const QString archivePath = QFileInfo(destinationPath).absoluteFilePath();
    QDir().mkpath(QFileInfo(archivePath).absolutePath());
    const int maxEntries = envMaxEntries();
    const qint64 maxInputBytes = envMaxCompressInputBytes();
    const int timeoutMs = qMax(1, envTimeoutSeconds()) * 1000;
    QElapsedTimer timer;
    timer.start();

    archive *a = archive_write_new();
    archive_write_set_format_zip(a);
    if (archive_write_open_filename(a, archivePath.toUtf8().constData()) != ARCHIVE_OK) {
        const QString detail = QString::fromUtf8(archive_error_string(a));
        archive_write_free(a);
        return makeError(QStringLiteral("ERR_COMPRESS_FAILED"),
                         QStringLiteral("Gagal membuat arsip."), detail);
    }

    QVariantList files;
    qint64 totalInputBytes = 0;
    int candidateEntries = 0;
    for (const QString &src : std::as_const(sourcePaths)) {
        QFileInfo fi(src);
        if (fi.isDir()) {
            QDirIterator it(src,
                            QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                            QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString p = it.next();
                const QFileInfo childFi = it.fileInfo();
                files.push_back(p);
                ++candidateEntries;
                if (candidateEntries > maxEntries) {
                    archive_write_close(a);
                    archive_write_free(a);
                    return makeError(QStringLiteral("ERR_RESOURCE_LIMIT"),
                                     QStringLiteral("Jumlah file arsip melebihi batas aman."));
                }
                if (childFi.isFile()) {
                    totalInputBytes += qMax<qint64>(0, childFi.size());
                    if (totalInputBytes > maxInputBytes) {
                        archive_write_close(a);
                        archive_write_free(a);
                        return makeError(QStringLiteral("ERR_RESOURCE_LIMIT"),
                                         QStringLiteral("Ukuran kompresi melebihi batas aman."));
                    }
                }
            }
        } else {
            files.push_back(src);
            ++candidateEntries;
            if (candidateEntries > maxEntries) {
                archive_write_close(a);
                archive_write_free(a);
                return makeError(QStringLiteral("ERR_RESOURCE_LIMIT"),
                                 QStringLiteral("Jumlah file arsip melebihi batas aman."));
            }
            if (fi.isFile()) {
                totalInputBytes += qMax<qint64>(0, fi.size());
                if (totalInputBytes > maxInputBytes) {
                    archive_write_close(a);
                    archive_write_free(a);
                    return makeError(QStringLiteral("ERR_RESOURCE_LIMIT"),
                                     QStringLiteral("Ukuran kompresi melebihi batas aman."));
                }
            }
        }
    }
    const int total = qMax(1, files.size());

    int index = 0;
    for (const QVariant &v : files) {
        if (timer.elapsed() > timeoutMs) {
            archive_write_close(a);
            archive_write_free(a);
            return makeError(QStringLiteral("ERR_TIMEOUT"),
                             QStringLiteral("Kompresi melebihi batas waktu."));
        }
        if (isCancelled(cancelFlag)) {
            archive_write_close(a);
            archive_write_free(a);
            return makeError(QStringLiteral("ERR_CANCELLED"), QStringLiteral("Operasi dibatalkan."));
        }

        const QString path = v.toString();
        QFileInfo fi(path);
        if (!fi.exists()) {
            continue;
        }

        QString relName;
        bool matchedRoot = false;
        for (const QString &root : std::as_const(sourcePaths)) {
            const QString rootAbs = QFileInfo(root).absoluteFilePath();
            if (path == rootAbs) {
                relName = QFileInfo(rootAbs).fileName();
                matchedRoot = true;
                break;
            }
            if (path.startsWith(rootAbs + QLatin1Char('/'))) {
                relName = QFileInfo(rootAbs).fileName() + QLatin1Char('/')
                          + path.mid(rootAbs.size() + 1);
                matchedRoot = true;
                break;
            }
        }
        if (!matchedRoot || relName.trimmed().isEmpty()) {
            relName = fi.fileName();
        }

        archive_entry *entry = archive_entry_new();
        archive_entry_set_pathname(entry, relName.toUtf8().constData());
        archive_entry_set_size(entry, fi.isDir() ? 0 : fi.size());
        archive_entry_set_filetype(entry, fi.isDir() ? AE_IFDIR : AE_IFREG);
        archive_entry_set_perm(entry, 0644);

        if (archive_write_header(a, entry) != ARCHIVE_OK) {
            const QString detail = QString::fromUtf8(archive_error_string(a));
            archive_entry_free(entry);
            archive_write_close(a);
            archive_write_free(a);
            return makeError(QStringLiteral("ERR_COMPRESS_FAILED"),
                             QStringLiteral("Gagal menulis arsip."), detail);
        }

        if (fi.isFile()) {
            QFile f(path);
            if (f.open(QIODevice::ReadOnly)) {
                QByteArray chunk;
                chunk.resize(64 * 1024);
                while (true) {
                    if (timer.elapsed() > timeoutMs) {
                        archive_entry_free(entry);
                        archive_write_close(a);
                        archive_write_free(a);
                        return makeError(QStringLiteral("ERR_TIMEOUT"),
                                         QStringLiteral("Kompresi melebihi batas waktu."));
                    }
                    const qint64 n = f.read(chunk.data(), chunk.size());
                    if (n <= 0) {
                        break;
                    }
                    if (archive_write_data(a, chunk.constData(), static_cast<size_t>(n)) < 0) {
                        const QString detail = QString::fromUtf8(archive_error_string(a));
                        archive_entry_free(entry);
                        archive_write_close(a);
                        archive_write_free(a);
                        return makeError(QStringLiteral("ERR_COMPRESS_FAILED"),
                                         QStringLiteral("Gagal menulis arsip."), detail);
                    }
                }
            }
        }

        archive_entry_free(entry);

        ++index;
        if (progress) {
            progress(qMin(99, (index * 100) / total));
        }
    }

    archive_write_close(a);
    archive_write_free(a);

    if (progress) {
        progress(100);
    }

    QVariantMap out;
    out.insert(QStringLiteral("ok"), true);
    out.insert(QStringLiteral("archive"), archivePath);
    out.insert(QStringLiteral("count"), index);
    return out;
}

} // namespace Slm::Archive
