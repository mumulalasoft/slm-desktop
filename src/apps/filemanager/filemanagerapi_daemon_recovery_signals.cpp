#include "filemanagerapi.h"
#include "src/apps/filemanager/ops/fileoperationserrors.h"

void FileManagerApi::onDaemonFileOpProgress(const QString &id, int percent)
{
    if (id.trimmed().isEmpty() || id.trimmed() != m_daemonFileOpJobId || m_batchKind == BatchKind::None) {
        return;
    }
    const int p = qBound(0, percent, 100);
    const qlonglong total = qMax<qlonglong>(1, m_batchTotal);
    const qlonglong current = qBound<qlonglong>(0, (total * p) / 100, total);
    if (current != m_batchCurrent) {
        m_batchCurrent = current;
        emit fileOperationProgress(batchKindToString(m_batchKind),
                                   QString(),
                                   QString(),
                                   m_batchCurrent,
                                   m_batchTotal);
        emit batchOperationStateChanged();
    }
}

void FileManagerApi::onDaemonFileOpProgressDetail(const QString &id, int current, int total)
{
    if (id.trimmed().isEmpty() || id.trimmed() != m_daemonFileOpJobId || m_batchKind == BatchKind::None) {
        return;
    }
    const qlonglong safeTotal = qMax<qlonglong>(1, total);
    const qlonglong safeCurrent = qBound<qlonglong>(qlonglong(0), qlonglong(current), safeTotal);
    bool changed = false;
    if (m_batchTotal != safeTotal) {
        m_batchTotal = safeTotal;
        changed = true;
    }
    if (m_batchCurrent != safeCurrent) {
        m_batchCurrent = safeCurrent;
        changed = true;
    }
    if (m_batchPaused) {
        m_batchPaused = false;
        m_batchState = BatchState::Running;
        changed = true;
    }
    if (changed) {
        emit fileOperationProgress(batchKindToString(m_batchKind),
                                   QString(),
                                   QString(),
                                   m_batchCurrent,
                                   m_batchTotal);
        emit batchOperationStateChanged();
    }
}

void FileManagerApi::onDaemonFileOpFinished(const QString &id)
{
    if (id.trimmed().isEmpty() || id.trimmed() != m_daemonFileOpJobId || m_batchKind == BatchKind::None) {
        return;
    }
    if (m_batchTotal > 0) {
        m_batchCurrent = m_batchTotal;
    }
    const bool ok = !m_daemonFileOpErrored;
    const QString err = m_daemonFileOpError;
    m_daemonRecoverMisses = 0;
    finishBatchEngine(ok, ok ? QString() : err);
}

void FileManagerApi::onDaemonFileOpError(const QString &id)
{
    if (id.trimmed().isEmpty() || id.trimmed() != m_daemonFileOpJobId || m_batchKind == BatchKind::None) {
        return;
    }
    m_daemonFileOpErrored = true;
    if (m_daemonFileOpError.isEmpty()) {
        m_daemonFileOpError = QString::fromLatin1(SlmFileOperationErrors::kDaemonFileOpError);
    }
}

void FileManagerApi::onDaemonFileOpErrorDetail(const QString &id,
                                               const QString &code,
                                               const QString &message)
{
    if (id.trimmed().isEmpty() || id.trimmed() != m_daemonFileOpJobId || m_batchKind == BatchKind::None) {
        return;
    }
    m_daemonFileOpErrored = true;
    const QString trimmedCode = code.trimmed();
    const QString trimmedMessage = message.trimmed();
    if (!trimmedCode.isEmpty() && !trimmedMessage.isEmpty()) {
        m_daemonFileOpError = trimmedCode + QStringLiteral(":") + trimmedMessage;
    } else if (!trimmedCode.isEmpty()) {
        m_daemonFileOpError = trimmedCode;
    } else if (!trimmedMessage.isEmpty()) {
        m_daemonFileOpError = trimmedMessage;
    } else if (m_daemonFileOpError.isEmpty()) {
        m_daemonFileOpError = QString::fromLatin1(SlmFileOperationErrors::kDaemonFileOpError);
    }
}
