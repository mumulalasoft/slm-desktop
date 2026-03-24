#pragma once

#include <QAtomicInt>
#include <QObject>
#include <QSize>
#include <QVariantMap>
#include <memory>

namespace Slm::Print {

class IPdfRenderer;
class PrintPreviewCache;

class PrintPreviewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString documentUri READ documentUri WRITE setDocumentUri NOTIFY previewChanged)
    Q_PROPERTY(int totalPages READ totalPages WRITE setTotalPages NOTIFY previewChanged)
    Q_PROPERTY(int currentPage READ currentPage WRITE setCurrentPage NOTIFY previewChanged)
    Q_PROPERTY(QString zoomMode READ zoomMode WRITE setZoomMode NOTIFY previewChanged)
    Q_PROPERTY(double zoomFactor READ zoomFactor WRITE setZoomFactor NOTIFY previewChanged)
    Q_PROPERTY(bool loading READ loading WRITE setLoading NOTIFY loadingChanged)
    Q_PROPERTY(QString errorString READ errorString WRITE setErrorString NOTIFY errorChanged)
    Q_PROPERTY(QString previewCacheKey READ previewCacheKey NOTIFY previewChanged)
    // Data URI ("data:image/png;base64,...") of the currently rendered page.
    // Empty while loading or when no document is open.
    Q_PROPERTY(QString currentPageDataUrl READ currentPageDataUrl NOTIFY currentPageDataUrlChanged)
    // True while an async render is running.
    Q_PROPERTY(bool rendering READ rendering NOTIFY renderingChanged)

public:
    explicit PrintPreviewModel(QObject *parent = nullptr);
    ~PrintPreviewModel() override;

    QString documentUri() const { return m_documentUri; }
    int totalPages() const { return m_totalPages; }
    int currentPage() const { return m_currentPage; }
    QString zoomMode() const { return m_zoomMode; } // fitPage, fitWidth, custom
    double zoomFactor() const { return m_zoomFactor; }
    bool loading() const { return m_loading; }
    QString errorString() const { return m_errorString; }
    QString previewCacheKey() const;
    QString currentPageDataUrl() const { return m_currentPageDataUrl; }
    bool rendering() const { return m_rendering; }

    void setDocumentUri(const QString &uri);
    void setTotalPages(int totalPages);
    void setCurrentPage(int page);
    void setZoomMode(const QString &mode);
    void setZoomFactor(double factor);
    void setLoading(bool loading);
    void setErrorString(const QString &error);

    // Inject the PDF renderer. Must be called before requestRender().
    // Ownership is NOT transferred — caller keeps the renderer alive.
    void setRenderer(IPdfRenderer *renderer);

    Q_INVOKABLE void nextPage();
    Q_INVOKABLE void previousPage();
    Q_INVOKABLE void reset();
    Q_INVOKABLE void applySettingsDigest(const QVariantMap &settingsPayload);

    // Trigger async render of the current page.
    // dpi: render resolution (96–300); viewportWidth/Height: target display size.
    // Emits currentPageDataUrlChanged() when done. Safe to call repeatedly —
    // stale renders are discarded via a serial counter.
    Q_INVOKABLE void requestRender(int dpi, int viewportWidth, int viewportHeight);

signals:
    void previewChanged();
    void loadingChanged();
    void errorChanged();
    void currentPageDataUrlChanged();
    void renderingChanged();

private:
    // Called from main thread when async worker finishes.
    void onRenderDone(int serial, int pageCount, const QImage &image, const QString &cacheKey);
    void setRendering(bool r);
    void setCurrentPageDataUrl(const QString &url);

    int boundedPage(int page) const;
    static QString normalizeZoomMode(const QString &mode);
    static double clampZoomFactor(double value);

    QString m_documentUri;
    int m_totalPages = 1;
    int m_currentPage = 1;
    QString m_zoomMode = QStringLiteral("fitPage");
    double m_zoomFactor = 1.0;
    bool m_loading = false;
    QString m_errorString;
    QString m_settingsDigest;

    // Render pipeline
    IPdfRenderer *m_renderer = nullptr; // not owned
    std::unique_ptr<PrintPreviewCache> m_cache;
    QAtomicInt m_renderSerial{0};
    QString m_currentPageDataUrl;
    bool m_rendering = false;
};

} // namespace Slm::Print
