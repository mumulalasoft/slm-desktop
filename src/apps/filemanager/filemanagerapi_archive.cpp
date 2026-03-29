#include "filemanagerapi.h"

#include <QDBusInterface>
#include <QDBusReply>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QProcess>
#include <QSet>
#include <QThread>
#include <QVariant>

#include <functional>
#include <thread>

namespace {

constexpr const char kArchiveService[] = "org.slm.Desktop.Archive";
constexpr const char kArchivePath[] = "/org/slm/Desktop/Archive";
constexpr const char kArchiveIface[] = "org.slm.Desktop.Archive";
using ArchiveJobProgressCallback = std::function<void(int, const QString &, const QString &)>;
constexpr qlonglong kDefaultTinyArchiveProgressBytes = 2LL * 1024LL * 1024LL;

static QString topLevelEntryNameArchive(const QString &path)
{
    QString p = path.trimmed();
    while (p.startsWith(QStringLiteral("./"))) {
        p.remove(0, 2);
    }
    while (p.endsWith(QLatin1Char('/'))) {
        p.chop(1);
    }
    if (p.isEmpty()) {
        return {};
    }
    const int slash = p.indexOf(QLatin1Char('/'));
    return slash > 0 ? p.left(slash) : p;
}

static QVariantMap inspectArchiveLayout(const QString &archive)
{
    QDBusInterface iface(QString::fromLatin1(kArchiveService),
                         QString::fromLatin1(kArchivePath),
                         QString::fromLatin1(kArchiveIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return {};
    }
    QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("ListArchive"), archive, 5000);
    if (!reply.isValid()) {
        return {};
    }
    const QVariantMap listed = reply.value();
    if (!listed.value(QStringLiteral("ok")).toBool()) {
        return {};
    }
    QVariantMap out;
    const QString layout = listed.value(QStringLiteral("layout")).toString().trimmed();
    if (!layout.isEmpty()) {
        out.insert(QStringLiteral("layout"), layout);
    }
    if (layout == QStringLiteral("single_root_folder")) {
        const QVariantList entries = listed.value(QStringLiteral("entries")).toList();
        QString rootName;
        for (const QVariant &v : entries) {
            const QVariantMap row = v.toMap();
            const QString top = topLevelEntryNameArchive(row.value(QStringLiteral("path")).toString());
            if (top.isEmpty()) {
                continue;
            }
            if (rootName.isEmpty()) {
                rootName = top;
                continue;
            }
            if (rootName != top) {
                rootName.clear();
                break;
            }
        }
        if (!rootName.isEmpty()) {
            out.insert(QStringLiteral("singleRootName"), rootName);
        }
    }
    return out;
}

static QVariantMap requestArchiveList(const QString &archive, int maxEntries)
{
    QDBusInterface iface(QString::fromLatin1(kArchiveService),
                         QString::fromLatin1(kArchivePath),
                         QString::fromLatin1(kArchiveIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return {};
    }
    QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("ListArchive"), archive, qMax(1, maxEntries));
    if (!reply.isValid()) {
        return {};
    }
    return reply.value();
}

static QString preferredExtractOpenPath(const QString &destinationDir,
                                        const QVariantMap &layoutInfo)
{
    const QString layout = layoutInfo.value(QStringLiteral("layout")).toString();
    const QString rootName = layoutInfo.value(QStringLiteral("singleRootName")).toString();
    if (layout == QStringLiteral("single_root_folder") && !rootName.trimmed().isEmpty()) {
        const QString candidate = QDir(destinationDir).absoluteFilePath(rootName.trimmed());
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

static QStringList uniqueExpandedPathsArchive(const QVariantList &paths)
{
    QStringList out;
    QSet<QString> seen;
    for (const QVariant &v : paths) {
        QString p = v.toString().trimmed();
        if (p == QStringLiteral("~")) {
            p = QDir::homePath();
        } else if (p.startsWith(QStringLiteral("~/"))) {
            p = QDir::homePath() + p.mid(1);
        }
        if (p.isEmpty()) {
            continue;
        }
        QFileInfo fi(p);
        const QString abs = fi.absoluteFilePath();
        if (seen.contains(abs)) {
            continue;
        }
        seen.insert(abs);
        out.push_back(abs);
    }
    return out;
}

static qlonglong tinyArchiveProgressBytesThreshold()
{
    const qlonglong v = qEnvironmentVariableIntValue("SLM_ARCHIVE_PROGRESS_TINY_BYTES");
    return v > 0 ? v : kDefaultTinyArchiveProgressBytes;
}

static qlonglong estimateExtractWorkBytes(const QString &archivePath)
{
    const QFileInfo fi(archivePath);
    if (!fi.exists() || !fi.isFile()) {
        return 0;
    }
    return qMax<qlonglong>(1, fi.size());
}

static qlonglong estimateCompressWorkBytes(const QStringList &sourcePaths)
{
    qlonglong total = 0;
    for (const QString &src : sourcePaths) {
        const QFileInfo fi(src);
        if (!fi.exists()) {
            continue;
        }
        if (fi.isFile()) {
            total += qMax<qlonglong>(0, fi.size());
            continue;
        }
        if (!fi.isDir()) {
            continue;
        }
        QDirIterator it(src,
                        QDir::Files | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            total += qMax<qlonglong>(0, it.fileInfo().size());
        }
    }
    return qMax<qlonglong>(1, total);
}

static bool shouldAutoOpenProgressPopup(const QString &opType, qlonglong totalWorkBytes)
{
    const QString op = opType.trimmed().toLower();
    if (op != QStringLiteral("extract") && op != QStringLiteral("compress")) {
        return true;
    }
    if (totalWorkBytes <= 0) {
        return true;
    }
    return totalWorkBytes > tinyArchiveProgressBytesThreshold();
}

static QVariantMap runBsdtarExtractFallback(const QString &archive, const QString &dstRoot)
{
    QString baseName = QFileInfo(archive).completeBaseName();
    if (baseName.isEmpty()) {
        baseName = QFileInfo(archive).baseName();
    }
    if (baseName.isEmpty()) {
        baseName = QStringLiteral("Extracted");
    }

    QString target = QDir(dstRoot).absoluteFilePath(baseName);
    int n = 2;
    while (QFileInfo::exists(target) && n < 10000) {
        target = QDir(dstRoot).absoluteFilePath(QStringLiteral("%1 %2").arg(baseName).arg(n));
        ++n;
    }
    QDir().mkpath(target);

    QProcess p;
    p.start(QStringLiteral("bsdtar"), QStringList{QStringLiteral("-xf"), archive, QStringLiteral("-C"), target});
    p.waitForFinished(-1);
    const QString out = QString::fromUtf8(p.readAllStandardOutput());
    const QString err = QString::fromUtf8(p.readAllStandardError()).trimmed();
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        return QVariantMap{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), err.isEmpty() ? QStringLiteral("extract-failed") : err}
        };
    }
    return QVariantMap{
        {QStringLiteral("ok"), true},
        {QStringLiteral("output"), out},
        {QStringLiteral("destination"), target}
    };
}

static QVariantMap runBsdtarCompressFallback(const QStringList &srcs, const QString &archive)
{
    QStringList args;
    args << QStringLiteral("-cf") << archive;
    for (const QString &p : srcs) {
        args << p;
    }
    QProcess p;
    p.start(QStringLiteral("bsdtar"), args);
    p.waitForFinished(-1);
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        const QString err = QString::fromUtf8(p.readAllStandardError()).trimmed();
        return QVariantMap{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), err.isEmpty() ? QStringLiteral("compress-failed") : err}
        };
    }
    return QVariantMap{{QStringLiteral("ok"), true}, {QStringLiteral("archive"), archive}};
}

static QString requestExtractJob(const QString &archive, const QString &destinationRoot)
{
    QDBusInterface iface(QString::fromLatin1(kArchiveService),
                         QString::fromLatin1(kArchivePath),
                         QString::fromLatin1(kArchiveIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return {};
    }

    QDBusReply<QString> reply = iface.call(QStringLiteral("ExtractArchive"),
                                           archive,
                                           destinationRoot,
                                           QStringLiteral("extract_sibling_default"),
                                           QStringLiteral("auto_rename"));
    return reply.isValid() ? reply.value().trimmed() : QString();
}

static QString requestCompressJob(const QStringList &paths,
                                  const QString &destinationArchive,
                                  const QString &format)
{
    QDBusInterface iface(QString::fromLatin1(kArchiveService),
                         QString::fromLatin1(kArchivePath),
                         QString::fromLatin1(kArchiveIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return {};
    }

    QDBusReply<QString> reply = iface.call(QStringLiteral("CompressPaths"),
                                           paths,
                                           destinationArchive,
                                           format.trimmed().isEmpty() ? QStringLiteral("zip") : format.trimmed());
    return reply.isValid() ? reply.value().trimmed() : QString();
}

static QVariantMap waitJobResult(const QString &jobId,
                                 int timeoutMs = 10 * 60 * 1000,
                                 const ArchiveJobProgressCallback &progressCb = {})
{
    QDBusInterface iface(QString::fromLatin1(kArchiveService),
                         QString::fromLatin1(kArchivePath),
                         QString::fromLatin1(kArchiveIface),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("archive-service-unavailable")}
        };
    }

    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeoutMs) {
        QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("GetJobStatus"), jobId);
        if (!reply.isValid()) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("archive-job-status-failed")}
            };
        }

        const QVariantMap st = reply.value();
        const QString state = st.value(QStringLiteral("state")).toString();
        if (progressCb) {
            progressCb(qBound(0, st.value(QStringLiteral("progress")).toInt(), 100),
                       st.value(QStringLiteral("message")).toString(),
                       state);
        }
        if (state == QStringLiteral("completed")) {
            QVariantMap result = st.value(QStringLiteral("result")).toMap();
            result.insert(QStringLiteral("ok"), true);
            return result;
        }
        if (state == QStringLiteral("failed") || state == QStringLiteral("cancelled")) {
            QVariantMap err = st.value(QStringLiteral("error")).toMap();
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("error"), err.value(QStringLiteral("message")).toString().trimmed().isEmpty()
                                              ? QStringLiteral("archive-job-failed")
                                              : err.value(QStringLiteral("message")).toString()},
                {QStringLiteral("errorCode"), err.value(QStringLiteral("errorCode")).toString()}
            };
        }

        QThread::msleep(90);
    }

    return {
        {QStringLiteral("ok"), false},
        {QStringLiteral("error"), QStringLiteral("archive-job-timeout")}
    };
}

} // namespace

QVariantMap FileManagerApi::extractArchive(const QString &archivePath, const QString &destinationDir) const
{
    const QString archive = expandPath(archivePath);
    if (!QFileInfo::exists(archive)) {
        return makeResult(false, QStringLiteral("not-found"));
    }
    const QString dstRoot = destinationDir.trimmed().isEmpty()
                                ? QFileInfo(archive).absolutePath()
                                : expandPath(destinationDir);
    QDir().mkpath(dstRoot);

    const QString jobId = requestExtractJob(archive, dstRoot);
    if (!jobId.isEmpty()) {
        const QVariantMap result = waitJobResult(jobId);
        if (result.value(QStringLiteral("ok")).toBool()) {
            return makeResult(true,
                              QString(),
                              {{QStringLiteral("to"), result.value(QStringLiteral("destination")).toString()},
                               {QStringLiteral("jobId"), jobId},
                               {QStringLiteral("backend"), QStringLiteral("archive-service")}});
        }
    }

    const QVariantMap fallback = runBsdtarExtractFallback(archive, dstRoot);
    if (!fallback.value(QStringLiteral("ok")).toBool()) {
        return makeResult(false, fallback.value(QStringLiteral("error")).toString());
    }
    return makeResult(true,
                      QString(),
                      {{QStringLiteral("to"), fallback.value(QStringLiteral("destination")).toString()},
                       {QStringLiteral("backend"), QStringLiteral("bsdtar-fallback")},
                       {QStringLiteral("output"), fallback.value(QStringLiteral("output")).toString()}});
}

QVariantMap FileManagerApi::compressArchive(const QVariantList &sourcePaths,
                                            const QString &archivePath,
                                            const QString &format) const
{
    const QString archive = expandPath(archivePath);
    const QStringList srcs = uniqueExpandedPathsArchive(sourcePaths);
    if (archive.isEmpty() || srcs.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-path"));
    }

    const QString jobId = requestCompressJob(srcs, archive, format);
    if (!jobId.isEmpty()) {
        const QVariantMap result = waitJobResult(jobId);
        if (result.value(QStringLiteral("ok")).toBool()) {
            return makeResult(true,
                              QString(),
                              {{QStringLiteral("path"), result.value(QStringLiteral("archive")).toString()},
                               {QStringLiteral("jobId"), jobId},
                               {QStringLiteral("backend"), QStringLiteral("archive-service")}});
        }
    }

    const QVariantMap fallback = runBsdtarCompressFallback(srcs, archive);
    if (!fallback.value(QStringLiteral("ok")).toBool()) {
        return makeResult(false, fallback.value(QStringLiteral("error")).toString());
    }
    return makeResult(true,
                      QString(),
                      {{QStringLiteral("path"), archive},
                       {QStringLiteral("backend"), QStringLiteral("bsdtar-fallback")}});
}

QVariantMap FileManagerApi::previewArchiveContents(const QString &archivePath, int maxEntries) const
{
    const QString archive = expandPath(archivePath);
    if (archive.isEmpty() || !QFileInfo::exists(archive)) {
        return makeResult(false, QStringLiteral("not-found"));
    }

    const int limit = qBound(1, maxEntries, 500);
    QVariantMap listed = requestArchiveList(archive, limit);

    if (!listed.value(QStringLiteral("ok")).toBool()) {
        QProcess p;
        p.start(QStringLiteral("bsdtar"), QStringList{QStringLiteral("-tf"), archive});
        p.waitForFinished(-1);
        if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
            const QString err = QString::fromUtf8(p.readAllStandardError()).trimmed();
            return makeResult(false, err.isEmpty() ? QStringLiteral("archive-preview-failed") : err);
        }
        const QStringList rows = QString::fromUtf8(p.readAllStandardOutput()).split(QLatin1Char('\n'),
                                                                                    Qt::SkipEmptyParts);
        QVariantList lines;
        const int count = qMin(limit, rows.size());
        for (int i = 0; i < count; ++i) {
            const QString line = rows.at(i).trimmed();
            if (!line.isEmpty()) {
                lines.push_back(line);
            }
        }
        return makeResult(true,
                          QString(),
                          {{QStringLiteral("entries"), lines},
                           {QStringLiteral("entryCount"), rows.size()},
                           {QStringLiteral("truncated"), rows.size() > count},
                           {QStringLiteral("layout"), QStringLiteral("")},
                           {QStringLiteral("backend"), QStringLiteral("bsdtar-fallback")}});
    }

    const QVariantList entries = listed.value(QStringLiteral("entries")).toList();
    QVariantList lines;
    lines.reserve(entries.size());
    for (const QVariant &v : entries) {
        const QVariantMap row = v.toMap();
        const QString path = row.value(QStringLiteral("path")).toString().trimmed();
        if (!path.isEmpty()) {
            lines.push_back(path);
        }
    }
    const int totalEntries = listed.value(QStringLiteral("entryCount")).toInt();
    return makeResult(true,
                      QString(),
                      {{QStringLiteral("entries"), lines},
                       {QStringLiteral("entryCount"), totalEntries},
                       {QStringLiteral("truncated"), totalEntries > lines.size()},
                       {QStringLiteral("layout"), listed.value(QStringLiteral("layout")).toString()},
                       {QStringLiteral("backend"), QStringLiteral("archive-service")}});
}

QVariantMap FileManagerApi::startExtractArchive(const QString &archivePath,
                                                const QString &destinationDir)
{
    const QString archive = expandPath(archivePath);
    const QString destination = destinationDir.trimmed();
    if (archive.isEmpty() || !QFileInfo::exists(archive)) {
        return makeResult(false, QStringLiteral("invalid-path"));
    }
    if (batchOperationActive()) {
        return makeResult(false, QStringLiteral("batch-busy"));
    }

    const QString opId = QStringLiteral("archive-extract-%1").arg(QDateTime::currentMSecsSinceEpoch());
    const qlonglong estimatedWorkBytes = estimateExtractWorkBytes(archive);
    const bool autoPopup = shouldAutoOpenProgressPopup(QStringLiteral("extract"), estimatedWorkBytes);
    m_batchKind = BatchKind::Extract;
    m_batchState = BatchState::Running;
    m_batchId = opId;
    m_batchTasks.clear();
    m_batchCurrent = 0;
    m_batchTotal = qMax<qlonglong>(1, estimatedWorkBytes);
    m_batchError.clear();
    m_batchCreatedPaths.clear();
    m_batchCancelRequested = false;
    m_batchCancelMode = CancelMode::KeepCompleted;
    m_batchOverwrite = false;
    m_batchTotalIsBytes = true;
    m_batchTaskInFlight = false;
    m_batchPreparingInFlight = false;
    m_batchPaused = false;
    m_daemonFileOpJobId.clear();
    m_daemonFileOpErrored = false;
    m_daemonFileOpError.clear();
    m_failedBatchItems.clear();
    emit failedBatchItemsChanged();
    emit fileOperationProgress(batchKindToString(m_batchKind), archive, QString(), m_batchCurrent, m_batchTotal);
    emit batchOperationStateChanged();

    std::thread([this, archive, destination, opId]() {
        const QString dstRoot = destination.trimmed().isEmpty()
                                    ? QFileInfo(archive).absolutePath()
                                    : expandPath(destination);
        QDir().mkpath(dstRoot);
        const QVariantMap layoutInfo = inspectArchiveLayout(archive);

        QVariantMap result;
        QString activeJobId = requestExtractJob(archive, dstRoot);
        if (!activeJobId.isEmpty()) {
            QMetaObject::invokeMethod(this, [this, activeJobId]() {
                if (m_batchKind != BatchKind::Extract) {
                    return;
                }
                m_batchId = activeJobId;
                m_daemonFileOpJobId = activeJobId;
                emit batchOperationStateChanged();
            }, Qt::QueuedConnection);

            result = waitJobResult(activeJobId, 10 * 60 * 1000, [this, activeJobId, archive](int progress, const QString &, const QString &) {
                QMetaObject::invokeMethod(this, [this, activeJobId, archive, progress]() {
                    if (m_batchKind != BatchKind::Extract) {
                        return;
                    }
                    if (!m_daemonFileOpJobId.isEmpty() && m_daemonFileOpJobId != activeJobId) {
                        return;
                    }
                    const qlonglong total = qMax<qlonglong>(1, m_batchTotal);
                    const qlonglong current = qBound<qlonglong>(0, (total * qBound(0, progress, 100)) / 100, total);
                    if (current != m_batchCurrent) {
                        m_batchCurrent = current;
                        emit fileOperationProgress(batchKindToString(m_batchKind),
                                                   archive,
                                                   QString(),
                                                   m_batchCurrent,
                                                   m_batchTotal);
                        emit batchOperationStateChanged();
                    }
                }, Qt::QueuedConnection);
            });
        }
        if (activeJobId.isEmpty()) {
            const QVariantMap fallback = runBsdtarExtractFallback(archive, dstRoot);
            if (fallback.value(QStringLiteral("ok")).toBool()) {
                const QString destinationPath = fallback.value(QStringLiteral("destination")).toString();
                const QString openPath = preferredExtractOpenPath(destinationPath, layoutInfo);
                result = makeResult(true,
                                    QString(),
                                    {{QStringLiteral("to"), destinationPath},
                                     {QStringLiteral("backend"), QStringLiteral("bsdtar-fallback")},
                                     {QStringLiteral("output"), fallback.value(QStringLiteral("output")).toString()},
                                     {QStringLiteral("layout"), layoutInfo.value(QStringLiteral("layout")).toString()},
                                     {QStringLiteral("singleRootName"), layoutInfo.value(QStringLiteral("singleRootName")).toString()},
                                     {QStringLiteral("openPath"), openPath}});
            } else {
                result = makeResult(false, fallback.value(QStringLiteral("error")).toString());
            }
        } else if (result.value(QStringLiteral("ok")).toBool()) {
            const QString destinationPath = result.value(QStringLiteral("destination")).toString();
            const QString openPath = preferredExtractOpenPath(destinationPath, layoutInfo);
            result = makeResult(true,
                                QString(),
                                {{QStringLiteral("to"), destinationPath},
                                 {QStringLiteral("jobId"), activeJobId},
                                 {QStringLiteral("backend"), QStringLiteral("archive-service")},
                                 {QStringLiteral("layout"), layoutInfo.value(QStringLiteral("layout")).toString()},
                                 {QStringLiteral("singleRootName"), layoutInfo.value(QStringLiteral("singleRootName")).toString()},
                                 {QStringLiteral("openPath"), openPath}});
        }

        const bool ok = result.value(QStringLiteral("ok")).toBool();
        const QString to = result.value(QStringLiteral("to")).toString();
        const QString error = result.value(QStringLiteral("error")).toString();
        QMetaObject::invokeMethod(this, [this, archive, ok, to, error, result, opId, activeJobId]() {
            if (m_batchKind == BatchKind::Extract
                    && (m_batchId == opId || m_batchId == activeJobId || m_batchId.isEmpty())) {
                if (ok) {
                    m_batchCurrent = m_batchTotal;
                }
                m_batchKind = BatchKind::None;
                m_batchId.clear();
                m_batchTasks.clear();
                m_batchCurrent = 0;
                m_batchTotal = 0;
                m_batchError.clear();
                m_batchCreatedPaths.clear();
                m_batchCancelRequested = false;
                m_batchTotalIsBytes = false;
                m_batchTaskInFlight = false;
                m_batchPreparingInFlight = false;
                m_batchPaused = false;
                m_batchState = BatchState::Idle;
                m_daemonFileOpJobId.clear();
                m_daemonFileOpErrored = false;
                m_daemonFileOpError.clear();
                emit batchOperationStateChanged();
            }
            emit archiveExtractFinished(archive, ok, to, error, result);
        }, Qt::QueuedConnection);
    }).detach();
    return makeResult(true, QString(),
                      {{QStringLiteral("archive"), archive},
                       {QStringLiteral("destinationDir"), destination},
                       {QStringLiteral("autoPopup"), autoPopup},
                       {QStringLiteral("estimatedWorkBytes"), estimatedWorkBytes},
                       {QStringLiteral("async"), true}});
}

QVariantMap FileManagerApi::startCompressArchive(const QVariantList &sourcePaths,
                                                 const QString &archivePath,
                                                 const QString &format)
{
    const QString archive = expandPath(archivePath);
    if (archive.isEmpty() || sourcePaths.isEmpty()) {
        return makeResult(false, QStringLiteral("invalid-path"));
    }
    if (batchOperationActive()) {
        return makeResult(false, QStringLiteral("batch-busy"));
    }

    const QString opId = QStringLiteral("archive-compress-%1").arg(QDateTime::currentMSecsSinceEpoch());
    const QStringList estimatedSourcePaths = uniqueExpandedPathsArchive(sourcePaths);
    const qlonglong estimatedWorkBytes = estimateCompressWorkBytes(estimatedSourcePaths);
    const bool autoPopup = shouldAutoOpenProgressPopup(QStringLiteral("compress"), estimatedWorkBytes);
    m_batchKind = BatchKind::Compress;
    m_batchState = BatchState::Running;
    m_batchId = opId;
    m_batchTasks.clear();
    m_batchCurrent = 0;
    m_batchTotal = qMax<qlonglong>(1, estimatedWorkBytes);
    m_batchError.clear();
    m_batchCreatedPaths.clear();
    m_batchCancelRequested = false;
    m_batchCancelMode = CancelMode::KeepCompleted;
    m_batchOverwrite = false;
    m_batchTotalIsBytes = true;
    m_batchTaskInFlight = false;
    m_batchPreparingInFlight = false;
    m_batchPaused = false;
    m_daemonFileOpJobId.clear();
    m_daemonFileOpErrored = false;
    m_daemonFileOpError.clear();
    m_failedBatchItems.clear();
    emit failedBatchItemsChanged();
    emit fileOperationProgress(batchKindToString(m_batchKind), QString(), archive, m_batchCurrent, m_batchTotal);
    emit batchOperationStateChanged();

    const QVariantList sources = sourcePaths;
    const QString fmt = format;
    std::thread([this, sources, archive, fmt, opId]() {
        const QStringList srcs = uniqueExpandedPathsArchive(sources);
        QVariantMap result;
        QString activeJobId = requestCompressJob(srcs, archive, fmt);
        if (!activeJobId.isEmpty()) {
            QMetaObject::invokeMethod(this, [this, activeJobId]() {
                if (m_batchKind != BatchKind::Compress) {
                    return;
                }
                m_batchId = activeJobId;
                m_daemonFileOpJobId = activeJobId;
                emit batchOperationStateChanged();
            }, Qt::QueuedConnection);

            result = waitJobResult(activeJobId, 10 * 60 * 1000, [this, activeJobId, archive](int progress, const QString &, const QString &) {
                QMetaObject::invokeMethod(this, [this, activeJobId, archive, progress]() {
                    if (m_batchKind != BatchKind::Compress) {
                        return;
                    }
                    if (!m_daemonFileOpJobId.isEmpty() && m_daemonFileOpJobId != activeJobId) {
                        return;
                    }
                    const qlonglong total = qMax<qlonglong>(1, m_batchTotal);
                    const qlonglong current = qBound<qlonglong>(0, (total * qBound(0, progress, 100)) / 100, total);
                    if (current != m_batchCurrent) {
                        m_batchCurrent = current;
                        emit fileOperationProgress(batchKindToString(m_batchKind),
                                                   QString(),
                                                   archive,
                                                   m_batchCurrent,
                                                   m_batchTotal);
                        emit batchOperationStateChanged();
                    }
                }, Qt::QueuedConnection);
            });
        }
        if (activeJobId.isEmpty()) {
            const QVariantMap fallback = runBsdtarCompressFallback(srcs, archive);
            if (fallback.value(QStringLiteral("ok")).toBool()) {
                result = makeResult(true,
                                    QString(),
                                    {{QStringLiteral("path"), archive},
                                     {QStringLiteral("backend"), QStringLiteral("bsdtar-fallback")}});
            } else {
                result = makeResult(false, fallback.value(QStringLiteral("error")).toString());
            }
        } else if (result.value(QStringLiteral("ok")).toBool()) {
            result = makeResult(true,
                                QString(),
                                {{QStringLiteral("path"), result.value(QStringLiteral("archive")).toString()},
                                 {QStringLiteral("jobId"), activeJobId},
                                 {QStringLiteral("backend"), QStringLiteral("archive-service")}});
        }
        const bool ok = result.value(QStringLiteral("ok")).toBool();
        const QString error = result.value(QStringLiteral("error")).toString();
        QMetaObject::invokeMethod(this, [this, archive, ok, error, result, opId, activeJobId]() {
            if (m_batchKind == BatchKind::Compress
                    && (m_batchId == opId || m_batchId == activeJobId || m_batchId.isEmpty())) {
                if (ok) {
                    m_batchCurrent = m_batchTotal;
                }
                m_batchKind = BatchKind::None;
                m_batchId.clear();
                m_batchTasks.clear();
                m_batchCurrent = 0;
                m_batchTotal = 0;
                m_batchError.clear();
                m_batchCreatedPaths.clear();
                m_batchCancelRequested = false;
                m_batchTotalIsBytes = false;
                m_batchTaskInFlight = false;
                m_batchPreparingInFlight = false;
                m_batchPaused = false;
                m_batchState = BatchState::Idle;
                m_daemonFileOpJobId.clear();
                m_daemonFileOpErrored = false;
                m_daemonFileOpError.clear();
                emit batchOperationStateChanged();
            }
            emit archiveCompressFinished(archive, ok, error, result);
        }, Qt::QueuedConnection);
    }).detach();
    return makeResult(true, QString(),
                      {{QStringLiteral("archive"), archive},
                       {QStringLiteral("autoPopup"), autoPopup},
                       {QStringLiteral("estimatedWorkBytes"), estimatedWorkBytes},
                       {QStringLiteral("async"), true}});
}
