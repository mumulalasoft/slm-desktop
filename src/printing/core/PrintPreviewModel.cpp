#include "PrintPreviewModel.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>

namespace Slm::Print {

PrintPreviewModel::PrintPreviewModel(QObject *parent)
    : QObject(parent)
{
}

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

} // namespace Slm::Print
