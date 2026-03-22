#include "filemanagerapi.h"
#include "filemanagerapi_daemon_common.h"

#include <QDateTime>

QVariantMap FileManagerApi::startRemovePaths(const QVariantList &paths, bool recursive)
{
    if (batchOperationActive()) {
        return makeResult(false, QStringLiteral("batch-busy"));
    }
    QStringList expandedPaths;
    for (const QVariant &v : paths) {
        const QString p = expandPath(v.toString());
        if (p.isEmpty() || expandedPaths.contains(p)) {
            continue;
        }
        expandedPaths.push_back(p);
    }
    QString blockedPath;
    for (const QString &p : expandedPaths) {
        if (FileManagerApiDaemonCommon::isProtectedDesktopFolderPath(p)) {
            blockedPath = p;
            break;
        }
    }
    if (!blockedPath.isEmpty()) {
        return makeResult(false, QStringLiteral("protected-path"), {{QStringLiteral("path"), blockedPath}});
    }
    Q_UNUSED(recursive)
    QString daemonJobId;
    if (!expandedPaths.isEmpty()
            && FileManagerApiDaemonCommon::callFileOpsServiceJob(QStringLiteral("Delete"),
                                                                  {QVariant::fromValue(expandedPaths)},
                                                                  &daemonJobId)
            && !daemonJobId.isEmpty()) {
        startDaemonBatchJob(BatchKind::Delete, daemonJobId, expandedPaths.size(), false);
        return makeResult(true, QString(), {{QStringLiteral("id"), daemonJobId},
                                            {QStringLiteral("operation"), QStringLiteral("delete")},
                                            {QStringLiteral("total"), expandedPaths.size()},
                                            {QStringLiteral("via"), QStringLiteral("daemon")}});
    }

    bool ok = false;
    QString err;
    const QList<BatchTask> tasks = buildDeleteTasks(expandedPaths, true, &ok, &err);
    if (!ok || tasks.isEmpty()) {
        return makeResult(false, err.isEmpty() ? QStringLiteral("delete-failed") : err);
    }
    m_batchOverwrite = false;
    const QString id = QString::number(QDateTime::currentMSecsSinceEpoch());
    startBatchEngine(BatchKind::Delete, id, tasks);
    return makeResult(true, QString(), {{QStringLiteral("id"), id},
                                        {QStringLiteral("operation"), QStringLiteral("delete")},
                                        {QStringLiteral("total"), tasks.size()}});
}

QVariantMap FileManagerApi::startTrashPaths(const QVariantList &paths)
{
    if (batchOperationActive()) {
        return makeResult(false, QStringLiteral("batch-busy"));
    }
    QStringList expandedPaths;
    for (const QVariant &v : paths) {
        const QString p = expandPath(v.toString());
        if (p.isEmpty() || expandedPaths.contains(p)) {
            continue;
        }
        expandedPaths.push_back(p);
    }
    QString blockedPath;
    for (const QString &p : expandedPaths) {
        if (FileManagerApiDaemonCommon::isProtectedDesktopFolderPath(p)) {
            blockedPath = p;
            break;
        }
    }
    if (!blockedPath.isEmpty()) {
        return makeResult(false, QStringLiteral("protected-path"), {{QStringLiteral("path"), blockedPath}});
    }
    QString daemonJobId;
    if (!expandedPaths.isEmpty()
            && FileManagerApiDaemonCommon::callFileOpsServiceJob(QStringLiteral("Trash"),
                                                                  {QVariant::fromValue(expandedPaths)},
                                                                  &daemonJobId)
            && !daemonJobId.isEmpty()) {
        startDaemonBatchJob(BatchKind::Trash, daemonJobId, expandedPaths.size(), false);
        return makeResult(true, QString(), {{QStringLiteral("id"), daemonJobId},
                                            {QStringLiteral("operation"), QStringLiteral("trash")},
                                            {QStringLiteral("total"), expandedPaths.size()},
                                            {QStringLiteral("via"), QStringLiteral("daemon")}});
    }

    bool ok = false;
    QString err;
    const QList<BatchTask> tasks = buildDeleteTasks(expandedPaths, true, &ok, &err);
    if (!ok || tasks.isEmpty()) {
        return makeResult(false, err.isEmpty() ? QStringLiteral("trash-failed") : err);
    }
    m_batchOverwrite = false;
    const QString id = QString::number(QDateTime::currentMSecsSinceEpoch());
    startBatchEngine(BatchKind::Trash, id, tasks);
    return makeResult(true, QString(), {{QStringLiteral("id"), id},
                                        {QStringLiteral("operation"), QStringLiteral("trash")},
                                        {QStringLiteral("total"), tasks.size()}});
}
