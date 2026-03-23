#pragma once

#include <QObject>
#include <QVariantMap>

namespace Slm::Print {

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

public:
    explicit PrintPreviewModel(QObject *parent = nullptr);

    QString documentUri() const { return m_documentUri; }
    int totalPages() const { return m_totalPages; }
    int currentPage() const { return m_currentPage; }
    QString zoomMode() const { return m_zoomMode; } // fitPage, fitWidth, custom
    double zoomFactor() const { return m_zoomFactor; }
    bool loading() const { return m_loading; }
    QString errorString() const { return m_errorString; }
    QString previewCacheKey() const;

    void setDocumentUri(const QString &uri);
    void setTotalPages(int totalPages);
    void setCurrentPage(int page);
    void setZoomMode(const QString &mode);
    void setZoomFactor(double factor);
    void setLoading(bool loading);
    void setErrorString(const QString &error);

    Q_INVOKABLE void nextPage();
    Q_INVOKABLE void previousPage();
    Q_INVOKABLE void reset();
    Q_INVOKABLE void applySettingsDigest(const QVariantMap &settingsPayload);

signals:
    void previewChanged();
    void loadingChanged();
    void errorChanged();

private:
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
};

} // namespace Slm::Print
