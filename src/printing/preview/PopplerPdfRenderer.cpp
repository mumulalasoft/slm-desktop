#include "PopplerPdfRenderer.h"
#include "FallbackPdfRenderer.h"

#ifdef SLM_HAVE_POPPLER_QT6
#include <poppler/qt6/poppler-qt6.h>
#endif

#include <QUrl>

namespace Slm::Print {

bool PopplerPdfRenderer::isAvailable() const
{
#ifdef SLM_HAVE_POPPLER_QT6
    return true;
#else
    return false;
#endif
}

int PopplerPdfRenderer::queryPageCount(const QString &filePath)
{
#ifdef SLM_HAVE_POPPLER_QT6
    const QUrl url(filePath);
    const QString localPath = url.isLocalFile() ? url.toLocalFile() : filePath;
    auto doc = Poppler::Document::load(localPath);
    if (!doc || doc->isLocked()) {
        return 1;
    }
    return qMax(1, doc->numPages());
#else
    return FallbackPdfRenderer().queryPageCount(filePath);
#endif
}

QImage PopplerPdfRenderer::renderPage(const QString &filePath,
                                      int pageIndex,
                                      int dpi,
                                      const QSize &maxViewport)
{
#ifdef SLM_HAVE_POPPLER_QT6
    const QUrl url(filePath);
    const QString localPath = url.isLocalFile() ? url.toLocalFile() : filePath;

    auto doc = Poppler::Document::load(localPath);
    if (!doc || doc->isLocked()) {
        return FallbackPdfRenderer().renderPage(filePath, pageIndex, dpi, maxViewport);
    }
    doc->setRenderHint(Poppler::Document::TextAntialiasing);
    doc->setRenderHint(Poppler::Document::Antialiasing);

    const int clampedIndex = qBound(0, pageIndex, doc->numPages() - 1);
    auto page = doc->page(clampedIndex);
    if (!page) {
        return FallbackPdfRenderer().renderPage(filePath, pageIndex, dpi, maxViewport);
    }

    const int effectiveDpi = (dpi > 0) ? dpi : 150;
    QImage rendered = page->renderToImage(effectiveDpi, effectiveDpi);
    if (rendered.isNull()) {
        return FallbackPdfRenderer().renderPage(filePath, pageIndex, dpi, maxViewport);
    }
    return scaledToViewport(std::move(rendered), maxViewport);
#else
    return FallbackPdfRenderer().renderPage(filePath, pageIndex, dpi, maxViewport);
#endif
}

QImage PopplerPdfRenderer::scaledToViewport(QImage &&src, const QSize &maxViewport) const
{
    if (!maxViewport.isValid() || src.isNull()) {
        return std::move(src);
    }
    if (src.width() <= maxViewport.width() && src.height() <= maxViewport.height()) {
        return std::move(src);
    }
    return src.scaled(maxViewport, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

} // namespace Slm::Print
