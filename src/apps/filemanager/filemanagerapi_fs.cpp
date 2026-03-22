#include "filemanagerapi.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QUrl>

#pragma push_macro("signals")
#undef signals
#include <gio/gio.h>
#pragma pop_macro("signals")

namespace {

struct GioProgressContextFs {
    std::function<void(qlonglong, qlonglong)> progress;
};

static QStringList uniqueExpandedPathsFs(const QVariantList &paths)
{
    QStringList out;
    QSet<QString> seen;
    for (const QVariant &v : paths) {
        QString p = v.toString().trimmed();
        if (p == QStringLiteral("~")) {
            p = QDir::homePath();
        } else if (p.startsWith(QStringLiteral("~/"))) {
            p = QDir::homePath() + p.mid(1);
        }
        if (p.isEmpty()) {
            continue;
        }
        QFileInfo fi(p);
        const QString abs = fi.absoluteFilePath();
        if (seen.contains(abs)) {
            continue;
        }
        seen.insert(abs);
        out.push_back(abs);
    }
    return out;
}

static QString normalizedPathForPolicyFs(const QString &path)
{
    QString out = QFileInfo(path).absoluteFilePath();
    if (out.isEmpty()) {
        return QString();
    }
    out = QDir::cleanPath(out);
    QFileInfo fi(out);
    if (fi.exists()) {
        const QString canonical = fi.canonicalFilePath();
        if (!canonical.isEmpty()) {
            out = QDir::cleanPath(canonical);
        }
    }
    return out;
}

static QString protectedDesktopPathFs()
{
    return QDir::cleanPath(QDir::homePath() + QStringLiteral("/Desktop"));
}

static bool isProtectedDesktopFolderPathFs(const QString &path)
{
    const QString normalized = normalizedPathForPolicyFs(path);
    if (normalized.isEmpty()) {
        return false;
    }
    return normalized == protectedDesktopPathFs();
}

static bool firstProtectedDesktopPathFs(const QStringList &paths, QString *blockedPath)
{
    for (const QString &p : paths) {
        if (isProtectedDesktopFolderPathFs(p)) {
            if (blockedPath) {
                *blockedPath = p;
            }
            return true;
        }
    }
    return false;
}

static void gioProgressCallbackFs(goffset currentNumBytes, goffset totalNumBytes, gpointer userData)
{
    GioProgressContextFs *ctx = static_cast<GioProgressContextFs *>(userData);
    if (!ctx || !ctx->progress) {
        return;
    }
    ctx->progress(static_cast<qlonglong>(currentNumBytes),
                  static_cast<qlonglong>(totalNumBytes));
}

static qlonglong estimatePathBytesRecursiveFs(const QString &path)
{
    QFileInfo info(path);
    if (!info.exists()) {
        return 0;
    }
    if (info.isFile() || info.isSymLink()) {
        return qMax<qlonglong>(0, info.size());
    }

    qlonglong total = 0;
    QDirIterator it(path,
                    QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo fi = it.fileInfo();
        if (fi.isFile() || fi.isSymLink()) {
            total += qMax<qlonglong>(0, fi.size());
        }
    }
    return total;
}

static bool gioCopyFileFs(const QString &src,
                          const QString &dst,
                          bool overwrite,
                          qlonglong *copiedBytes,
                          qlonglong *totalBytes,
                          const std::function<void(qlonglong, qlonglong)> &progressCb,
                          QString *error)
{
    if (copiedBytes) {
        *copiedBytes = 0;
    }
    if (totalBytes) {
        *totalBytes = 0;
    }
    if (error) {
        error->clear();
    }

    QFileInfo dstInfo(dst);
    QDir parent = dstInfo.absoluteDir();
    if (!parent.exists() && !parent.mkpath(QStringLiteral("."))) {
        if (error) {
            *error = QStringLiteral("mkdir-failed");
        }
        return false;
    }

    GError *gerr = nullptr;
    GFile *srcFile = g_file_new_for_path(src.toUtf8().constData());
    GFile *dstFile = g_file_new_for_path(dst.toUtf8().constData());

    GFileCopyFlags flags = static_cast<GFileCopyFlags>(G_FILE_COPY_ALL_METADATA);
    if (overwrite) {
        flags = static_cast<GFileCopyFlags>(flags | G_FILE_COPY_OVERWRITE);
    }

    GioProgressContextFs ctx{progressCb};
    const bool ok = g_file_copy(srcFile,
                                dstFile,
                                flags,
                                nullptr,
                                progressCb ? gioProgressCallbackFs : nullptr,
                                progressCb ? &ctx : nullptr,
                                &gerr);
    if (!ok && gerr) {
        if (error) {
            *error = QString::fromUtf8(gerr->message);
        }
        g_error_free(gerr);
    }

    const qlonglong total = qMax<qlonglong>(0, QFileInfo(src).size());
    if (totalBytes) {
        *totalBytes = total;
    }
    if (copiedBytes) {
        *copiedBytes = ok ? total : 0;
    }
    if (ok && progressCb) {
        progressCb(total, total);
    }

    g_object_unref(dstFile);
    g_object_unref(srcFile);
    return ok;
}

static bool gioMovePathFs(const QString &src,
                          const QString &dst,
                          bool overwrite,
                          const std::function<void(qlonglong, qlonglong)> &progressCb,
                          QString *error)
{
    if (error) {
        error->clear();
    }
    QFileInfo dstInfo(dst);
    QDir parent = dstInfo.absoluteDir();
    if (!parent.exists() && !parent.mkpath(QStringLiteral("."))) {
        if (error) {
            *error = QStringLiteral("mkdir-failed");
        }
        return false;
    }

    GError *gerr = nullptr;
    GFile *srcFile = g_file_new_for_path(src.toUtf8().constData());
    GFile *dstFile = g_file_new_for_path(dst.toUtf8().constData());
    GFileCopyFlags flags = static_cast<GFileCopyFlags>(G_FILE_COPY_ALL_METADATA);
    if (overwrite) {
        flags = static_cast<GFileCopyFlags>(flags | G_FILE_COPY_OVERWRITE);
    }
    GioProgressContextFs ctx{progressCb};
    const bool ok = g_file_move(srcFile,
                                dstFile,
                                flags,
                                nullptr,
                                progressCb ? gioProgressCallbackFs : nullptr,
                                progressCb ? &ctx : nullptr,
                                &gerr);
    if (!ok && gerr) {
        if (error) {
            *error = QString::fromUtf8(gerr->message);
        }
        g_error_free(gerr);
    }
    if (ok && progressCb) {
        const qlonglong total = qMax<qlonglong>(1, estimatePathBytesRecursiveFs(dst));
        progressCb(total, total);
    }
    g_object_unref(dstFile);
    g_object_unref(srcFile);
    return ok;
}

static bool gioDeleteSinglePathFs(const QString &path, QString *error)
{
    if (error) {
        error->clear();
    }
    GError *gerr = nullptr;
    GFile *file = g_file_new_for_path(path.toUtf8().constData());
    const bool ok = g_file_delete(file, nullptr, &gerr);
    if (!ok && gerr) {
        if (error) {
            *error = QString::fromUtf8(gerr->message);
        }
        g_error_free(gerr);
    }
    g_object_unref(file);
    return ok;
}

static bool removeRecursivelyPathFs(const QString &path, QString *error)
{
    QFileInfo info(path);
    if (!info.exists()) {
        return true;
    }
    if (info.isDir() && !info.isSymLink()) {
        QDir dir(path);
        const QFileInfoList entries = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden,
                                                        QDir::Name);
        for (const QFileInfo &entry : entries) {
            if (!removeRecursivelyPathFs(entry.absoluteFilePath(), error)) {
                return false;
            }
        }
        if (!gioDeleteSinglePathFs(path, error)) {
            if (error) {
                if (error->isEmpty()) {
                    *error = QStringLiteral("rmdir-failed");
                }
            }
            return false;
        }
        return true;
    }
    if (!gioDeleteSinglePathFs(path, error)) {
        if (error) {
            if (error->isEmpty()) {
                *error = QStringLiteral("remove-failed");
            }
        }
        return false;
    }
    return true;
}

static bool copyRecursivelyFs(const QString &src,
                              const QString &dst,
                              const std::function<void(qlonglong, qlonglong)> &progressCb,
                              QString *error)
{
    QFileInfo srcInfo(src);
    if (!srcInfo.exists()) {
        if (error) {
            *error = QStringLiteral("not-found");
        }
        return false;
    }

    const qlonglong overallTotal = qMax<qlonglong>(1, estimatePathBytesRecursiveFs(src));
    qlonglong overallDone = 0;

    std::function<bool(const QString &, const QString &)> copyImpl;
    copyImpl = [&](const QString &currentSrc, const QString &currentDst) -> bool {
        QFileInfo info(currentSrc);
        if (info.isDir() && !info.isSymLink()) {
            QDir dstDir(currentDst);
            if (!dstDir.exists() && !QDir().mkpath(currentDst)) {
                if (error) {
                    *error = QStringLiteral("mkdir-failed");
                }
                return false;
            }
            QDir srcDir(currentSrc);
            const QFileInfoList entries = srcDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden,
                                                                QDir::Name);
            for (const QFileInfo &entry : entries) {
                const QString childSrc = entry.absoluteFilePath();
                const QString childDst = QDir(currentDst).filePath(entry.fileName());
                if (!copyImpl(childSrc, childDst)) {
                    return false;
                }
            }
            return true;
        }

        qlonglong copied = 0;
        qlonglong total = 0;
        const bool copiedOk = gioCopyFileFs(currentSrc,
                                            currentDst,
                                            true,
                                            &copied,
                                            &total,
                                            [&](qlonglong fileDone, qlonglong fileTotal) {
            const qlonglong boundedTotal = qMax<qlonglong>(1, fileTotal);
            const qlonglong boundedDone = qBound<qlonglong>(0, fileDone, boundedTotal);
            const qlonglong currentOverall = qMin(overallTotal, overallDone + boundedDone);
            if (progressCb) {
                progressCb(currentOverall, overallTotal);
            }
        },
                                            error);
        if (!copiedOk) {
            return false;
        }
        overallDone = qMin(overallTotal, overallDone + qMax<qlonglong>(0, total));
        if (progressCb) {
            progressCb(overallDone, overallTotal);
        }
        return true;
    };

    const bool ok = copyImpl(src, dst);
    if (ok && progressCb) {
        progressCb(overallTotal, overallTotal);
    }
    return ok;
}

static QString uniqueTargetPathFs(const QString &dstDir, const QString &name)
{
    QDir dir(dstDir);
    QFileInfo baseInfo(name);
    const QString stem = baseInfo.completeBaseName();
    const QString suffix = baseInfo.suffix();
    for (int i = 0; i < 10000; ++i) {
        QString fileName;
        if (i == 0) {
            fileName = name;
        } else if (!suffix.isEmpty()) {
            fileName = stem + QStringLiteral(" (%1).").arg(i) + suffix;
        } else {
            fileName = name + QStringLiteral(" (%1)").arg(i);
        }
        const QString candidate = dir.filePath(fileName);
        if (!QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return dir.filePath(name + QStringLiteral("-") + QString::number(QDateTime::currentMSecsSinceEpoch()));
}

static QString parseOriginalPathFromTrashInfoFileFs(const QString &infoPath)
{
    QFile infoFile(infoPath);
    if (!infoFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    QTextStream ts(&infoFile);
    while (!ts.atEnd()) {
        const QString line = ts.readLine();
        if (!line.startsWith(QStringLiteral("Path="))) {
            continue;
        }
        const QString raw = line.mid(5).trimmed();
        const QString decoded = QUrl::fromPercentEncoding(raw.toUtf8());
        return decoded.isEmpty() ? raw : decoded;
    }
    return QString();
}

} // namespace

bool FileManagerApi::isProtectedPath(const QString &path) const
{
    const QString p = expandPath(path);
    if (p.isEmpty()) {
        return false;
    }
    return isProtectedDesktopFolderPathFs(p);
}

QVariantMap FileManagerApi::renamePath(const QString &fromPath, const QString &toPath)
{
    const QString src = expandPath(fromPath);
    const QString dst = expandPath(toPath);
    if (src.isEmpty() || dst.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-path"));
    }
    if (isProtectedDesktopFolderPathFs(src)) {
        return makeResult(false, QStringLiteral("protected-path"),
                          {{QStringLiteral("path"), src}});
    }
    if (!QFileInfo::exists(src)) {
        return makeResult(false, QStringLiteral("not-found"));
    }
    QDir parent = QFileInfo(dst).absoluteDir();
    if (!parent.exists()) {
        parent.mkpath(QStringLiteral("."));
    }
    if (!QFile::rename(src, dst)) {
        return makeResult(false, QStringLiteral("rename-failed"));
    }
    emit pathChanged(src, QStringLiteral("file"));
    emit pathChanged(dst, QStringLiteral("file"));
    emit pathChanged(QFileInfo(src).absolutePath(), QStringLiteral("directory"));
    emit pathChanged(QFileInfo(dst).absolutePath(), QStringLiteral("directory"));
    return makeResult(true, QString(), {{QStringLiteral("from"), src}, {QStringLiteral("to"), dst}});
}

QVariantMap FileManagerApi::removePath(const QString &path, bool recursive)
{
    const QString p = expandPath(path);
    if (isProtectedDesktopFolderPathFs(p)) {
        return makeResult(false, QStringLiteral("protected-path"),
                          {{QStringLiteral("path"), p}});
    }
    QFileInfo fi(p);
    if (!fi.exists()) {
        return makeResult(false, QStringLiteral("not-found"));
    }
    QString err;
    bool ok = false;
    if (fi.isDir() && !fi.isSymLink()) {
        if (!recursive) {
            QDir d(p);
            ok = d.rmdir(p);
        } else {
            ok = removeRecursivelyPathFs(p, &err);
        }
    } else {
        ok = QFile::remove(p);
        if (!ok) {
            err = QStringLiteral("remove-failed");
        }
    }
    if (!ok) {
        return makeResult(false, err.isEmpty() ? QStringLiteral("remove-failed") : err);
    }
    emit pathChanged(p, fi.isDir() ? QStringLiteral("directory") : QStringLiteral("file"));
    emit pathChanged(fi.absolutePath(), QStringLiteral("directory"));
    return makeResult(true, QString(), {{QStringLiteral("path"), p}});
}

QVariantMap FileManagerApi::copyPath(const QString &sourcePath,
                                     const QString &targetPath,
                                     bool overwrite)
{
    Q_UNUSED(overwrite)
    const QString src = expandPath(sourcePath);
    const QString dst = expandPath(targetPath);
    QFileInfo srcInfo(src);
    if (!srcInfo.exists()) {
        return makeResult(false, QStringLiteral("not-found"));
    }

    QString finalDst = dst;
    if (QFileInfo::exists(finalDst)) {
        finalDst = uniqueTargetPathFs(QFileInfo(finalDst).absolutePath(), QFileInfo(finalDst).fileName());
    }

    QString err;
    const bool ok = copyRecursivelyFs(src, finalDst, nullptr, &err);
    if (!ok) {
        return makeResult(false, err.isEmpty() ? QStringLiteral("copy-failed") : err);
    }
    emit pathChanged(finalDst, srcInfo.isDir() ? QStringLiteral("directory") : QStringLiteral("file"));
    emit pathChanged(QFileInfo(finalDst).absolutePath(), QStringLiteral("directory"));
    return makeResult(true, QString(), {{QStringLiteral("from"), src}, {QStringLiteral("to"), finalDst}});
}

QVariantMap FileManagerApi::copyPaths(const QVariantList &sourcePaths,
                                      const QString &targetDirectory,
                                      bool overwrite)
{
    const QStringList paths = uniqueExpandedPathsFs(sourcePaths);
    const QString dir = expandPath(targetDirectory);
    if (paths.isEmpty() || dir.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-path"));
    }
    QVariantMap last = makeResult(true);
    for (const QString &src : paths) {
        const QString dst = uniqueTargetPathFs(dir, QFileInfo(src).fileName());
        last = copyPath(src, dst, overwrite);
        if (!last.value(QStringLiteral("ok")).toBool()) {
            return last;
        }
    }
    return makeResult(true, QString(), {{QStringLiteral("count"), paths.size()}});
}

QVariantMap FileManagerApi::movePath(const QString &sourcePath,
                                     const QString &targetPath,
                                     bool overwrite)
{
    const QString src = expandPath(sourcePath);
    QString dst = expandPath(targetPath);
    if (isProtectedDesktopFolderPathFs(src)) {
        return makeResult(false, QStringLiteral("protected-path"),
                          {{QStringLiteral("path"), src}});
    }
    QFileInfo srcInfo(src);
    if (!srcInfo.exists()) {
        return makeResult(false, QStringLiteral("not-found"));
    }
    if (QFileInfo::exists(dst) && !overwrite) {
        dst = uniqueTargetPathFs(QFileInfo(dst).absolutePath(), QFileInfo(dst).fileName());
    }

    QDir parent = QFileInfo(dst).absoluteDir();
    if (!parent.exists()) {
        parent.mkpath(QStringLiteral("."));
    }

    bool ok = QFile::rename(src, dst);
    if (!ok) {
        const QVariantMap copied = copyPath(src, dst, overwrite);
        if (!copied.value(QStringLiteral("ok")).toBool()) {
            return copied;
        }
        const QVariantMap removed = removePath(src, true);
        if (!removed.value(QStringLiteral("ok")).toBool()) {
            return removed;
        }
    }

    emit pathChanged(src, srcInfo.isDir() ? QStringLiteral("directory") : QStringLiteral("file"));
    emit pathChanged(dst, srcInfo.isDir() ? QStringLiteral("directory") : QStringLiteral("file"));
    emit pathChanged(QFileInfo(src).absolutePath(), QStringLiteral("directory"));
    emit pathChanged(QFileInfo(dst).absolutePath(), QStringLiteral("directory"));
    return makeResult(true, QString(), {{QStringLiteral("from"), src}, {QStringLiteral("to"), dst}});
}

QVariantMap FileManagerApi::removePaths(const QVariantList &paths, bool recursive)
{
    const QStringList list = uniqueExpandedPathsFs(paths);
    if (list.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-path"));
    }
    QString blockedPath;
    if (firstProtectedDesktopPathFs(list, &blockedPath)) {
        return makeResult(false, QStringLiteral("protected-path"),
                          {{QStringLiteral("path"), blockedPath}});
    }
    int removed = 0;
    QVariantMap last = makeResult(true);
    for (const QString &p : list) {
        last = removePath(p, recursive);
        if (!last.value(QStringLiteral("ok")).toBool()) {
            return last;
        }
        ++removed;
    }
    return makeResult(true, QString(), {{QStringLiteral("deletedCount"), removed}});
}

QVariantMap FileManagerApi::startRestoreTrashPaths(const QVariantList &paths, const QString &fallbackDirectory)
{
    if (batchOperationActive()) {
        return makeResult(false, QStringLiteral("batch-busy"));
    }
    const QStringList list = uniqueExpandedPathsFs(paths);
    if (list.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-path"));
    }

    const QString fallbackDir = expandPath(fallbackDirectory);
    if (fallbackDir.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-fallback-directory"));
    }

    QList<BatchTask> tasks;
    tasks.reserve(list.size());
    for (const QString &src : list) {
        QFileInfo srcInfo(src);
        if (!srcInfo.exists()) {
            return makeResult(false, QStringLiteral("not-found"),
                              {{QStringLiteral("path"), src}});
        }

        const QString base = srcInfo.fileName();
        const QString infoPath = QDir::homePath()
                + QStringLiteral("/.local/share/Trash/info/")
                + base + QStringLiteral(".trashinfo");
        const QString originalPath = parseOriginalPathFromTrashInfoFileFs(infoPath);

        QString parentDir = fallbackDir;
        QString fileName = base;
        if (!originalPath.isEmpty()) {
            QFileInfo originalInfo(originalPath);
            const QString parsedName = originalInfo.fileName();
            fileName = parsedName.isEmpty() ? base : parsedName;
            const QString parsedParent = originalInfo.absolutePath();
            parentDir = parsedParent.isEmpty() ? fallbackDir : parsedParent;
        }

        if (!QDir(parentDir).exists()) {
            QDir().mkpath(parentDir);
        }
        const QString target = uniqueTargetPathFs(parentDir, fileName);

        BatchTask t;
        t.sourcePath = srcInfo.absoluteFilePath();
        t.targetPath = target;
        t.auxiliaryPath = QFileInfo::exists(infoPath) ? infoPath : QString();
        t.isDir = srcInfo.isDir();
        t.existedBefore = QFileInfo::exists(target);
        tasks.push_back(t);
    }

    const QString id = QString::number(QDateTime::currentMSecsSinceEpoch());
    startBatchEngine(BatchKind::Restore, id, tasks);
    return makeResult(true, QString(), {{QStringLiteral("id"), id},
                                        {QStringLiteral("operation"), QStringLiteral("restore")},
                                        {QStringLiteral("total"), tasks.size()}});
}
