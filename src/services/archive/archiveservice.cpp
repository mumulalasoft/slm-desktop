#include "archiveservice.h"

#include "archivebackend.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDateTime>
#include <QMutexLocker>
#include <QThread>
#include <QtConcurrent>

namespace Slm::Archive {
namespace {
constexpr const char kService[] = "org.slm.Desktop.Archive";
constexpr const char kPath[] = "/org/slm/Desktop/Archive";
}

ArchiveService::ArchiveService(QObject *parent)
    : QObject(parent)
{
}

ArchiveService::~ArchiveService()
{
    if (!m_registered) {
        return;
    }
    QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(kPath));
    QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(kService));
}

bool ArchiveService::registerService()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusConnectionInterface *iface = bus.interface();
    if (!bus.isConnected() || !iface) {
        m_registered = false;
        emit serviceRegisteredChanged();
        return false;
    }
    if (iface->isServiceRegistered(QString::fromLatin1(kService)).value()) {
        m_registered = false;
        emit serviceRegisteredChanged();
        return false;
    }
    if (!bus.registerService(QString::fromLatin1(kService))) {
        m_registered = false;
        emit serviceRegisteredChanged();
        return false;
    }
    const bool ok = bus.registerObject(QString::fromLatin1(kPath),
                                       this,
                                       QDBusConnection::ExportAllSlots
                                           | QDBusConnection::ExportAllSignals
                                           | QDBusConnection::ExportAllProperties);
    if (!ok) {
        bus.unregisterService(QString::fromLatin1(kService));
        m_registered = false;
        emit serviceRegisteredChanged();
        return false;
    }

    m_registered = true;
    emit serviceRegisteredChanged();
    return true;
}

QVariantMap ArchiveService::Ping() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("service"), QString::fromLatin1(kService)},
        {QStringLiteral("registered"), m_registered}
    };
}

QVariantMap ArchiveService::GetCapabilities() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("service"), QString::fromLatin1(kService)},
        {QStringLiteral("capabilities"), QStringList{
             QStringLiteral("list"),
             QStringLiteral("test"),
             QStringLiteral("extract"),
             QStringLiteral("compress"),
             QStringLiteral("cancel"),
             QStringLiteral("job-status")
        }}
    };
}

QVariantMap ArchiveService::ListArchive(const QString &path, int maxEntries)
{
    return ArchiveBackend::listArchive(path, maxEntries);
}

QVariantMap ArchiveService::TestArchive(const QString &path)
{
    return ArchiveBackend::testArchive(path);
}

QString ArchiveService::nextJobId()
{
    const auto n = ++m_jobCounter;
    return QStringLiteral("arc-%1-%2")
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(static_cast<qulonglong>(n));
}

QVariantMap ArchiveService::jobToStatus(const Job &job) const
{
    QVariantMap out;
    out.insert(QStringLiteral("jobId"), job.id);
    out.insert(QStringLiteral("type"), job.type);
    out.insert(QStringLiteral("state"), job.state);
    out.insert(QStringLiteral("progress"), job.progress);
    out.insert(QStringLiteral("message"), job.userMessage);
    out.insert(QStringLiteral("result"), job.result);
    out.insert(QStringLiteral("error"), job.error);
    out.insert(QStringLiteral("createdAt"), job.createdAt.toString(Qt::ISODate));
    out.insert(QStringLiteral("updatedAt"), job.updatedAt.toString(Qt::ISODate));
    return out;
}

void ArchiveService::updateJobProgress(const QString &jobId, int progress, const QString &message)
{
    QVariantMap status;
    {
        QMutexLocker locker(&m_jobsMutex);
        auto it = m_jobs.find(jobId);
        if (it == m_jobs.end()) {
            return;
        }
        it->progress = qBound(0, progress, 100);
        if (!message.isEmpty()) {
            it->userMessage = message;
        }
        it->updatedAt = QDateTime::currentDateTimeUtc();
        status = jobToStatus(*it);
    }
    emit JobUpdated(jobId, status);
}

void ArchiveService::finishJobSuccess(const QString &jobId, const QVariantMap &result)
{
    QVariantMap status;
    {
        QMutexLocker locker(&m_jobsMutex);
        auto it = m_jobs.find(jobId);
        if (it == m_jobs.end()) {
            return;
        }
        it->state = QStringLiteral("completed");
        it->progress = 100;
        it->result = result;
        it->error.clear();
        it->updatedAt = QDateTime::currentDateTimeUtc();
        status = jobToStatus(*it);
    }
    emit JobUpdated(jobId, status);
    emit JobCompleted(jobId, result);
}

void ArchiveService::finishJobError(const QString &jobId, const QVariantMap &error, bool cancelled)
{
    QVariantMap status;
    {
        QMutexLocker locker(&m_jobsMutex);
        auto it = m_jobs.find(jobId);
        if (it == m_jobs.end()) {
            return;
        }
        it->state = cancelled ? QStringLiteral("cancelled") : QStringLiteral("failed");
        it->error = error;
        it->updatedAt = QDateTime::currentDateTimeUtc();
        status = jobToStatus(*it);
    }
    emit JobUpdated(jobId, status);
    emit JobFailed(jobId, error);
}

QString ArchiveService::ExtractArchive(const QString &path,
                                      const QString &destination,
                                      const QString &mode,
                                      const QString &conflictPolicy)
{
    const QString jobId = nextJobId();
    {
        QMutexLocker locker(&m_jobsMutex);
        Job job;
        job.id = jobId;
        job.type = QStringLiteral("extract");
        job.state = QStringLiteral("pending");
        job.progress = 0;
        job.userMessage = QStringLiteral("Menyiapkan ekstraksi...");
        job.createdAt = QDateTime::currentDateTimeUtc();
        job.updatedAt = job.createdAt;
        job.cancelFlag = std::make_shared<std::atomic_bool>(false);
        m_jobs.insert(jobId, job);
    }
    emit JobUpdated(jobId, GetJobStatus(jobId));

    startExtractJob(jobId, path, destination, mode, conflictPolicy);
    return jobId;
}

QString ArchiveService::CompressPaths(const QStringList &paths,
                                      const QString &destination,
                                      const QString &format)
{
    const QString jobId = nextJobId();
    {
        QMutexLocker locker(&m_jobsMutex);
        Job job;
        job.id = jobId;
        job.type = QStringLiteral("compress");
        job.state = QStringLiteral("pending");
        job.progress = 0;
        job.userMessage = QStringLiteral("Menyiapkan kompresi...");
        job.createdAt = QDateTime::currentDateTimeUtc();
        job.updatedAt = job.createdAt;
        job.cancelFlag = std::make_shared<std::atomic_bool>(false);
        m_jobs.insert(jobId, job);
    }
    emit JobUpdated(jobId, GetJobStatus(jobId));

    startCompressJob(jobId, paths, destination, format);
    return jobId;
}

void ArchiveService::startExtractJob(const QString &jobId,
                                     const QString &path,
                                     const QString &destination,
                                     const QString &mode,
                                     const QString &conflictPolicy)
{
    std::shared_ptr<std::atomic_bool> cancelFlag;
    {
        QMutexLocker locker(&m_jobsMutex);
        auto it = m_jobs.find(jobId);
        if (it == m_jobs.end()) {
            return;
        }
        it->state = QStringLiteral("running");
        it->progress = 1;
        it->userMessage = QStringLiteral("Mengekstrak arsip...");
        it->updatedAt = QDateTime::currentDateTimeUtc();
        cancelFlag = it->cancelFlag;
    }
    emit JobUpdated(jobId, GetJobStatus(jobId));

    (void)QtConcurrent::run([this, jobId, path, destination, mode, conflictPolicy, cancelFlag]() {
        const QVariantMap result = ArchiveBackend::extractArchive(
            path,
            destination,
            mode,
            conflictPolicy,
            cancelFlag.get(),
            [this, jobId](int progress) {
                QMetaObject::invokeMethod(this,
                                          [this, jobId, progress]() {
                                              updateJobProgress(jobId, progress);
                                          },
                                          Qt::QueuedConnection);
            });

        const bool ok = result.value(QStringLiteral("ok")).toBool();
        const bool cancelled = result.value(QStringLiteral("errorCode")).toString() == QStringLiteral("ERR_CANCELLED");
        QMetaObject::invokeMethod(this,
                                  [this, jobId, result, ok, cancelled]() {
                                      if (ok) {
                                          finishJobSuccess(jobId, result);
                                      } else {
                                          finishJobError(jobId, result, cancelled);
                                      }
                                  },
                                  Qt::QueuedConnection);
    });
}

void ArchiveService::startCompressJob(const QString &jobId,
                                      const QStringList &paths,
                                      const QString &destination,
                                      const QString &format)
{
    std::shared_ptr<std::atomic_bool> cancelFlag;
    {
        QMutexLocker locker(&m_jobsMutex);
        auto it = m_jobs.find(jobId);
        if (it == m_jobs.end()) {
            return;
        }
        it->state = QStringLiteral("running");
        it->progress = 1;
        it->userMessage = QStringLiteral("Membuat arsip...");
        it->updatedAt = QDateTime::currentDateTimeUtc();
        cancelFlag = it->cancelFlag;
    }
    emit JobUpdated(jobId, GetJobStatus(jobId));

    (void)QtConcurrent::run([this, jobId, paths, destination, format, cancelFlag]() {
        const QVariantMap result = ArchiveBackend::compressPaths(
            paths,
            destination,
            format,
            cancelFlag.get(),
            [this, jobId](int progress) {
                QMetaObject::invokeMethod(this,
                                          [this, jobId, progress]() {
                                              updateJobProgress(jobId, progress);
                                          },
                                          Qt::QueuedConnection);
            });

        const bool ok = result.value(QStringLiteral("ok")).toBool();
        const bool cancelled = result.value(QStringLiteral("errorCode")).toString() == QStringLiteral("ERR_CANCELLED");
        QMetaObject::invokeMethod(this,
                                  [this, jobId, result, ok, cancelled]() {
                                      if (ok) {
                                          finishJobSuccess(jobId, result);
                                      } else {
                                          finishJobError(jobId, result, cancelled);
                                      }
                                  },
                                  Qt::QueuedConnection);
    });
}

bool ArchiveService::CancelJob(const QString &jobId)
{
    QMutexLocker locker(&m_jobsMutex);
    auto it = m_jobs.find(jobId);
    if (it == m_jobs.end()) {
        return false;
    }
    if (it->cancelFlag) {
        it->cancelFlag->store(true);
    }
    it->userMessage = QStringLiteral("Membatalkan...");
    it->updatedAt = QDateTime::currentDateTimeUtc();
    return true;
}

QVariantMap ArchiveService::GetJobStatus(const QString &jobId) const
{
    QMutexLocker locker(&m_jobsMutex);
    auto it = m_jobs.find(jobId);
    if (it == m_jobs.end()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("errorCode"), QStringLiteral("ERR_JOB_NOT_FOUND")},
            {QStringLiteral("message"), QStringLiteral("Job tidak ditemukan.")}
        };
    }
    QVariantMap out = jobToStatus(*it);
    out.insert(QStringLiteral("ok"), true);
    return out;
}

QVariantList ArchiveService::ListJobs() const
{
    QVariantList out;
    QMutexLocker locker(&m_jobsMutex);
    for (auto it = m_jobs.cbegin(); it != m_jobs.cend(); ++it) {
        out.push_back(jobToStatus(it.value()));
    }
    return out;
}

} // namespace Slm::Archive
