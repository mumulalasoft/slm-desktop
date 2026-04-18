#pragma once

#include "cleanertypes.h"

#include <QVariantMap>
#include <QVector>

namespace Slm::Cleaner {

class CleanerAnalyzer
{
public:
    QVariantMap analyze(const QVector<CacheNodeStat> &nodes) const;
};

} // namespace Slm::Cleaner

