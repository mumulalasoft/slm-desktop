#pragma once

#include "cleanertypes.h"

#include <QString>
#include <QVector>

namespace Slm::Cleaner {

class CleanerScanner
{
public:
    static QString resolveCacheHome();
    static QString resolveThumbnailRoot();
    static QString resolveFailThumbnailRoot();

    QVector<CacheNodeStat> scanTopLevel(QString *errorMessage = nullptr) const;
    CacheNodeStat scanPathRecursive(const QString &path, const QString &name,
                                    QString *errorMessage = nullptr) const;
};

} // namespace Slm::Cleaner

