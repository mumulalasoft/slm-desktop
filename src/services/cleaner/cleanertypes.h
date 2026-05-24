#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

namespace Slm::Cleaner {

struct CacheNodeStat {
    QString name;
    QString path;
    quint64 sizeBytes = 0;
    quint64 fileCount = 0;
    qint64 lastAccessEpoch = 0;
};

struct CleanerPolicy {
    bool autoClean = false;
    int maxCacheSizeMb = 1024;
    int deleteAfterDays = 30;
};

struct CleanRequest {
    bool clearThumbnail = true;
    bool clearFailedThumbnail = true;
    QStringList selectedAppPaths;
    QString mode = QStringLiteral("full"); // full, age, smart
    int deleteAfterDays = 30;
    bool previewOnly = false;
};

inline bool isValidMode(const QString &mode)
{
    return mode == QLatin1String("full")
            || mode == QLatin1String("age")
            || mode == QLatin1String("smart");
}

} // namespace Slm::Cleaner
