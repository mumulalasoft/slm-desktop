#pragma once

#include "IPdfRenderer.h"

namespace Slm::Print {

// PDF renderer backed by libpoppler-qt6.
// When SLM_HAVE_POPPLER_QT6 is not defined (Poppler not found at configure
// time) this class compiles but isAvailable() returns false and all methods
// degrade gracefully to the same output as FallbackPdfRenderer.
class PopplerPdfRenderer : public IPdfRenderer
{
public:
    PopplerPdfRenderer() = default;

    bool isAvailable() const override;
    int queryPageCount(const QString &filePath) override;
    QImage renderPage(const QString &filePath,
                      int pageIndex,
                      int dpi,
                      const QSize &maxViewport) override;

private:
    QImage scaledToViewport(QImage &&src, const QSize &maxViewport) const;
};

} // namespace Slm::Print
