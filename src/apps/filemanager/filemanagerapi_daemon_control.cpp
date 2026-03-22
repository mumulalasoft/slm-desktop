#include "filemanagerapi.h"
#include "filemanagerapi_daemon_common.h"

#include <QTimer>

QVariantMap FileManagerApi::cancelActiveBatchOperation(const QString &mode)
{
    if (!batchOperationActive()) {
        return makeResult(false, QStringLiteral("no-active-batch"));
    }
    if (!m_daemonFileOpJobId.isEmpty()) {
        Q_UNUSED(mode)
        bool ok = false;
        QString daemonError;
        if (!FileManagerApiDaemonCommon::callFileOpsServiceBool(QStringLiteral("Cancel"),
                                                                 {m_daemonFileOpJobId},
                                                                 &ok,
                                                                 &daemonError)) {
            return makeResult(false, daemonError.isEmpty() ? QStringLiteral("daemon-unavailable") : daemonError,
                              {{QStringLiteral("id"), m_daemonFileOpJobId}});
        }
        return makeResult(ok, ok ? QString() : QStringLiteral("cancel-failed"),
                          {{QStringLiteral("id"), m_daemonFileOpJobId}});
    }
    m_batchCancelMode = parseCancelMode(mode);
    m_batchCancelRequested = true;
    m_batchState = BatchState::Cancelling;
    emit batchOperationStateChanged();
    return makeResult(true);
}

QVariantMap FileManagerApi::pauseActiveBatchOperation()
{
    if (!batchOperationActive() || m_batchState == BatchState::Preparing) {
        return makeResult(false, QStringLiteral("no-active-batch"));
    }
    if (!m_daemonFileOpJobId.isEmpty()) {
        bool ok = false;
        QString daemonError;
        if (!FileManagerApiDaemonCommon::callFileOpsServiceBool(QStringLiteral("Pause"),
                                                                 {m_daemonFileOpJobId},
                                                                 &ok,
                                                                 &daemonError)) {
            return makeResult(false, daemonError.isEmpty() ? QStringLiteral("daemon-unavailable") : daemonError,
                              {{QStringLiteral("id"), m_daemonFileOpJobId}});
        }
        if (ok) {
            m_batchPaused = true;
            m_batchState = BatchState::Paused;
            emit batchOperationStateChanged();
        }
        return makeResult(ok, ok ? QString() : QStringLiteral("pause-failed"),
                          {{QStringLiteral("id"), m_daemonFileOpJobId}});
    }
    m_batchPaused = true;
    m_batchState = BatchState::Paused;
    emit batchOperationStateChanged();
    return makeResult(true);
}

QVariantMap FileManagerApi::resumeActiveBatchOperation()
{
    if (!batchOperationActive() || m_batchState == BatchState::Preparing) {
        return makeResult(false, QStringLiteral("no-active-batch"));
    }
    if (!m_daemonFileOpJobId.isEmpty()) {
        bool ok = false;
        QString daemonError;
        if (!FileManagerApiDaemonCommon::callFileOpsServiceBool(QStringLiteral("Resume"),
                                                                 {m_daemonFileOpJobId},
                                                                 &ok,
                                                                 &daemonError)) {
            return makeResult(false, daemonError.isEmpty() ? QStringLiteral("daemon-unavailable") : daemonError,
                              {{QStringLiteral("id"), m_daemonFileOpJobId}});
        }
        if (ok) {
            m_batchPaused = false;
            m_batchState = BatchState::Running;
            emit batchOperationStateChanged();
        }
        return makeResult(ok, ok ? QString() : QStringLiteral("resume-failed"),
                          {{QStringLiteral("id"), m_daemonFileOpJobId}});
    }
    m_batchPaused = false;
    m_batchState = BatchState::Running;
    emit batchOperationStateChanged();
    QTimer::singleShot(0, this, &FileManagerApi::processBatchStep);
    return makeResult(true);
}
