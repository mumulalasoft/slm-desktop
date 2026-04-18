#include "cleaneranalyzer.h"
#include "cleanerscanner.h"

#include <QDateTime>

namespace Slm::Cleaner {

QVariantMap CleanerAnalyzer::analyze(const QVector<CacheNodeStat> &nodes) const
{
    QVariantMap out;
    QVariantList appList;

    quint64 thumbnailSize = 0;
    quint64 thumbnailFiles = 0;
    quint64 failedSize = 0;
    quint64 failedFiles = 0;
    quint64 totalSize = 0;
    quint64 totalFiles = 0;

    const QString thumbnailRoot = CleanerScanner::resolveThumbnailRoot();
    const QString failRoot = CleanerScanner::resolveFailThumbnailRoot();

    for (const CacheNodeStat &node : nodes) {
        totalSize += node.sizeBytes;
        totalFiles += node.fileCount;

        const QString path = node.path;
        const bool isThumb = (path == thumbnailRoot || path.startsWith(thumbnailRoot + QLatin1Char('/')));
        const bool isFail = (path == failRoot || path.startsWith(failRoot + QLatin1Char('/')));
        if (isThumb) {
            thumbnailSize += node.sizeBytes;
            thumbnailFiles += node.fileCount;
            if (isFail) {
                failedSize += node.sizeBytes;
                failedFiles += node.fileCount;
            }
            continue;
        }

        QVariantMap app;
        app.insert(QStringLiteral("name"), node.name);
        app.insert(QStringLiteral("path"), node.path);
        app.insert(QStringLiteral("sizeBytes"), QVariant::fromValue<qulonglong>(node.sizeBytes));
        app.insert(QStringLiteral("files"), QVariant::fromValue<qulonglong>(node.fileCount));
        app.insert(QStringLiteral("lastAccessEpoch"), QVariant::fromValue<qlonglong>(node.lastAccessEpoch));
        app.insert(QStringLiteral("lastAccessIso"),
                   node.lastAccessEpoch > 0
                   ? QDateTime::fromSecsSinceEpoch(node.lastAccessEpoch, Qt::UTC).toString(Qt::ISODate)
                   : QString());
        appList.push_back(app);
    }

    QVariantMap thumbnail;
    thumbnail.insert(QStringLiteral("sizeBytes"), QVariant::fromValue<qulonglong>(thumbnailSize));
    thumbnail.insert(QStringLiteral("files"), QVariant::fromValue<qulonglong>(thumbnailFiles));
    out.insert(QStringLiteral("thumbnail"), thumbnail);

    QVariantMap failed;
    failed.insert(QStringLiteral("sizeBytes"), QVariant::fromValue<qulonglong>(failedSize));
    failed.insert(QStringLiteral("files"), QVariant::fromValue<qulonglong>(failedFiles));
    out.insert(QStringLiteral("failed_thumbnail"), failed);

    out.insert(QStringLiteral("apps"), appList);
    out.insert(QStringLiteral("totalSizeBytes"), QVariant::fromValue<qulonglong>(totalSize));
    out.insert(QStringLiteral("totalFiles"), QVariant::fromValue<qulonglong>(totalFiles));

    QVariantList suggestions;
    if (thumbnailSize > (500ull * 1024ull * 1024ull)) {
        suggestions.push_back(QStringLiteral("Thumbnail cache > 500MB"));
    }
    if (!appList.isEmpty()) {
        suggestions.push_back(QStringLiteral("Unused cache detected"));
    }
    out.insert(QStringLiteral("smartSuggestions"), suggestions);
    return out;
}

} // namespace Slm::Cleaner

