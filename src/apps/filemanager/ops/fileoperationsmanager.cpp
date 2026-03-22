#include "fileoperationsmanager.h"
#include "fileoperationserrors.h"
#include "../../../../urlutils.h"

#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QThread>
#include <QUrl>
#include <QUuid>
#include <QtConcurrent>

namespace {
constexpr int kMaxJobHistory = 128;

QString nowIso()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

QString statePreparing() { return QStringLiteral("preparing"); }
QString stateRunning() { return QStringLiteral("running"); }
QString statePaused() { return QStringLiteral("paused"); }
QString stateCancelling() { return QStringLiteral("cancelling"); }
QString stateFinished() { return QStringLiteral("finished"); }
QString stateError() { return QStringLiteral("error"); }
}

FileOperationsManager::FileOperationsManager(QObject *parent)
    : QObject(parent)
{
}

FileOperationsManager::~FileOperationsManager()
{
    {
        QMutexLocker locker(&m_jobsMutex);
        for (auto it = m_jobs.begin(); it != m_jobs.end(); ++it) {
            if (it.value()) {
                it.value()->cancelled.store(true);
                it.value()->paused.store(false);
            }
        }
    }
    m_workerSync.waitForFinished();
}

QString FileOperationsManager::Copy(const QStringList &uris, const QString &destination)
{
    return startCopyJob(uris, destination, false);
}

QString FileOperationsManager::Move(const QStringList &uris, const QString &destination)
{
    return startCopyJob(uris, destination, true);
}

QString FileOperationsManager::Delete(const QStringList &uris)
{
    return startDeleteJob(uris, false);
}

QString FileOperationsManager::Trash(const QStringList &uris)
{
    return startDeleteJob(uris, true);
}

QString FileOperationsManager::EmptyTrash()
{
    return startEmptyTrashJob();
}

bool FileOperationsManager::Pause(const QString &id)
{
    const QString key = id.trimmed();
    const auto job = findJob(key);
    if (!job) {
        return false;
    }
    job->paused.store(true);
    markJobState(key, statePaused());
    return true;
}

bool FileOperationsManager::Resume(const QString &id)
{
    const QString key = id.trimmed();
    const auto job = findJob(key);
    if (!job) {
        return false;
    }
    job->paused.store(false);
    markJobState(key, stateRunning());
    return true;
}

bool FileOperationsManager::Cancel(const QString &id)
{
    const auto job = findJob(id.trimmed());
    if (!job) {
        return false;
    }
    job->cancelled.store(true);
    job->paused.store(false);
    markJobState(id.trimmed(), stateCancelling());
    return true;
}

QVariantMap FileOperationsManager::GetJob(const QString &id) const
{
    const QString key = id.trimmed();
    if (key.isEmpty()) {
        return QVariantMap();
    }
    QMutexLocker locker(&m_jobsMutex);
    return m_jobSnapshots.value(key);
}

QVariantList FileOperationsManager::ListJobs() const
{
    QVariantList out;
    QMutexLocker locker(&m_jobsMutex);
    out.reserve(m_jobOrder.size());
    for (const QString &id : m_jobOrder) {
        const QVariantMap row = m_jobSnapshots.value(id);
        if (!row.isEmpty()) {
            out.push_back(row);
        }
    }
    return out;
}

QString FileOperationsManager::startCopyJob(const QStringList &uris,
                                            const QString &destination,
                                            bool moveAfterCopy)
{
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    upsertJobSnapshot(id,
                      makeJobSnapshot(id,
                                      moveAfterCopy ? QStringLiteral("move")
                                                    : QStringLiteral("copy"),
                                      qMax(1, uris.size())));
    const QString dstDir = normalizeUriOrPath(destination);
    const QFileInfo dstInfo(dstDir);
    if (!dstInfo.exists() || !dstInfo.isDir()) {
        emitErrorAsync(id,
                       QString::fromLatin1(SlmFileOperationErrors::kInvalidDestination),
                       destination);
        emitFinishedAsync(id);
        return id;
    }

    QStringList sources;
    sources.reserve(uris.size());
    for (const QString &uri : uris) {
        const QString local = normalizeUriOrPath(uri);
        if (!local.isEmpty()) {
            sources.push_back(local);
        }
    }
    if (sources.isEmpty()) {
        markJobState(id, stateFinished());
        emitFinishedAsync(id);
        return id;
    }
    if (moveAfterCopy) {
        for (const QString &src : sources) {
            QString reason;
            if (isProtectedDestructiveTarget(src, &reason)) {
                emitErrorAsync(id,
                               QString::fromLatin1(SlmFileOperationErrors::kProtectedTarget),
                               reason);
                emitFinishedAsync(id);
                return id;
            }
        }
    }
    const auto job = registerJob(id, sources.size());
    upsertJobSnapshot(id, QVariantMap{{QStringLiteral("total"), sources.size()}});

    m_workerSync.addFuture(QtConcurrent::run([this, id, sources, dstDir, moveAfterCopy, job]() {
        markJobState(id, stateRunning());
        updateJobProgress(id, job, 0, sources.size());
        const int total = sources.size();
        int processed = 0;
        bool ok = true;
        for (const QString &src : sources) {
            if (!waitIfPausedOrCancelled(job)) {
                ok = false;
                break;
            }
            const QFileInfo srcInfo(src);
            if (!srcInfo.exists()) {
                ok = false;
                break;
            }
            const QString name = srcInfo.fileName();
            const QString targetPath = ensureUniquePath(dstDir, name);
            bool stepOk = false;
            if (moveAfterCopy) {
                stepOk = movePath(src, targetPath);
            } else {
                stepOk = copyPathRecursively(src, targetPath);
            }
            if (!stepOk) {
                ok = false;
                break;
            }
            processed += 1;
            updateJobProgress(id, job, processed, total);
        }
        if (job->cancelled.load()) {
            emitErrorAsync(id,
                           QString::fromLatin1(SlmFileOperationErrors::kCancelled),
                           QString::fromLatin1(SlmFileOperationErrors::kJobCancelled));
        } else if (!ok) {
            emitErrorAsync(id,
                           QString::fromLatin1(SlmFileOperationErrors::kCopyMoveFailed),
                           QString::fromLatin1(SlmFileOperationErrors::kCopyOrMoveFailed));
        } else if (processed < total) {
            updateJobProgress(id, job, total, total);
        }
        emitFinishedAsync(id);
        unregisterJob(id);
    }));

    return id;
}

QString FileOperationsManager::startDeleteJob(const QStringList &uris, bool toTrash)
{
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    upsertJobSnapshot(id,
                      makeJobSnapshot(id,
                                      toTrash ? QStringLiteral("trash")
                                              : QStringLiteral("delete"),
                                      qMax(1, uris.size())));
    QStringList sources;
    sources.reserve(uris.size());
    for (const QString &uri : uris) {
        const QString local = normalizeUriOrPath(uri);
        if (!local.isEmpty()) {
            sources.push_back(local);
        }
    }
    if (sources.isEmpty()) {
        markJobState(id, stateFinished());
        emitFinishedAsync(id);
        return id;
    }
    for (const QString &path : sources) {
        QString reason;
        if (isProtectedDestructiveTarget(path, &reason)) {
            emitErrorAsync(id,
                           QString::fromLatin1(SlmFileOperationErrors::kProtectedTarget),
                           reason);
            emitFinishedAsync(id);
            return id;
        }
    }
    const auto job = registerJob(id, sources.size());
    upsertJobSnapshot(id, QVariantMap{{QStringLiteral("total"), sources.size()}});

    m_workerSync.addFuture(QtConcurrent::run([this, id, sources, toTrash, job]() {
        markJobState(id, stateRunning());
        updateJobProgress(id, job, 0, sources.size());
        const int total = sources.size();
        int processed = 0;
        bool ok = true;
        for (const QString &path : sources) {
            if (!waitIfPausedOrCancelled(job)) {
                ok = false;
                break;
            }
            bool stepOk = toTrash ? trashPath(path) : removePathRecursively(path);
            if (!stepOk) {
                ok = false;
                break;
            }
            processed += 1;
            updateJobProgress(id, job, processed, total);
        }
        if (job->cancelled.load()) {
            emitErrorAsync(id,
                           QString::fromLatin1(SlmFileOperationErrors::kCancelled),
                           QString::fromLatin1(SlmFileOperationErrors::kJobCancelled));
        } else if (!ok) {
            emitErrorAsync(id,
                           QString::fromLatin1(SlmFileOperationErrors::kDeleteTrashFailed),
                           QString::fromLatin1(SlmFileOperationErrors::kDeleteOrTrashFailed));
        } else if (processed < total) {
            updateJobProgress(id, job, total, total);
        }
        emitFinishedAsync(id);
        unregisterJob(id);
    }));

    return id;
}

QString FileOperationsManager::startEmptyTrashJob()
{
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    upsertJobSnapshot(id, makeJobSnapshot(id, QStringLiteral("empty-trash"), 1));
    const QString trashRoot = QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
            + QStringLiteral("/.local/share/Trash");
    const QString filesDir = trashRoot + QStringLiteral("/files");
    const QString infoDir = trashRoot + QStringLiteral("/info");

    m_workerSync.addFuture(QtConcurrent::run([this, id, filesDir, infoDir]() {
        bool ok = true;

        QDir files(filesDir);
        const QStringList fileEntries = files.entryList(QDir::NoDotAndDotDot | QDir::AllEntries);
        QDir info(infoDir);
        const QStringList infoEntries = info.entryList(QStringList() << QStringLiteral("*.trashinfo"),
                                                       QDir::NoDotAndDotDot | QDir::Files);
        const int total = qMax(1, fileEntries.size() + infoEntries.size());
        int processed = 0;
        const auto job = registerJob(id, total);
        upsertJobSnapshot(id, QVariantMap{{QStringLiteral("total"), total}});
        markJobState(id, stateRunning());
        updateJobProgress(id, job, 0, total);
        for (const QString &entry : fileEntries) {
            if (!waitIfPausedOrCancelled(job)) {
                ok = false;
                break;
            }
            if (!removePathRecursively(files.absoluteFilePath(entry))) {
                ok = false;
                break;
            }
            processed += 1;
            updateJobProgress(id, job, processed, total);
        }

        if (ok) {
            for (const QString &entry : infoEntries) {
                if (!waitIfPausedOrCancelled(job)) {
                    ok = false;
                    break;
                }
                if (!QFile::remove(info.absoluteFilePath(entry))) {
                    ok = false;
                    break;
                }
                processed += 1;
                updateJobProgress(id, job, processed, total);
            }
        }

        if (job->cancelled.load()) {
            emitErrorAsync(id,
                           QString::fromLatin1(SlmFileOperationErrors::kCancelled),
                           QString::fromLatin1(SlmFileOperationErrors::kJobCancelled));
        } else if (!ok) {
            emitErrorAsync(id,
                           QString::fromLatin1(SlmFileOperationErrors::kEmptyTrashFailed),
                           QString::fromLatin1(SlmFileOperationErrors::kEmptyTrashFailed));
        } else {
            updateJobProgress(id, job, total, total);
        }
        emitFinishedAsync(id);
        unregisterJob(id);
    }));

    return id;
}

QString FileOperationsManager::normalizeUriOrPath(const QString &value)
{
    const QString s = value.trimmed();
    if (s.isEmpty()) {
        return QString();
    }
    return UrlUtils::localPathFromUriOrPath(s);
}

QString FileOperationsManager::ensureUniquePath(const QString &baseDir, const QString &name)
{
    QString candidate = QDir(baseDir).filePath(name);
    if (!QFileInfo::exists(candidate)) {
        return candidate;
    }

    QFileInfo fi(name);
    const QString stem = fi.completeBaseName();
    const QString suffix = fi.completeSuffix();
    int i = 2;
    while (true) {
        QString nextName = QStringLiteral("%1 (%2)").arg(stem).arg(i);
        if (!suffix.isEmpty()) {
            nextName += QStringLiteral(".") + suffix;
        }
        candidate = QDir(baseDir).filePath(nextName);
        if (!QFileInfo::exists(candidate)) {
            return candidate;
        }
        i += 1;
    }
}

bool FileOperationsManager::isProtectedDestructiveTarget(const QString &path, QString *reason)
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty()) {
        if (reason) {
            *reason = QStringLiteral("empty-target");
        }
        return true;
    }

    const QUrl url = QUrl::fromUserInput(trimmed);
    if (url.isValid() && !url.scheme().isEmpty() && !url.isLocalFile()) {
        if (reason) {
            *reason = QStringLiteral("non-local-target:") + trimmed;
        }
        return true;
    }

    auto isAtOrUnder = [](const QString &candidate, const QString &root) {
        const QString c = QDir::cleanPath(candidate);
        const QString r = QDir::cleanPath(root);
        if (r == QStringLiteral("/")) {
            return c == r;
        }
        return c == r || c.startsWith(r + QLatin1Char('/'));
    };

    auto checkCritical = [&](const QString &candidate, const QString &prefix) {
        const QString clean = QDir::cleanPath(candidate);
        const QString homeRoot = QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
        const QStringList exactCritical = {
            QStringLiteral("/"),
            QStringLiteral("/boot"),
            QStringLiteral("/boot/efi"),
            QStringLiteral("/home"),
            homeRoot,
            QStringLiteral("/root"),
            QStringLiteral("/usr"),
            QStringLiteral("/etc"),
            QStringLiteral("/var"),
            QStringLiteral("/bin"),
            QStringLiteral("/sbin"),
            QStringLiteral("/lib"),
            QStringLiteral("/lib64"),
            QStringLiteral("/opt"),
            QStringLiteral("/dev"),
            QStringLiteral("/proc"),
            QStringLiteral("/sys"),
            QStringLiteral("/run"),
        };
        for (const QString &exact : exactCritical) {
            if (clean == exact) {
                if (reason) {
                    *reason = prefix + clean;
                }
                return true;
            }
        }

        const QStringList subtreeCritical = {
            QStringLiteral("/boot"),
            QStringLiteral("/boot/efi"),
            QStringLiteral("/root"),
            QStringLiteral("/usr"),
            QStringLiteral("/etc"),
            QStringLiteral("/var"),
            QStringLiteral("/bin"),
            QStringLiteral("/sbin"),
            QStringLiteral("/lib"),
            QStringLiteral("/lib64"),
            QStringLiteral("/opt"),
            QStringLiteral("/dev"),
            QStringLiteral("/proc"),
            QStringLiteral("/sys"),
            QStringLiteral("/run"),
        };
        for (const QString &root : subtreeCritical) {
            if (isAtOrUnder(clean, root)) {
                if (reason) {
                    *reason = prefix + clean;
                }
                return true;
            }
        }
        return false;
    };

    QFileInfo fi(trimmed);
    QString candidate = fi.absoluteFilePath();
    if (!candidate.startsWith(QLatin1Char('/'))) {
        candidate = QFileInfo(candidate).absoluteFilePath();
    }
    candidate = QDir::cleanPath(candidate);
    if (checkCritical(candidate, QStringLiteral("critical-path:"))) {
        return true;
    }

    if (fi.exists()) {
        const QString canonical = QDir::cleanPath(fi.canonicalFilePath());
        if (!canonical.isEmpty()
                && canonical != candidate
                && checkCritical(canonical, QStringLiteral("canonical-critical-path:"))) {
            return true;
        }
    }

    if (fi.isSymLink()) {
        QString symTarget = fi.symLinkTarget();
        if (!symTarget.isEmpty()) {
            if (!symTarget.startsWith(QLatin1Char('/'))) {
                symTarget = QFileInfo(fi.absolutePath(), symTarget).absoluteFilePath();
            }
            symTarget = QDir::cleanPath(symTarget);
            if (checkCritical(symTarget, QStringLiteral("symlink-to-critical:"))) {
                return true;
            }
        }
    }
    return false;
}

bool FileOperationsManager::copyPathRecursively(const QString &srcPath, const QString &dstPath)
{
    QFileInfo srcInfo(srcPath);
    if (!srcInfo.exists()) {
        return false;
    }
    if (srcInfo.isDir()) {
        QDir dstDir(dstPath);
        if (!dstDir.mkpath(QStringLiteral("."))) {
            return false;
        }
        QDir srcDir(srcPath);
        const QFileInfoList entries = srcDir.entryInfoList(QDir::NoDotAndDotDot
                                                            | QDir::AllEntries
                                                            | QDir::Hidden
                                                            | QDir::System);
        for (const QFileInfo &entry : entries) {
            const QString childDst = QDir(dstPath).filePath(entry.fileName());
            if (!copyPathRecursively(entry.absoluteFilePath(), childDst)) {
                return false;
            }
        }
        return true;
    }
    return QFile::copy(srcPath, dstPath);
}

bool FileOperationsManager::removePathRecursively(const QString &path)
{
    QFileInfo info(path);
    if (!info.exists()) {
        return true;
    }
    // Never recurse through symlinked directories: remove link only.
    if (info.isSymLink()) {
        return QFile::remove(path);
    }
    if (info.isDir()) {
        QDir dir(path);
        return dir.removeRecursively();
    }
    return QFile::remove(path);
}

bool FileOperationsManager::movePath(const QString &srcPath, const QString &dstPath)
{
    QDir dstParent(QFileInfo(dstPath).absolutePath());
    if (!dstParent.exists() && !dstParent.mkpath(QStringLiteral("."))) {
        return false;
    }
    if (QFile::rename(srcPath, dstPath)) {
        return true;
    }
    if (!copyPathRecursively(srcPath, dstPath)) {
        return false;
    }
    return removePathRecursively(srcPath);
}

bool FileOperationsManager::trashPath(const QString &path)
{
    return QFile::moveToTrash(path);
}

void FileOperationsManager::emitProgressAsync(const QString &id, int percent)
{
    const int clamped = qMax(0, qMin(100, percent));
    QMetaObject::invokeMethod(this, [this, id, clamped]() {
        emit Progress(id, clamped);
    }, Qt::QueuedConnection);
}

std::shared_ptr<FileOperationsManager::JobControl> FileOperationsManager::registerJob(const QString &id, int total)
{
    auto job = std::make_shared<JobControl>();
    job->total.store(qMax(1, total));
    QMutexLocker locker(&m_jobsMutex);
    m_jobs.insert(id, job);
    return job;
}

std::shared_ptr<FileOperationsManager::JobControl> FileOperationsManager::findJob(const QString &id) const
{
    QMutexLocker locker(&m_jobsMutex);
    return m_jobs.value(id);
}

void FileOperationsManager::unregisterJob(const QString &id)
{
    QMutexLocker locker(&m_jobsMutex);
    m_jobs.remove(id);
    pruneJobHistoryLocked();
}

bool FileOperationsManager::waitIfPausedOrCancelled(const std::shared_ptr<JobControl> &job) const
{
    while (job && job->paused.load()) {
        if (job->cancelled.load()) {
            return false;
        }
        QThread::msleep(40);
    }
    return !(job && job->cancelled.load());
}

void FileOperationsManager::updateJobProgress(const QString &id,
                                              const std::shared_ptr<JobControl> &job,
                                              int current,
                                              int total)
{
    if (!job) {
        return;
    }
    const int safeTotal = qMax(1, total);
    const int safeCurrent = qBound(0, current, safeTotal);
    job->total.store(safeTotal);
    job->current.store(safeCurrent);
    upsertJobSnapshot(id,
                      QVariantMap{
                          {QStringLiteral("current"), safeCurrent},
                          {QStringLiteral("total"), safeTotal},
                          {QStringLiteral("percent"), (safeCurrent * 100) / safeTotal},
                          {QStringLiteral("updated_at"), nowIso()},
                      });
    emitProgressDetailAsync(id, safeCurrent, safeTotal);
    emitProgressAsync(id, (safeCurrent * 100) / safeTotal);
}

void FileOperationsManager::emitProgressDetailAsync(const QString &id, int current, int total)
{
    const int safeTotal = qMax(1, total);
    const int safeCurrent = qBound(0, current, safeTotal);
    QMetaObject::invokeMethod(this, [this, id, safeCurrent, safeTotal]() {
        emit ProgressDetail(id, safeCurrent, safeTotal);
    }, Qt::QueuedConnection);
}

void FileOperationsManager::emitFinishedAsync(const QString &id)
{
    QMetaObject::invokeMethod(this, [this, id]() {
        const QVariantMap row = GetJob(id);
        const QString state = row.value(QStringLiteral("state")).toString();
        if (state != stateError()) {
            markJobState(id, stateFinished());
        }
        upsertJobSnapshot(id, QVariantMap{{QStringLiteral("updated_at"), nowIso()}});
        emit Finished(id);
    }, Qt::QueuedConnection);
}

void FileOperationsManager::emitErrorAsync(const QString &id,
                                           const QString &code,
                                           const QString &message)
{
    QMetaObject::invokeMethod(this, [this, id, code, message]() {
        const QString normalizedCode = code.trimmed().isEmpty()
                ? QString::fromLatin1(SlmFileOperationErrors::kOperationFailed)
                : code;
        markJobState(id, stateError());
        upsertJobSnapshot(id,
                          QVariantMap{
                              {QStringLiteral("error_code"), normalizedCode},
                              {QStringLiteral("error_message"), message},
                              {QStringLiteral("updated_at"), nowIso()},
                          });
        emit Error(id);
        emit ErrorDetail(id, normalizedCode, message);
    }, Qt::QueuedConnection);
}

QVariantMap FileOperationsManager::makeJobSnapshot(const QString &id,
                                                   const QString &operation,
                                                   int total) const
{
    return QVariantMap{
        {QStringLiteral("id"), id},
        {QStringLiteral("operation"), operation},
        {QStringLiteral("state"), statePreparing()},
        {QStringLiteral("current"), 0},
        {QStringLiteral("total"), qMax(1, total)},
        {QStringLiteral("percent"), 0},
        {QStringLiteral("error_code"), QString()},
        {QStringLiteral("error_message"), QString()},
        {QStringLiteral("created_at"), nowIso()},
        {QStringLiteral("updated_at"), nowIso()},
    };
}

void FileOperationsManager::upsertJobSnapshot(const QString &id, const QVariantMap &delta)
{
    if (id.trimmed().isEmpty()) {
        return;
    }
    QMutexLocker locker(&m_jobsMutex);
    QVariantMap row = m_jobSnapshots.value(id);
    if (row.isEmpty()) {
        row.insert(QStringLiteral("id"), id);
        m_jobOrder.removeAll(id);
        m_jobOrder.push_back(id);
    }
    for (auto it = delta.cbegin(); it != delta.cend(); ++it) {
        row.insert(it.key(), it.value());
    }
    m_jobSnapshots.insert(id, row);
    pruneJobHistoryLocked();
    notifyJobsChangedAsync();
}

void FileOperationsManager::markJobState(const QString &id, const QString &state)
{
    if (id.trimmed().isEmpty() || state.trimmed().isEmpty()) {
        return;
    }
    upsertJobSnapshot(id,
                      QVariantMap{
                          {QStringLiteral("state"), state},
                          {QStringLiteral("updated_at"), nowIso()},
                      });
}

void FileOperationsManager::pruneJobHistoryLocked()
{
    while (m_jobOrder.size() > kMaxJobHistory) {
        const QString oldest = m_jobOrder.takeFirst();
        m_jobSnapshots.remove(oldest);
    }
}

void FileOperationsManager::notifyJobsChangedAsync()
{
    bool expected = false;
    if (!m_jobsChangedQueued.compare_exchange_strong(expected, true)) {
        return;
    }
    QMetaObject::invokeMethod(this, [this]() {
        m_jobsChangedQueued.store(false);
        emit JobsChanged();
    }, Qt::QueuedConnection);
}
