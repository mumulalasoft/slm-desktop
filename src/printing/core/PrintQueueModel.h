#pragma once

#include "PrintTypes.h"

#include <QAbstractListModel>
#include <QDateTime>
#include <QHash>
#include <QString>
#include <QVariantMap>

namespace Slm::Print {

// Holds the data for a single print job in the queue.
struct PrintJobInfo {
    QString       jobHandle;            // opaque — do not parse or display raw
    QString       documentTitle;
    QString       printerDisplayName;
    PrintJobStatus status       = PrintJobStatus::Unknown;
    QString       statusDetail;         // user-facing detail message (may be empty)
    int           pagesSent     = 0;
    int           totalPages    = 0;
    QDateTime     submittedAt;
};

// PrintQueueModel — QAbstractListModel over the active and recent print jobs.
//
// Roles map PrintJobInfo fields to QML-accessible names.
// Job control methods (cancel/pause/resume) delegate to IBackendAdapter
// via the JobOrchestrator — this model does not talk to CUPS directly.
class PrintQueueModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        JobHandleRole = Qt::UserRole + 1,
        DocumentTitleRole,
        PrinterDisplayNameRole,
        StatusRole,          // string: "queued", "printing", "complete", etc.
        StatusDetailRole,    // user-facing detail text
        PagesSentRole,
        TotalPagesRole,
        SubmittedAtRole,
        ProgressRole,        // 0.0–1.0, computed from pagesSent/totalPages
    };
    Q_ENUM(Roles)

    explicit PrintQueueModel(QObject *parent = nullptr);

    // QAbstractListModel interface
    int      rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Add or update a job. If a job with the same jobHandle already exists,
    // its fields are updated in-place. Otherwise a new row is appended.
    void upsertJob(const PrintJobInfo &info);

    // Remove the job with the given jobHandle from the model.
    void removeJob(const QString &jobHandle);

    // Replace the entire job list (e.g. on a full queue refresh).
    void resetJobs(const QList<PrintJobInfo> &jobs);

    // ── Job control (dispatched to the backend via the orchestrator) ───────
    Q_INVOKABLE void cancelJob(const QString &jobHandle);
    Q_INVOKABLE void pauseJob(const QString &jobHandle);
    Q_INVOKABLE void resumeJob(const QString &jobHandle);

signals:
    void countChanged();
    void cancelRequested(const QString &jobHandle);
    void pauseRequested(const QString &jobHandle);
    void resumeRequested(const QString &jobHandle);

private:
    int indexOfHandle(const QString &jobHandle) const;

    QList<PrintJobInfo> m_jobs;
};

} // namespace Slm::Print
