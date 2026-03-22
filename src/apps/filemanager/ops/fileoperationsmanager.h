#pragma once

#include <QObject>
#include <QHash>
#include <QMutex>
#include <QFutureSynchronizer>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <atomic>
#include <memory>

class FileOperationsManager : public QObject
{
    Q_OBJECT

public:
    explicit FileOperationsManager(QObject *parent = nullptr);
    ~FileOperationsManager() override;

    QString Copy(const QStringList &uris, const QString &destination);
    QString Move(const QStringList &uris, const QString &destination);
    QString Delete(const QStringList &uris);
    QString Trash(const QStringList &uris);
    QString EmptyTrash();
    bool Pause(const QString &id);
    bool Resume(const QString &id);
    bool Cancel(const QString &id);
    QVariantMap GetJob(const QString &id) const;
    QVariantList ListJobs() const;

signals:
    void JobsChanged();
    void Progress(const QString &id, int percent);
    void ProgressDetail(const QString &id, int current, int total);
    void Finished(const QString &id);
    void Error(const QString &id);
    void ErrorDetail(const QString &id, const QString &code, const QString &message);

private:
    struct JobControl {
        std::atomic_bool paused{false};
        std::atomic_bool cancelled{false};
        std::atomic_int current{0};
        std::atomic_int total{1};
    };

    QString startCopyJob(const QStringList &uris, const QString &destination, bool moveAfterCopy);
    QString startDeleteJob(const QStringList &uris, bool toTrash);
    QString startEmptyTrashJob();

    static QString normalizeUriOrPath(const QString &value);
    static QString ensureUniquePath(const QString &baseDir, const QString &name);
    static bool isProtectedDestructiveTarget(const QString &path, QString *reason = nullptr);
    static bool copyPathRecursively(const QString &srcPath, const QString &dstPath);
    static bool removePathRecursively(const QString &path);
    static bool movePath(const QString &srcPath, const QString &dstPath);
    static bool trashPath(const QString &path);

    std::shared_ptr<JobControl> registerJob(const QString &id, int total);
    std::shared_ptr<JobControl> findJob(const QString &id) const;
    void unregisterJob(const QString &id);
    bool waitIfPausedOrCancelled(const std::shared_ptr<JobControl> &job) const;
    void updateJobProgress(const QString &id,
                           const std::shared_ptr<JobControl> &job,
                           int current,
                           int total);
    void emitProgressAsync(const QString &id, int percent);
    void emitProgressDetailAsync(const QString &id, int current, int total);
    void emitFinishedAsync(const QString &id);
    void emitErrorAsync(const QString &id,
                        const QString &code = QStringLiteral("operation-failed"),
                        const QString &message = QString());

    QVariantMap makeJobSnapshot(const QString &id,
                                const QString &operation,
                                int total) const;
    void upsertJobSnapshot(const QString &id, const QVariantMap &delta);
    void markJobState(const QString &id, const QString &state);
    void pruneJobHistoryLocked();
    void notifyJobsChangedAsync();

    mutable QMutex m_jobsMutex;
    QHash<QString, std::shared_ptr<JobControl>> m_jobs;
    QHash<QString, QVariantMap> m_jobSnapshots;
    QStringList m_jobOrder;
    std::atomic_bool m_jobsChangedQueued{false};
    QFutureSynchronizer<void> m_workerSync;
};
