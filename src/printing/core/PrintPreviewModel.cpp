#include "PrintPreviewModel.h"

#include "../preview/FallbackPdfRenderer.h"
#include "../preview/IPdfRenderer.h"
#include "../preview/PrintPreviewCache.h"

#include <QBuffer>
#include <QByteArray>
#include <QCryptographicHash>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QPointer>
#include <QThreadPool>

namespace Slm::Print {

PrintPreviewModel::PrintPreviewModel(QObject *parent)
    : QObject(parent)
    , m_cache(std::make_unique<PrintPreviewCache>(24))
{
}

PrintPreviewModel::~PrintPreviewModel() = default;

QString PrintPreviewModel::previewCacheKey() const
{
    return QStringLiteral("%1|p=%2|z=%3|f=%4|d=%5")
        .arg(m_documentUri)
        .arg(m_currentPage)
        .arg(m_zoomMode)
        .arg(m_zoomFactor, 0, 'f', 3)
        .arg(m_settingsDigest);
}

void PrintPreviewModel::setDocumentUri(const QString &uri)
{
    if (m_documentUri == uri) {
        return;
    }
    m_documentUri = uri;
    emit previewChanged();
}

void PrintPreviewModel::setTotalPages(int totalPages)
{
    const int nextTotalPages = qMax(1, totalPages);
    if (m_totalPages == nextTotalPages) {
        return;
    }
    m_totalPages = nextTotalPages;
    m_currentPage = boundedPage(m_currentPage);
    emit previewChanged();
}

void PrintPreviewModel::setCurrentPage(int page)
{
    const int nextPage = boundedPage(page);
    if (m_currentPage == nextPage) {
        return;
    }
    m_currentPage = nextPage;
    emit previewChanged();
}

void PrintPreviewModel::setZoomMode(const QString &mode)
{
    const QString normalized = normalizeZoomMode(mode);
    if (m_zoomMode == normalized) {
        return;
    }
    m_zoomMode = normalized;
    emit previewChanged();
}

void PrintPreviewModel::setZoomFactor(double factor)
{
    const double next = clampZoomFactor(factor);
    if (qFuzzyCompare(m_zoomFactor, next)) {
        return;
    }
    m_zoomFactor = next;
    emit previewChanged();
}

void PrintPreviewModel::setLoading(bool loading)
{
    if (m_loading == loading) {
        return;
    }
    m_loading = loading;
    emit loadingChanged();
}

void PrintPreviewModel::setErrorString(const QString &error)
{
    if (m_errorString == error) {
        return;
    }
    m_errorString = error;
    emit errorChanged();
}

void PrintPreviewModel::nextPage()
{
    setCurrentPage(m_currentPage + 1);
}

void PrintPreviewModel::previousPage()
{
    setCurrentPage(m_currentPage - 1);
}

void PrintPreviewModel::reset()
{
    m_documentUri.clear();
    m_totalPages = 1;
    m_currentPage = 1;
    m_zoomMode = QStringLiteral("fitPage");
    m_zoomFactor = 1.0;
    m_loading = false;
    m_errorString.clear();
    m_settingsDigest.clear();
    emit previewChanged();
    emit loadingChanged();
    emit errorChanged();
}

void PrintPreviewModel::applySettingsDigest(const QVariantMap &settingsPayload)
{
    const QJsonObject json = QJsonObject::fromVariantMap(settingsPayload);
    const QByteArray compact = QJsonDocument(json).toJson(QJsonDocument::Compact);
    const QString digest = QString::fromLatin1(
        QCryptographicHash::hash(compact, QCryptographicHash::Sha256).toHex());
    if (m_settingsDigest == digest) {
        return;
    }
    m_settingsDigest = digest;
    emit previewChanged();
}

int PrintPreviewModel::boundedPage(int page) const
{
    if (page < 1) {
        return 1;
    }
    if (page > m_totalPages) {
        return m_totalPages;
    }
    return page;
}

QString PrintPreviewModel::normalizeZoomMode(const QString &mode)
{
    const QString lower = mode.trimmed().toLower();
    if (lower == QStringLiteral("fitwidth")) {
        return QStringLiteral("fitWidth");
    }
    if (lower == QStringLiteral("custom")) {
        return QStringLiteral("custom");
    }
    return QStringLiteral("fitPage");
}

double PrintPreviewModel::clampZoomFactor(double value)
{
    if (value < 0.1) {
        return 0.1;
    }
    if (value > 5.0) {
        return 5.0;
    }
    return value;
}

// ── Render pipeline ──────────────────────────────────────────────────────────

void PrintPreviewModel::setRenderer(IPdfRenderer *renderer)
{
    m_renderer = renderer;
}

void PrintPreviewModel::requestRender(int dpi, int viewportWidth, int viewportHeight)
{
    if (m_documentUri.isEmpty()) {
        return;
    }

    // Increment serial — any in-flight worker with an older serial is discarded.
    const int serial = m_renderSerial.fetchAndAddRelaxed(1) + 1;
    setRendering(true);

    const QString uri = m_documentUri;
    const int pageIndex = m_currentPage - 1; // 0-indexed for renderer
    const QString cacheKey = previewCacheKey();
    const QSize viewport(viewportWidth, viewportHeight);
    const int effectiveDpi = (dpi > 0) ? dpi : 150;

    // Fast path: cache hit — no thread needed.
    {
        const QImage cached = m_cache->get(cacheKey);
        if (!cached.isNull()) {
            onRenderDone(serial, -1 /*page count unknown from cache*/, cached, cacheKey);
            return;
        }
    }

    // Choose renderer: injected one if available, fallback otherwise.
    IPdfRenderer *renderer = (m_renderer && m_renderer->isAvailable()) ? m_renderer : nullptr;
    PrintPreviewCache *cache = m_cache.get();
    QPointer<PrintPreviewModel> self(this);

    QThreadPool::globalInstance()->start([=]() {
        // --- Worker thread ---
        FallbackPdfRenderer fallback;
        IPdfRenderer *active = renderer ? renderer : &fallback;

        const int pageCount = active->queryPageCount(uri);
        const QImage image = active->renderPage(uri, pageIndex, effectiveDpi, viewport);

        // Return to main thread.
        QMetaObject::invokeMethod(self, [self, serial, pageCount, image, cacheKey, cache]() {
            if (!self) {
                return;
            }
            // Discard stale renders.
            if (self->m_renderSerial.loadRelaxed() != serial) {
                return;
            }
            cache->put(cacheKey, image);
            self->onRenderDone(serial, pageCount, image, cacheKey);
        }, Qt::QueuedConnection);
    });
}

void PrintPreviewModel::onRenderDone(int serial, int pageCount, const QImage &image,
                                     const QString & /*cacheKey*/)
{
    // Guard against races (invokeMethod can queue after another requestRender).
    if (serial >= 0 && m_renderSerial.loadRelaxed() != serial) {
        return;
    }

    // Update page count if the renderer returned a real value.
    if (pageCount > 0) {
        setTotalPages(pageCount);
    }

    // Encode image → data URI.
    if (image.isNull()) {
        setCurrentPageDataUrl(QString());
    } else {
        QByteArray buf;
        QBuffer buffer(&buf);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        const QString dataUrl = QStringLiteral("data:image/png;base64,")
                                + QString::fromLatin1(buf.toBase64());
        setCurrentPageDataUrl(dataUrl);
    }

    setRendering(false);
}

void PrintPreviewModel::setRendering(bool r)
{
    if (m_rendering == r) {
        return;
    }
    m_rendering = r;
    emit renderingChanged();
}

void PrintPreviewModel::setCurrentPageDataUrl(const QString &url)
{
    if (m_currentPageDataUrl == url) {
        return;
    }
    m_currentPageDataUrl = url;
    emit currentPageDataUrlChanged();
}

} // namespace Slm::Print
