#include "filemanagerapi.h"
#include "filemanagerapi_daemon_common.h"

#include <QTimer>

namespace {

constexpr int kDaemonRecoverMaxMisses = 4;

} // namespace

void FileManagerApi::recoverDaemonFileOpState()
{
    auto parseKind = [](const QString &op) -> BatchKind {
        const QString k = op.trimmed().toLower();
        if (k == QStringLiteral("copy")) return BatchKind::Copy;
        if (k == QStringLiteral("move")) return BatchKind::Move;
        if (k == QStringLiteral("delete")) return BatchKind::Delete;
        if (k == QStringLiteral("trash")) return BatchKind::Trash;
        if (k == QStringLiteral("restore")) return BatchKind::Restore;
        return BatchKind::None;
    };

    auto parseState = [](const QString &stateText, bool *pausedOut) -> BatchState {
        if (pausedOut) {
            *pausedOut = false;
        }
        const QString s = stateText.trimmed().toLower();
        if (s == QStringLiteral("preparing")) return BatchState::Preparing;
        if (s == QStringLiteral("running")) return BatchState::Running;
        if (s == QStringLiteral("paused")) {
            if (pausedOut) {
                *pausedOut = true;
            }
            return BatchState::Paused;
        }
        if (s == QStringLiteral("cancelling")) return BatchState::Cancelling;
        return BatchState::Idle;
    };

    auto applyRow = [this, &parseKind, &parseState](const QVariantMap &row) {
        if (row.isEmpty()) {
            return;
        }
        const QString id = row.value(QStringLiteral("id")).toString().trimmed();
        const BatchKind kind = parseKind(row.value(QStringLiteral("operation")).toString());
        if (id.isEmpty() || kind == BatchKind::None) {
            return;
        }
        bool paused = false;
        const BatchState state = parseState(row.value(QStringLiteral("state")).toString(), &paused);
        if (state == BatchState::Idle) {
            return;
        }

        const qlonglong total = qMax<qlonglong>(1, row.value(QStringLiteral("total")).toLongLong());
        qlonglong current = row.value(QStringLiteral("current")).toLongLong();
        current = qBound<qlonglong>(0, current, total);

        m_batchKind = kind;
        m_batchState = state;
        m_batchId = id;
        m_batchTasks.clear();
        m_batchCurrent = current;
        m_batchTotal = total;
        m_batchError.clear();
        m_batchCreatedPaths.clear();
        m_batchCancelRequested = false;
        m_batchCancelMode = CancelMode::KeepCompleted;
        m_batchOverwrite = false;
        m_batchTotalIsBytes = false;
        m_batchTaskInFlight = false;
        m_batchPreparingInFlight = false;
        m_batchPaused = paused;
        m_daemonFileOpJobId = id;
        m_daemonRecoverMisses = 0;
        m_daemonFileOpErrored = false;
        const QString errCode = row.value(QStringLiteral("error_code")).toString().trimmed();
        const QString errMsg = row.value(QStringLiteral("error_message")).toString().trimmed();
        if (!errCode.isEmpty() || !errMsg.isEmpty()) {
            m_daemonFileOpErrored = true;
            m_daemonFileOpError = errCode;
            if (!errMsg.isEmpty()) {
                m_daemonFileOpError += (m_daemonFileOpError.isEmpty() ? QString() : QStringLiteral(":")) + errMsg;
            }
        } else {
            m_daemonFileOpError.clear();
        }

        emit fileOperationProgress(batchKindToString(kind),
                                   QString(),
                                   QString(),
                                   m_batchCurrent,
                                   m_batchTotal);
        emit batchOperationStateChanged();
    };

    if (batchOperationActive() && !m_daemonFileOpJobId.trimmed().isEmpty()) {
        QVariantMap row;
        if (FileManagerApiDaemonCommon::callFileOpsServiceMap(QStringLiteral("GetJob"),
                                                               {m_daemonFileOpJobId},
                                                               &row)
                && !row.isEmpty()) {
            applyRow(row);
            m_daemonRecoverMisses = 0;
            return;
        }
        ++m_daemonRecoverMisses;
        if (m_daemonRecoverMisses < kDaemonRecoverMaxMisses) {
            QTimer::singleShot(250, this, &FileManagerApi::recoverDaemonFileOpState);
            return;
        }
        m_daemonRecoverMisses = 0;
        finishBatchEngine(false, QStringLiteral("daemon-unavailable"));
        return;
    }
    if (batchOperationActive() && m_daemonFileOpJobId.trimmed().isEmpty()) {
        return;
    }

    QVariantList rows;
    if (!FileManagerApiDaemonCommon::callFileOpsServiceList(QStringLiteral("ListJobs"), {}, &rows)
            || rows.isEmpty()) {
        m_daemonRecoverMisses = 0;
        return;
    }

    for (int i = rows.size() - 1; i >= 0; --i) {
        const QVariantMap row = rows.at(i).toMap();
        const QString state = row.value(QStringLiteral("state")).toString().trimmed().toLower();
        if (state == QStringLiteral("preparing")
            || state == QStringLiteral("running")
            || state == QStringLiteral("paused")
            || state == QStringLiteral("cancelling")) {
            applyRow(row);
            return;
        }
    }
    m_daemonRecoverMisses = 0;
}

void FileManagerApi::onDaemonFileOpsJobsChanged()
{
    QTimer::singleShot(0, this, &FileManagerApi::recoverDaemonFileOpState);
}
