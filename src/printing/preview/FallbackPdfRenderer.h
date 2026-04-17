#pragma once

#include "IPdfRenderer.h"

namespace Slm::Print {

// Used when libpoppler-qt6 is not available.
// queryPageCount always returns 1; renderPage returns a simple
// placeholder image with the filename and page number.
class FallbackPdfRenderer : public IPdfRenderer
{
public:
    FallbackPdfRenderer() = default;

    bool isAvailable() const override { return false; }
    int queryPageCount(const QString &filePath) override;
    QImage renderPage(const QString &filePath,
                      int pageIndex,
                      int dpi,
                      const QSize &maxViewport) override;
};

} // namespace Slm::Print
