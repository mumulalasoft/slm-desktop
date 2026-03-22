#include "filemanagerapi.h"

void FileManagerApi::startDaemonBatchJob(BatchKind kind,
                                         const QString &jobId,
                                         qlonglong total,
                                         bool overwrite)
{
    m_batchKind = kind;
    m_batchState = BatchState::Running;
    m_batchId = jobId;
    m_batchTasks.clear();
    m_batchCurrent = 0;
    m_batchTotal = qMax<qlonglong>(0, total);
    m_batchError.clear();
    m_batchCreatedPaths.clear();
    m_batchCancelRequested = false;
    m_batchCancelMode = CancelMode::KeepCompleted;
    m_batchOverwrite = overwrite;
    m_batchTotalIsBytes = false;
    m_batchTaskInFlight = false;
    m_batchPreparingInFlight = false;
    m_batchPaused = false;
    m_daemonFileOpJobId = jobId;
    m_daemonFileOpErrored = false;
    m_daemonFileOpError.clear();
    m_failedBatchItems.clear();
    emit failedBatchItemsChanged();
    emit batchOperationStateChanged();
}

void FileManagerApi::startPreparingBatchJob(BatchKind kind,
                                            const QString &id,
                                            bool overwrite)
{
    m_batchKind = kind;
    m_batchState = BatchState::Preparing;
    m_batchId = id;
    m_batchTasks.clear();
    m_batchCurrent = 0;
    m_batchTotal = 0;
    m_batchError.clear();
    m_batchCreatedPaths.clear();
    m_batchCancelRequested = false;
    m_batchCancelMode = CancelMode::KeepCompleted;
    m_batchOverwrite = overwrite;
    m_batchTotalIsBytes = false;
    m_batchTaskInFlight = false;
    m_batchPreparingInFlight = true;
    m_batchPaused = false;
    m_daemonFileOpJobId.clear();
    m_daemonFileOpErrored = false;
    m_daemonFileOpError.clear();
    m_failedBatchItems.clear();
    emit failedBatchItemsChanged();
    emit batchOperationStateChanged();
}
