#include "filemanagerapi.h"
#include "filemanagerapi_daemon_common.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <thread>

QVariantMap FileManagerApi::startEmptyTrash(const QString &trashFilesPath)
{
    if (batchOperationActive()) {
        return makeResult(false, QStringLiteral("batch-busy"));
    }
    QString daemonJobId;
    if (FileManagerApiDaemonCommon::callFileOpsServiceJob(QStringLiteral("EmptyTrash"), {}, &daemonJobId)
            && !daemonJobId.isEmpty()) {
        Q_UNUSED(trashFilesPath)
        startDaemonBatchJob(BatchKind::Delete, daemonJobId, 100, false);
        return makeResult(true, QString(), {{QStringLiteral("id"), daemonJobId},
                                            {QStringLiteral("operation"), QStringLiteral("delete")},
                                            {QStringLiteral("total"), -1},
                                            {QStringLiteral("via"), QStringLiteral("daemon")}});
    }

    const QString trashDirPath = expandPath(trashFilesPath);
    const QString id = QString::number(QDateTime::currentMSecsSinceEpoch());
    startPreparingBatchJob(BatchKind::Delete, id, false);

    std::thread([this, trashDirPath]() {
        QList<BatchTask> tasks;
        QDir trashDir(trashDirPath);
        if (trashDir.exists()) {
            const QFileInfoList entries = trashDir.entryInfoList(
                        QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden,
                        QDir::Name);
            tasks.reserve(entries.size());
            for (const QFileInfo &fi : entries) {
                BatchTask t;
                t.sourcePath = fi.absoluteFilePath();
                t.targetPath = fi.absoluteFilePath();
                t.isDir = fi.isDir();
                t.existedBefore = true;
                tasks.push_back(t);
            }
        }
        QMetaObject::invokeMethod(this, [this, tasks]() {
            if (m_batchKind == BatchKind::None) {
                return;
            }
            m_batchPreparingInFlight = false;
            if (m_batchCancelRequested) {
                finishBatchEngine(false, QStringLiteral("cancelled"));
                return;
            }
            m_batchTasks = tasks;
            m_batchTotal = tasks.size();
            m_batchState = m_batchPaused ? BatchState::Paused : BatchState::Running;
            emit batchOperationStateChanged();
            if (m_batchTasks.isEmpty()) {
                finishBatchEngine(true, QString());
                return;
            }
            if (!m_batchPaused) {
                m_batchTimer.start();
            }
        }, Qt::QueuedConnection);
    }).detach();

    return makeResult(true, QString(), {{QStringLiteral("id"), id},
                                        {QStringLiteral("operation"), QStringLiteral("delete")},
                                        {QStringLiteral("total"), -1}});
}
