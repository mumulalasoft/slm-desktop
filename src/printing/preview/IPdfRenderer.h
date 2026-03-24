#pragma once

#include <QImage>
#include <QSize>
#include <QString>

namespace Slm::Print {

// Stateless interface — each call re-opens the document internally.
// This is intentional: it makes renderer implementations trivially
// thread-safe when called from QThreadPool workers.
class IPdfRenderer
{
public:
    virtual ~IPdfRenderer() = default;

    // Returns true if this renderer can actually render (e.g. Poppler present).
    // FallbackPdfRenderer always returns false; PopplerPdfRenderer returns true
    // when libpoppler-qt6 was compiled in.
    virtual bool isAvailable() const = 0;

    // Returns the number of pages in the file, or 1 on error.
    virtual int queryPageCount(const QString &filePath) = 0;

    // Renders a single page (0-indexed). Returns a null QImage on error.
    // dpi controls resolution; maxViewport is a hint for scaling — the returned
    // image is scaled to fit within maxViewport while preserving aspect ratio.
    // Passing a null maxViewport skips scaling.
    virtual QImage renderPage(const QString &filePath,
                              int pageIndex,
                              int dpi,
                              const QSize &maxViewport) = 0;
};

} // namespace Slm::Print
