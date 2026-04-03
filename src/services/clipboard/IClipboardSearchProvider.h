#pragma once

#include "ClipboardSearchContract.h"

#include <QString>
#include <QVector>

namespace Slm::Clipboard {

class IClipboardSearchProvider
{
public:
    virtual ~IClipboardSearchProvider() = default;

    virtual QVector<ClipboardSearchSummary> queryPreview(const QString &query,
                                                         const ClipboardSearchOptions &options) const = 0;
    virtual ClipboardResolvedItem resolveResult(const QString &resultId,
                                                const ClipboardResolveContext &context) const = 0;
    virtual QVector<ClipboardSearchSummary> recentPreview(int limit,
                                                          const ClipboardSearchOptions &options) const = 0;
};

} // namespace Slm::Clipboard
