#include "filemanagerapi.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QMetaObject>
#include <QSet>
#include <QThread>
#include <QTimer>

#pragma push_macro("signals")
#undef signals
#include <gio/gio.h>
#pragma pop_macro("signals")

#include <thread>

namespace {

struct GioProgressContextBatch {
    std::function<void(qlonglong, qlonglong)> progress;
};

static void gioProgressCallbackBatch(goffset current_num_bytes,
                                     goffset total_num_bytes,
                                     gpointer user_data)
{
    auto *ctx = static_cast<GioProgressContextBatch *>(user_data);
    if (!ctx || !ctx->progress) {
        return;
    }
    ctx->progress(static_cast<qlonglong>(current_num_bytes),
                  static_cast<qlonglong>(total_num_bytes));
}

static qlonglong estimatePathBytesRecursiveBatch(const QString &path)
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

static bool gioCopyFileBatch(const QString &src,
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

    QFileInfo srcInfo(src);
    if (!srcInfo.exists()) {
        if (error) {
            *error = QStringLiteral("not-found");
        }
        return false;
    }
    if (totalBytes) {
        *totalBytes = srcInfo.isFile() ? qMax<qlonglong>(0, srcInfo.size()) : 0;
    }

    GError *gerr = nullptr;
    GFile *srcFile = g_file_new_for_path(src.toUtf8().constData());
    GFile *dstFile = g_file_new_for_path(dst.toUtf8().constData());
    GFileCopyFlags flags = static_cast<GFileCopyFlags>(G_FILE_COPY_ALL_METADATA);
    if (overwrite) {
        flags = static_cast<GFileCopyFlags>(flags | G_FILE_COPY_OVERWRITE);
    }
    GioProgressContextBatch ctx{progressCb};
    const bool ok = g_file_copy(srcFile,
                                dstFile,
                                flags,
                                nullptr,
                                progressCb ? gioProgressCallbackBatch : nullptr,
                                progressCb ? &ctx : nullptr,
                                &gerr);
    if (!ok && gerr) {
        if (error) {
            *error = QString::fromUtf8(gerr->message);
        }
        g_error_free(gerr);
    }
    if (ok) {
        const qlonglong done = srcInfo.isFile() ? qMax<qlonglong>(0, srcInfo.size()) : 0;
        if (copiedBytes) {
            *copiedBytes = done;
        }
        if (totalBytes && *totalBytes < done) {
            *totalBytes = done;
        }
        if (progressCb) {
            progressCb(done, qMax<qlonglong>(1, done));
        }
    }
    g_object_unref(dstFile);
    g_object_unref(srcFile);
    return ok;
}

static bool gioMovePathBatch(const QString &src,
                             const QString &dst,
                             bool overwrite,
                             const std::function<void(qlonglong, qlonglong)> &progressCb,
                             QString *error)
{
    if (error) {
        error->clear();
    }
    QFileInfo srcInfo(src);
    if (!srcInfo.exists()) {
        if (error) {
            *error = QStringLiteral("not-found");
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
    GioProgressContextBatch ctx{progressCb};
    const bool ok = g_file_move(srcFile,
                                dstFile,
                                flags,
                                nullptr,
                                progressCb ? gioProgressCallbackBatch : nullptr,
                                progressCb ? &ctx : nullptr,
                                &gerr);
    if (!ok && gerr) {
        if (error) {
            *error = QString::fromUtf8(gerr->message);
        }
        g_error_free(gerr);
    }
    if (ok && progressCb) {
        const qlonglong total = qMax<qlonglong>(1, estimatePathBytesRecursiveBatch(dst));
        progressCb(total, total);
    }
    g_object_unref(dstFile);
    g_object_unref(srcFile);
    return ok;
}

static bool gioDeleteSinglePathBatch(const QString &path, QString *error)
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

static bool removeRecursivelyPathBatch(const QString &path, QString *error)
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
            if (!removeRecursivelyPathBatch(entry.absoluteFilePath(), error)) {
                return false;
            }
        }
        if (!gioDeleteSinglePathBatch(path, error)) {
            if (error && error->isEmpty()) {
                *error = QStringLiteral("rmdir-failed");
            }
            return false;
        }
        return true;
    }
    if (!gioDeleteSinglePathBatch(path, error)) {
        if (error && error->isEmpty()) {
            *error = QStringLiteral("remove-failed");
        }
        return false;
    }
    return true;
}

static bool copyRecursivelyBatch(const QString &src,
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

    const qlonglong overallTotal = qMax<qlonglong>(1, estimatePathBytesRecursiveBatch(src));
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
        const bool copiedOk = gioCopyFileBatch(currentSrc,
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

static QString uniqueTargetPathBatch(const QString &dstDir, const QString &name)
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

} // namespace

QList<FileManagerApi::BatchTask> FileManagerApi::buildCopyTasks(const QStringList &sources,
                                                                const QString &targetDirectory,
                                                                bool *ok,
                                                                QString *error) const
{
    QList<BatchTask> tasks;
    if (ok) *ok = false;
    if (error) error->clear();

    const QString dstRoot = expandPath(targetDirectory);
    if (dstRoot.isEmpty() || sources.isEmpty()) {
        if (error) *error = QStringLiteral("invalid-path");
        return tasks;
    }

    QSet<QString> reserved;
    for (const QString &src : sources) {
        QFileInfo fi(src);
        if (!fi.exists()) {
            if (error) *error = QStringLiteral("not-found");
            return {};
        }
        QString dst = uniqueTargetPathBatch(dstRoot, fi.fileName());
        while (reserved.contains(dst)) {
            dst = uniqueTargetPathBatch(dstRoot, fi.fileName());
        }
        reserved.insert(dst);
        BatchTask t;
        t.sourcePath = fi.absoluteFilePath();
        t.targetPath = dst;
        t.isDir = fi.isDir();
        t.existedBefore = QFileInfo::exists(dst);
        tasks.push_back(t);
    }

    if (ok) *ok = true;
    return tasks;
}

QList<FileManagerApi::BatchTask> FileManagerApi::buildDeleteTasks(const QStringList &targets,
                                                                  bool recursive,
                                                                  bool *ok,
                                                                  QString *error) const
{
    Q_UNUSED(recursive)
    QList<BatchTask> tasks;
    if (ok) *ok = false;
    if (error) error->clear();
    if (targets.isEmpty()) {
        if (error) *error = QStringLiteral("invalid-path");
        return tasks;
    }
    for (const QString &p : targets) {
        QFileInfo fi(p);
        if (!fi.exists()) {
            if (error) *error = QStringLiteral("not-found");
            return {};
        }
        BatchTask t;
        t.sourcePath = fi.absoluteFilePath();
        t.targetPath = fi.absoluteFilePath();
        t.isDir = fi.isDir();
        t.existedBefore = true;
        tasks.push_back(t);
    }
    if (ok) *ok = true;
    return tasks;
}

void FileManagerApi::startBatchEngine(BatchKind kind, const QString &id, const QList<BatchTask> &tasks)
{
    m_batchKind = kind;
    m_batchState = BatchState::Preparing;
    m_batchId = id;
    m_batchTasks = tasks;
    m_batchCurrent = 0;
    m_batchTotal = tasks.size();
    m_batchTotalIsBytes = false;
    m_batchError.clear();
    m_batchCreatedPaths.clear();
    m_batchCancelRequested = false;
    m_batchCancelMode = CancelMode::KeepCompleted;
    m_batchOverwrite = false;
    m_batchTaskInFlight = false;
    m_batchPreparingInFlight = false;
    m_batchPaused = false;
    m_failedBatchItems.clear();
    emit failedBatchItemsChanged();
    emit batchOperationStateChanged();

    if (kind == BatchKind::Copy || kind == BatchKind::Move || kind == BatchKind::Restore) {
        const QList<BatchTask> prepTasks = m_batchTasks;
        m_batchPreparingInFlight = true;
        std::thread([this, prepTasks]() {
            qlonglong totalBytes = 0;
            for (const BatchTask &t : prepTasks) {
                if (t.isDir) {
                    totalBytes += estimatePathBytesRecursiveBatch(t.sourcePath);
                } else {
                    totalBytes += qMax<qlonglong>(0, QFileInfo(t.sourcePath).size());
                }
            }
            QMetaObject::invokeMethod(this, [this, totalBytes]() {
                m_batchPreparingInFlight = false;
                if (m_batchKind == BatchKind::None) {
                    return;
                }
                if (m_batchCancelRequested) {
                    finishBatchEngine(false, QStringLiteral("cancelled"));
                    return;
                }
                if (totalBytes > 0) {
                    m_batchTotal = totalBytes;
                    m_batchTotalIsBytes = true;
                }
                m_batchState = m_batchPaused ? BatchState::Paused : BatchState::Running;
                emit batchOperationStateChanged();
                if (!m_batchPaused) {
                    m_batchTimer.start();
                }
            }, Qt::QueuedConnection);
        }).detach();
        return;
    }

    m_batchState = m_batchPaused ? BatchState::Paused : BatchState::Running;
    emit batchOperationStateChanged();
    if (!m_batchPaused) {
        m_batchTimer.start();
    }
}

void FileManagerApi::processBatchStep()
{
    if (m_batchKind == BatchKind::None) {
        m_batchTimer.stop();
        return;
    }
    if (m_batchTaskInFlight) {
        return;
    }
    if (m_batchPreparingInFlight || m_batchState == BatchState::Preparing) {
        return;
    }
    if (m_batchCancelRequested) {
        finishBatchEngine(false, QStringLiteral("cancelled"));
        return;
    }
    if (m_batchPaused) {
        return;
    }
    if (m_batchTasks.isEmpty()) {
        finishBatchEngine(true, QString());
        return;
    }

    const BatchTask task = m_batchTasks.takeFirst();
    const BatchKind kind = m_batchKind;
    const qlonglong taskBase = m_batchCurrent;
    m_batchTaskInFlight = true;
    m_batchState = BatchState::Running;
    m_batchTimer.stop();

    std::thread([this, task, kind, taskBase]() {
        bool ok = true;
        QString err;
        qlonglong bytesDone = 0;
        qlonglong bytesTotal = 0;
        qlonglong lastTaskDone = 0;

        auto reportProgress = [this, kind, &task, taskBase, &lastTaskDone](qlonglong done, qlonglong total) {
            if (kind != BatchKind::Copy && kind != BatchKind::Move && kind != BatchKind::Restore) {
                return;
            }
            const qlonglong boundedTotal = qMax<qlonglong>(1, total);
            const qlonglong boundedDone = qBound<qlonglong>(0, done, boundedTotal);
            if (boundedDone < lastTaskDone) {
                lastTaskDone = 0;
            }
            lastTaskDone = boundedDone;
            QMetaObject::invokeMethod(this, [this, taskBase, boundedDone, boundedTotal]() {
                onBatchTaskProgress(taskBase, boundedDone, boundedTotal);
            }, Qt::QueuedConnection);
            QMetaObject::invokeMethod(this, [this, kind, task, boundedDone, boundedTotal]() {
                emit fileOperationProgress(batchKindToString(kind),
                                           task.sourcePath,
                                           task.targetPath,
                                           boundedDone,
                                           boundedTotal);
            }, Qt::QueuedConnection);
        };

        if (kind == BatchKind::Copy) {
            if (task.isDir) {
                bytesTotal = estimatePathBytesRecursiveBatch(task.sourcePath);
                if (bytesTotal <= 0) {
                    bytesTotal = 1;
                }
                ok = copyRecursivelyBatch(task.sourcePath, task.targetPath, reportProgress, &err);
                if (ok) {
                    bytesDone = bytesTotal;
                    reportProgress(bytesDone, bytesTotal);
                }
            } else {
                ok = gioCopyFileBatch(task.sourcePath,
                                      task.targetPath,
                                      m_batchOverwrite,
                                      &bytesDone,
                                      &bytesTotal,
                                      reportProgress,
                                      &err);
            }
        } else if (kind == BatchKind::Move || kind == BatchKind::Restore) {
            QFileInfo fi(task.sourcePath);
            ok = gioMovePathBatch(task.sourcePath,
                                  task.targetPath,
                                  m_batchOverwrite,
                                  reportProgress,
                                  &err);
            if (!ok) {
                if (fi.isDir()) {
                    bytesTotal = estimatePathBytesRecursiveBatch(task.sourcePath);
                    if (bytesTotal <= 0) {
                        bytesTotal = 1;
                    }
                    ok = copyRecursivelyBatch(task.sourcePath, task.targetPath, reportProgress, &err);
                    if (ok) {
                        bytesDone = bytesTotal;
                        reportProgress(bytesDone, bytesTotal);
                    }
                } else {
                    ok = gioCopyFileBatch(task.sourcePath,
                                          task.targetPath,
                                          m_batchOverwrite,
                                          &bytesDone,
                                          &bytesTotal,
                                          reportProgress,
                                          &err);
                }
                if (ok) {
                    QString rmErr;
                    ok = removeRecursivelyPathBatch(task.sourcePath, &rmErr);
                    if (!ok) {
                        err = rmErr;
                    }
                }
            }
        } else if (kind == BatchKind::Delete) {
            ok = removeRecursivelyPathBatch(task.sourcePath, &err);
        } else if (kind == BatchKind::Trash) {
            GError *gerr = nullptr;
            GFile *file = g_file_new_for_path(task.sourcePath.toUtf8().constData());
            ok = g_file_trash(file, nullptr, &gerr);
            if (!ok && gerr) {
                err = QString::fromUtf8(gerr->message);
                g_error_free(gerr);
            }
            g_object_unref(file);
        }

        QMetaObject::invokeMethod(this, [this, task, kind, ok, err, bytesTotal]() {
            m_batchTaskInFlight = false;
            if (m_batchKind == BatchKind::None) {
                return;
            }

            if (!ok) {
                const QString e = err.isEmpty() ? QStringLiteral("task-failed") : err;
                emit batchOperationTaskError(batchKindToString(kind),
                                             (kind == BatchKind::Delete) ? task.sourcePath : task.targetPath,
                                             e);
                FailedBatchItem f;
                f.task = task;
                f.error = e;
                f.kind = kind;
                m_failedBatchItems.push_back(f);
                emit failedBatchItemsChanged();
                if (!m_batchTotalIsBytes) {
                    ++m_batchCurrent;
                }
            } else {
                if (m_batchTotalIsBytes) {
                    if (kind == BatchKind::Copy || kind == BatchKind::Move || kind == BatchKind::Restore) {
                        qlonglong step = bytesTotal;
                        if (step <= 0 && !task.isDir) {
                            step = QFileInfo(task.sourcePath).size();
                        }
                        if (step <= 0) {
                            step = 1;
                        }
                        m_batchCurrent += step;
                    } else {
                        ++m_batchCurrent;
                    }
                } else {
                    ++m_batchCurrent;
                }

                const QString kindStr = task.isDir ? QStringLiteral("directory") : QStringLiteral("file");
                if (kind == BatchKind::Delete) {
                    emit pathChanged(task.sourcePath, kindStr);
                    emit pathChanged(QFileInfo(task.sourcePath).absolutePath(), QStringLiteral("directory"));
                } else {
                    emit pathChanged(task.targetPath, kindStr);
                    emit pathChanged(QFileInfo(task.targetPath).absolutePath(), QStringLiteral("directory"));
                    if (!task.auxiliaryPath.isEmpty()) {
                        QString auxErr;
                        removeRecursivelyPathBatch(task.auxiliaryPath, &auxErr);
                    }
                }
            }

            if (m_batchTotalIsBytes && m_batchCurrent > m_batchTotal) {
                m_batchCurrent = m_batchTotal;
            }
            if (!m_batchTotalIsBytes) {
                emit fileOperationProgress(batchKindToString(kind),
                                           task.sourcePath,
                                           task.targetPath,
                                           m_batchCurrent,
                                           m_batchTotal);
            }
            emit batchOperationStateChanged();

            if (m_batchCancelRequested) {
                finishBatchEngine(false, QStringLiteral("cancelled"));
                return;
            }
            if (m_batchPaused) {
                return;
            }
            if (m_batchTasks.isEmpty()) {
                finishBatchEngine(true, QString());
                return;
            }
            QTimer::singleShot(0, this, &FileManagerApi::processBatchStep);
        }, Qt::QueuedConnection);
    }).detach();
}

void FileManagerApi::finishBatchEngine(bool ok, const QString &error)
{
    m_batchTimer.stop();
    const QString op = batchOperationType();
    if (ok && m_batchTotalIsBytes) {
        m_batchCurrent = m_batchTotal;
    }
    const qlonglong processed = m_batchCurrent;
    const qlonglong total = m_batchTotal;
    const bool finalOk = ok && m_failedBatchItems.isEmpty();
    QString finalError = error;
    if (finalError.isEmpty() && !m_failedBatchItems.isEmpty()) {
        finalError = QStringLiteral("partial-failed");
    }
    emit fileBatchOperationFinished(op, finalOk, processed, total, finalError);

    m_batchKind = BatchKind::None;
    m_batchId.clear();
    m_batchTasks.clear();
    m_batchCurrent = 0;
    m_batchTotal = 0;
    m_batchError.clear();
    m_batchCreatedPaths.clear();
    m_batchCancelRequested = false;
    m_batchTotalIsBytes = false;
    m_batchTaskInFlight = false;
    m_batchPreparingInFlight = false;
    m_batchPaused = false;
    m_batchState = BatchState::Idle;
    m_daemonFileOpJobId.clear();
    m_daemonFileOpErrored = false;
    m_daemonFileOpError.clear();
    emit batchOperationStateChanged();
}

bool FileManagerApi::rollbackCreatedPaths(CancelMode mode)
{
    Q_UNUSED(mode)
    return true;
}

QString FileManagerApi::uniqueTrashPath(const QString &path) const
{
    const QString trashDir = expandPath(QStringLiteral("~/.local/share/Trash/files"));
    QDir().mkpath(trashDir);
    return uniqueTargetPathBatch(trashDir, QFileInfo(path).fileName());
}
