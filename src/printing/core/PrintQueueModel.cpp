#include "PrintQueueModel.h"

namespace Slm::Print {

PrintQueueModel::PrintQueueModel(QObject *parent)
    : QAbstractListModel(parent)
{}

int PrintQueueModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_jobs.size());
}

QVariant PrintQueueModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_jobs.size()))
        return {};

    const PrintJobInfo &job = m_jobs.at(index.row());
    switch (role) {
    case JobHandleRole:         return job.jobHandle;
    case DocumentTitleRole:     return job.documentTitle;
    case PrinterDisplayNameRole:return job.printerDisplayName;
    case StatusRole:            return toString(job.status);
    case StatusDetailRole:      return job.statusDetail;
    case PagesSentRole:         return job.pagesSent;
    case TotalPagesRole:        return job.totalPages;
    case SubmittedAtRole:       return job.submittedAt;
    case ProgressRole: {
        if (job.totalPages <= 0)
            return 0.0;
        return qBound(0.0, static_cast<double>(job.pagesSent) / job.totalPages, 1.0);
    }
    default:
        return {};
    }
}

QHash<int, QByteArray> PrintQueueModel::roleNames() const
{
    return {
        { JobHandleRole,          "jobHandle"          },
        { DocumentTitleRole,      "documentTitle"      },
        { PrinterDisplayNameRole, "printerDisplayName" },
        { StatusRole,             "status"             },
        { StatusDetailRole,       "statusDetail"       },
        { PagesSentRole,          "pagesSent"          },
        { TotalPagesRole,         "totalPages"         },
        { SubmittedAtRole,        "submittedAt"        },
        { ProgressRole,           "progress"           },
    };
}

void PrintQueueModel::upsertJob(const PrintJobInfo &info)
{
    const int existing = indexOfHandle(info.jobHandle);
    if (existing >= 0) {
        m_jobs[existing] = info;
        const QModelIndex idx = index(existing);
        emit dataChanged(idx, idx);
    } else {
        const int row = static_cast<int>(m_jobs.size());
        beginInsertRows({}, row, row);
        m_jobs.append(info);
        endInsertRows();
        emit countChanged();
    }
}

void PrintQueueModel::removeJob(const QString &jobHandle)
{
    const int row = indexOfHandle(jobHandle);
    if (row < 0)
        return;
    beginRemoveRows({}, row, row);
    m_jobs.removeAt(row);
    endRemoveRows();
    emit countChanged();
}

void PrintQueueModel::resetJobs(const QList<PrintJobInfo> &jobs)
{
    beginResetModel();
    m_jobs = jobs;
    endResetModel();
    emit countChanged();
}

void PrintQueueModel::cancelJob(const QString &jobHandle)
{
    emit cancelRequested(jobHandle);
}

void PrintQueueModel::pauseJob(const QString &jobHandle)
{
    emit pauseRequested(jobHandle);
}

void PrintQueueModel::resumeJob(const QString &jobHandle)
{
    emit resumeRequested(jobHandle);
}

int PrintQueueModel::indexOfHandle(const QString &jobHandle) const
{
    for (int i = 0; i < static_cast<int>(m_jobs.size()); ++i) {
        if (m_jobs.at(i).jobHandle == jobHandle)
            return i;
    }
    return -1;
}

} // namespace Slm::Print
