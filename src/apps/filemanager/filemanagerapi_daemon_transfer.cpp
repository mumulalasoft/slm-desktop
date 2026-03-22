#include "filemanagerapi.h"
#include "filemanagerapi_daemon_common.h"

#include <QDateTime>

QVariantMap FileManagerApi::startCopyPaths(const QVariantList &sourcePaths,
                                           const QString &targetDirectory,
                                           bool overwrite)
{
    if (batchOperationActive()) {
        return makeResult(false, QStringLiteral("batch-busy"));
    }
    QStringList expandedPaths;
    for (const QVariant &v : sourcePaths) {
        const QString p = expandPath(v.toString());
        if (p.isEmpty() || expandedPaths.contains(p)) {
            continue;
        }
        expandedPaths.push_back(p);
    }
    const QString targetDir = expandPath(targetDirectory);
    QString daemonJobId;
    QString daemonCallError;
    if (!expandedPaths.isEmpty() && !targetDir.isEmpty()
            && FileManagerApiDaemonCommon::callFileOpsServiceJob(QStringLiteral("Copy"),
                                                                  {QVariant::fromValue(expandedPaths), targetDir},
                                                                  &daemonJobId,
                                                                  &daemonCallError)
            && !daemonJobId.isEmpty()) {
        Q_UNUSED(overwrite)
        startDaemonBatchJob(BatchKind::Copy, daemonJobId, expandedPaths.size(), overwrite);
        return makeResult(true, QString(), {{QStringLiteral("id"), daemonJobId},
                                            {QStringLiteral("operation"), QStringLiteral("copy")},
                                            {QStringLiteral("total"), expandedPaths.size()},
                                            {QStringLiteral("via"), QStringLiteral("daemon")}});
    }

    bool ok = false;
    QString err;
    const QList<BatchTask> tasks = buildCopyTasks(expandedPaths, targetDir, &ok, &err);
    if (!ok || tasks.isEmpty()) {
        QVariantMap extra;
        if (!daemonCallError.isEmpty()) {
            extra.insert(QStringLiteral("daemonError"), daemonCallError);
        }
        return makeResult(false, err.isEmpty() ? QStringLiteral("copy-failed") : err, extra);
    }
    m_batchOverwrite = overwrite;
    const QString id = QString::number(QDateTime::currentMSecsSinceEpoch());
    startBatchEngine(BatchKind::Copy, id, tasks);
    return makeResult(true, QString(), {{QStringLiteral("id"), id},
                                        {QStringLiteral("operation"), QStringLiteral("copy")},
                                        {QStringLiteral("total"), tasks.size()}});
}

QVariantMap FileManagerApi::startMovePaths(const QVariantList &sourcePaths,
                                           const QString &targetDirectory,
                                           bool overwrite)
{
    if (batchOperationActive()) {
        return makeResult(false, QStringLiteral("batch-busy"));
    }
    QStringList expandedPaths;
    for (const QVariant &v : sourcePaths) {
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
    const QString targetDir = expandPath(targetDirectory);
    QString daemonJobId;
    QString daemonCallError;
    if (!expandedPaths.isEmpty() && !targetDir.isEmpty()
            && FileManagerApiDaemonCommon::callFileOpsServiceJob(QStringLiteral("Move"),
                                                                  {QVariant::fromValue(expandedPaths), targetDir},
                                                                  &daemonJobId,
                                                                  &daemonCallError)
            && !daemonJobId.isEmpty()) {
        startDaemonBatchJob(BatchKind::Move, daemonJobId, expandedPaths.size(), overwrite);
        return makeResult(true, QString(), {{QStringLiteral("id"), daemonJobId},
                                            {QStringLiteral("operation"), QStringLiteral("move")},
                                            {QStringLiteral("total"), expandedPaths.size()},
                                            {QStringLiteral("via"), QStringLiteral("daemon")}});
    }

    bool ok = false;
    QString err;
    const QList<BatchTask> tasks = buildCopyTasks(expandedPaths, targetDir, &ok, &err);
    if (!ok || tasks.isEmpty()) {
        QVariantMap extra;
        if (!daemonCallError.isEmpty()) {
            extra.insert(QStringLiteral("daemonError"), daemonCallError);
        }
        return makeResult(false, err.isEmpty() ? QStringLiteral("move-failed") : err, extra);
    }
    m_batchOverwrite = overwrite;
    const QString id = QString::number(QDateTime::currentMSecsSinceEpoch());
    startBatchEngine(BatchKind::Move, id, tasks);
    return makeResult(true, QString(), {{QStringLiteral("id"), id},
                                        {QStringLiteral("operation"), QStringLiteral("move")},
                                        {QStringLiteral("total"), tasks.size()}});
}
