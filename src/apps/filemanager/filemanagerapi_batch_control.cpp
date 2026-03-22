#include "filemanagerapi.h"

#include <QDateTime>
#include <QTimer>

QVariantList FileManagerApi::failedBatchItems() const
{
    QVariantList out;
    for (const FailedBatchItem &it : m_failedBatchItems) {
        QVariantMap row;
        row.insert(QStringLiteral("operation"), batchKindToString(it.kind));
        row.insert(QStringLiteral("sourcePath"), it.task.sourcePath);
        row.insert(QStringLiteral("targetPath"), it.task.targetPath);
        row.insert(QStringLiteral("path"), (it.kind == BatchKind::Delete) ? it.task.sourcePath : it.task.targetPath);
        row.insert(QStringLiteral("isDir"), it.task.isDir);
        row.insert(QStringLiteral("error"), it.error);
        out.push_back(row);
    }
    return out;
}

QVariantMap FileManagerApi::retryFailedBatchItem(int index)
{
    if (index < 0 || index >= m_failedBatchItems.size()) {
        return makeResult(false, QStringLiteral("index-out-of-range"));
    }
    if (batchOperationActive()) {
        return makeResult(false, QStringLiteral("batch-busy"));
    }
    FailedBatchItem item = m_failedBatchItems.takeAt(index);
    emit failedBatchItemsChanged();
    QList<BatchTask> tasks;
    tasks.push_back(item.task);
    startBatchEngine(item.kind, QString::number(QDateTime::currentMSecsSinceEpoch()), tasks);
    return makeResult(true);
}

QVariantMap FileManagerApi::retryFailedBatchItems()
{
    if (m_failedBatchItems.isEmpty()) {
        return makeResult(false, QStringLiteral("no-failed-items"));
    }
    if (batchOperationActive()) {
        return makeResult(false, QStringLiteral("batch-busy"));
    }
    const BatchKind kind = m_failedBatchItems.first().kind;
    QList<BatchTask> tasks;
    for (const FailedBatchItem &it : m_failedBatchItems) {
        if (it.kind == kind) {
            tasks.push_back(it.task);
        }
    }
    m_failedBatchItems.clear();
    emit failedBatchItemsChanged();
    startBatchEngine(kind, QString::number(QDateTime::currentMSecsSinceEpoch()), tasks);
    return makeResult(true);
}

bool FileManagerApi::batchOperationActive() const
{
    return m_batchKind != BatchKind::None;
}

QString FileManagerApi::batchOperationState() const
{
    return batchStateToString(m_batchState);
}

QString FileManagerApi::batchOperationType() const
{
    return batchKindToString(m_batchKind);
}

QString FileManagerApi::batchOperationId() const
{
    return m_batchId;
}

qlonglong FileManagerApi::batchOperationCurrent() const
{
    return m_batchCurrent;
}

qlonglong FileManagerApi::batchOperationTotal() const
{
    return m_batchTotal;
}

double FileManagerApi::batchOperationProgress() const
{
    if (m_batchTotal <= 0) return 0.0;
    return static_cast<double>(m_batchCurrent) / static_cast<double>(m_batchTotal);
}

bool FileManagerApi::batchOperationPaused() const
{
    return m_batchPaused;
}

int FileManagerApi::failedBatchCount() const
{
    return m_failedBatchItems.size();
}

FileManagerApi::CancelMode FileManagerApi::parseCancelMode(const QString &mode) const
{
    const QString m = mode.trimmed().toLower();
    if (m == QStringLiteral("remove-created")) return CancelMode::RemoveCreated;
    if (m == QStringLiteral("trash-created")) return CancelMode::MoveCreatedToTrash;
    return CancelMode::KeepCompleted;
}

QString FileManagerApi::batchKindToString(BatchKind kind) const
{
    if (kind == BatchKind::Copy) return QStringLiteral("copy");
    if (kind == BatchKind::Move) return QStringLiteral("move");
    if (kind == BatchKind::Delete) return QStringLiteral("delete");
    if (kind == BatchKind::Trash) return QStringLiteral("trash");
    if (kind == BatchKind::Restore) return QStringLiteral("restore");
    return QStringLiteral("unknown");
}

QString FileManagerApi::batchStateToString(BatchState state) const
{
    if (state == BatchState::Preparing) return QStringLiteral("preparing");
    if (state == BatchState::Running) return QStringLiteral("running");
    if (state == BatchState::Paused) return QStringLiteral("paused");
    if (state == BatchState::Cancelling) return QStringLiteral("cancelling");
    return QStringLiteral("idle");
}
void FileManagerApi::onBatchTaskProgress(qlonglong baseBytes, qlonglong currentBytes, qlonglong totalBytes)
{
    if (!m_batchTotalIsBytes || m_batchKind == BatchKind::None) {
        return;
    }
    qlonglong total = totalBytes > 0 ? totalBytes : 1;
    qlonglong current = qBound<qlonglong>(0, currentBytes, total);
    qlonglong next = baseBytes + current;
    if (next > m_batchTotal) {
        next = m_batchTotal;
    }
    if (next < 0) {
        next = 0;
    }
    if (next != m_batchCurrent) {
        m_batchCurrent = next;
        emit batchOperationStateChanged();
    }
}

