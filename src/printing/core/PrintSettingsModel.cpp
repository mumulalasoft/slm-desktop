#include "PrintSettingsModel.h"

namespace Slm::Print {

namespace {
bool ticketsEqual(const PrintTicket &lhs, const PrintTicket &rhs)
{
    return lhs.printerId == rhs.printerId && lhs.copies == rhs.copies && lhs.pageRange == rhs.pageRange
           && lhs.paperSize == rhs.paperSize && lhs.orientation == rhs.orientation && lhs.duplex == rhs.duplex
           && lhs.colorMode == rhs.colorMode && lhs.quality == rhs.quality && qFuzzyCompare(lhs.scale, rhs.scale)
           && lhs.mediaSource == rhs.mediaSource && lhs.resolutionDpi == rhs.resolutionDpi
           && lhs.pluginFeatures == rhs.pluginFeatures;
}
} // namespace

PrintSettingsModel::PrintSettingsModel(QObject *parent)
    : QObject(parent)
{
    resetDefaults();
}

void PrintSettingsModel::setPrinterId(const QString &value)
{
    if (m_ticket.printerId == value) {
        return;
    }
    m_ticket.printerId = value;
    emit settingsChanged();
}

void PrintSettingsModel::setCopies(int value)
{
    const int sanitized = qMax(1, value);
    if (m_ticket.copies == sanitized) {
        return;
    }
    m_ticket.copies = sanitized;
    emit settingsChanged();
}

void PrintSettingsModel::setPageRange(const QString &value)
{
    if (m_ticket.pageRange == value) {
        return;
    }
    m_ticket.pageRange = value;
    emit settingsChanged();
}

void PrintSettingsModel::setPaperSize(const QString &value)
{
    if (m_ticket.paperSize == value || value.trimmed().isEmpty()) {
        return;
    }
    m_ticket.paperSize = value;
    emit settingsChanged();
}

void PrintSettingsModel::setOrientation(const QString &value)
{
    const Orientation parsed = orientationFromString(value);
    if (m_ticket.orientation == parsed) {
        return;
    }
    m_ticket.orientation = parsed;
    emit settingsChanged();
}

void PrintSettingsModel::setDuplex(const QString &value)
{
    const DuplexMode parsed = duplexFromString(value);
    if (m_ticket.duplex == parsed) {
        return;
    }
    m_ticket.duplex = parsed;
    emit settingsChanged();
}

void PrintSettingsModel::setColorMode(const QString &value)
{
    const ColorMode parsed = colorModeFromString(value);
    if (m_ticket.colorMode == parsed) {
        return;
    }
    m_ticket.colorMode = parsed;
    emit settingsChanged();
}

void PrintSettingsModel::setQuality(const QString &value)
{
    if (m_ticket.quality == value || value.trimmed().isEmpty()) {
        return;
    }
    m_ticket.quality = value;
    emit settingsChanged();
}

void PrintSettingsModel::setScale(double value)
{
    const double sanitized = clampScale(value);
    if (qFuzzyCompare(m_ticket.scale, sanitized)) {
        return;
    }
    m_ticket.scale = sanitized;
    emit settingsChanged();
}

void PrintSettingsModel::setMediaSource(const QString &value)
{
    const QString sanitized = value.trimmed().isEmpty() ? QStringLiteral("auto") : value.trimmed();
    if (m_ticket.mediaSource == sanitized) {
        return;
    }
    m_ticket.mediaSource = sanitized;
    emit settingsChanged();
}

void PrintSettingsModel::setResolutionDpi(int value)
{
    const int sanitized = qMax(0, value);
    if (m_ticket.resolutionDpi == sanitized) {
        return;
    }
    m_ticket.resolutionDpi = sanitized;
    emit settingsChanged();
}

void PrintSettingsModel::setPluginFeatures(const QVariantMap &value)
{
    if (m_ticket.pluginFeatures == value) {
        return;
    }
    m_ticket.pluginFeatures = value;
    emit settingsChanged();
}

void PrintSettingsModel::resetDefaults()
{
    const PrintTicket before = m_ticket;
    m_ticket = PrintTicket {};
    emitIfChanged(before);
}

QVariantMap PrintSettingsModel::serialize() const
{
    QVariantMap payload;
    payload.insert(QStringLiteral("printerId"), m_ticket.printerId);
    payload.insert(QStringLiteral("copies"), m_ticket.copies);
    payload.insert(QStringLiteral("pageRange"), m_ticket.pageRange);
    payload.insert(QStringLiteral("paperSize"), m_ticket.paperSize);
    payload.insert(QStringLiteral("orientation"), toString(m_ticket.orientation));
    payload.insert(QStringLiteral("duplex"), toString(m_ticket.duplex));
    payload.insert(QStringLiteral("colorMode"), toString(m_ticket.colorMode));
    payload.insert(QStringLiteral("quality"), m_ticket.quality);
    payload.insert(QStringLiteral("scale"), m_ticket.scale);
    payload.insert(QStringLiteral("mediaSource"), m_ticket.mediaSource);
    payload.insert(QStringLiteral("resolutionDpi"), m_ticket.resolutionDpi);
    payload.insert(QStringLiteral("pluginFeatures"), m_ticket.pluginFeatures);
    return payload;
}

void PrintSettingsModel::deserialize(const QVariantMap &payload)
{
    const PrintTicket before = m_ticket;
    m_ticket.printerId = payload.value(QStringLiteral("printerId"), m_ticket.printerId).toString();
    m_ticket.copies = qMax(1, payload.value(QStringLiteral("copies"), m_ticket.copies).toInt());
    m_ticket.pageRange = payload.value(QStringLiteral("pageRange"), m_ticket.pageRange).toString();
    m_ticket.paperSize = payload.value(QStringLiteral("paperSize"), m_ticket.paperSize).toString();
    m_ticket.orientation = orientationFromString(payload.value(QStringLiteral("orientation"), toString(m_ticket.orientation)).toString());
    m_ticket.duplex = duplexFromString(payload.value(QStringLiteral("duplex"), toString(m_ticket.duplex)).toString());
    m_ticket.colorMode = colorModeFromString(payload.value(QStringLiteral("colorMode"), toString(m_ticket.colorMode)).toString());
    m_ticket.quality = payload.value(QStringLiteral("quality"), m_ticket.quality).toString();
    m_ticket.scale = clampScale(payload.value(QStringLiteral("scale"), m_ticket.scale).toDouble());
    m_ticket.mediaSource = payload.value(QStringLiteral("mediaSource"), m_ticket.mediaSource).toString();
    if (m_ticket.mediaSource.trimmed().isEmpty()) m_ticket.mediaSource = QStringLiteral("auto");
    m_ticket.resolutionDpi = qMax(0, payload.value(QStringLiteral("resolutionDpi"), m_ticket.resolutionDpi).toInt());
    m_ticket.pluginFeatures = payload.value(QStringLiteral("pluginFeatures"), m_ticket.pluginFeatures).toMap();
    emitIfChanged(before);
}

void PrintSettingsModel::applyCapability(const PrinterCapability &capability)
{
    const PrintTicket before = m_ticket;
    if (!capability.supportsColor) {
        m_ticket.colorMode = ColorMode::Grayscale;
    }
    if (!capability.supportsDuplex) {
        m_ticket.duplex = DuplexMode::Off;
    }
    if (!capability.paperSizes.isEmpty() && !capability.paperSizes.contains(m_ticket.paperSize)) {
        m_ticket.paperSize = capability.paperSizes.first();
    }
    if (!capability.qualityModes.isEmpty() && !capability.qualityModes.contains(m_ticket.quality)) {
        m_ticket.quality = capability.qualityModes.first();
    }
    emitIfChanged(before);
}

void PrintSettingsModel::setTicket(const PrintTicket &ticket)
{
    const PrintTicket before = m_ticket;
    m_ticket = ticket;
    m_ticket.copies = qMax(1, m_ticket.copies);
    m_ticket.scale = clampScale(m_ticket.scale);
    emitIfChanged(before);
}

void PrintSettingsModel::emitIfChanged(const PrintTicket &before)
{
    if (!ticketsEqual(before, m_ticket)) {
        emit settingsChanged();
    }
}

double PrintSettingsModel::clampScale(double value)
{
    if (value < 10.0) {
        return 10.0;
    }
    if (value > 400.0) {
        return 400.0;
    }
    return value;
}

} // namespace Slm::Print
