#pragma once

#include <QObject>
#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QVariantList>
#include <QVariantMap>

#include <atomic>
#include <memory>

namespace Slm::Archive {

class ArchiveService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Desktop.Archive")
    Q_PROPERTY(bool serviceRegistered READ serviceRegistered NOTIFY serviceRegisteredChanged)

public:
    explicit ArchiveService(QObject *parent = nullptr);
    ~ArchiveService() override;

    bool registerService();
    bool serviceRegistered() const { return m_registered; }

public slots:
    QVariantMap Ping() const;
    QVariantMap GetCapabilities() const;

    QVariantMap ListArchive(const QString &path, int maxEntries = 4000);
    QVariantMap TestArchive(const QString &path);

    QString ExtractArchive(const QString &path,
                          const QString &destination,
                          const QString &mode = QStringLiteral("extract_sibling_default"),
                          const QString &conflictPolicy = QStringLiteral("auto_rename"));
    QString CompressPaths(const QStringList &paths,
                          const QString &destination,
                          const QString &format = QStringLiteral("zip"));

    bool CancelJob(const QString &jobId);
    QVariantMap GetJobStatus(const QString &jobId) const;
    QVariantList ListJobs() const;

signals:
    void serviceRegisteredChanged();
    void JobUpdated(const QString &jobId, const QVariantMap &status);
    void JobCompleted(const QString &jobId, const QVariantMap &result);
    void JobFailed(const QString &jobId, const QVariantMap &error);

private:
    struct Job {
        QString id;
        QString type;
        QString state; // pending|running|completed|failed|cancelled
        int progress = 0;
        QString userMessage;
        QVariantMap result;
        QVariantMap error;
        QDateTime createdAt;
        QDateTime updatedAt;
        std::shared_ptr<std::atomic_bool> cancelFlag;
    };

    QString nextJobId();
    void startExtractJob(const QString &jobId,
                         const QString &path,
                         const QString &destination,
                         const QString &mode,
                         const QString &conflictPolicy);
    void startCompressJob(const QString &jobId,
                          const QStringList &paths,
                          const QString &destination,
                          const QString &format);

    QVariantMap jobToStatus(const Job &job) const;
    void updateJobProgress(const QString &jobId, int progress, const QString &message = QString());
    void finishJobSuccess(const QString &jobId, const QVariantMap &result);
    void finishJobError(const QString &jobId, const QVariantMap &error, bool cancelled = false);

    mutable QMutex m_jobsMutex;
    QHash<QString, Job> m_jobs;
    std::atomic_ulong m_jobCounter {0};

    bool m_registered = false;
};

} // namespace Slm::Archive
