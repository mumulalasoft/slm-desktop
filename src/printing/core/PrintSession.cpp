#include "PrintSession.h"
#include "PrintJobBuilder.h"

#include <QUuid>

namespace Slm::Print {

PrintSession::PrintSession(QObject *parent)
    : QObject(parent)
    , m_settingsModel(new PrintSettingsModel(this))
{
}

void PrintSession::setDocumentUri(const QString &uri)
{
    if (m_documentUri == uri) {
        return;
    }
    m_documentUri = uri;
    emit sessionChanged();
}

void PrintSession::setDocumentTitle(const QString &title)
{
    if (m_documentTitle == title) {
        return;
    }
    m_documentTitle = title;
    emit sessionChanged();
}

void PrintSession::setStatus(SessionStatus status)
{
    if (m_status == status) {
        return;
    }
    m_status = status;
    emit statusChanged();
}

void PrintSession::setPrinterCapability(const QVariantMap &capability)
{
    if (m_printerCapabilityRaw == capability) {
        return;
    }
    m_printerCapabilityRaw = capability;
    m_settingsModel->applyCapability(parseCapabilityMap(capability));
    emit printerCapabilityChanged();
}

void PrintSession::begin(const QString &documentUri, const QString &documentTitle)
{
    m_sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_documentUri = documentUri;
    m_documentTitle = documentTitle;
    m_status = SessionStatus::Prepared;
    emit sessionChanged();
    emit statusChanged();
}

void PrintSession::reset()
{
    m_sessionId.clear();
    m_documentUri.clear();
    m_documentTitle.clear();
    m_printerCapabilityRaw.clear();
    m_settingsModel->resetDefaults();
    m_status = SessionStatus::Idle;
    emit sessionChanged();
    emit statusChanged();
    emit printerCapabilityChanged();
}

QVariantMap PrintSession::buildTicketPayload() const
{
    return PrintTicketSerializer::toVariantMap(m_settingsModel->ticket());
}

QVariantMap PrintSession::buildIppAttributes() const
{
    return PrintTicketSerializer::toIppAttributes(m_settingsModel->ticket());
}

QVariantMap PrintSession::buildJobPayload() const
{
    return PrintJobBuilder::build(this);
}

void PrintSession::updateSettingsFromPayload(const QVariantMap &payload)
{
    m_settingsModel->deserialize(payload);
}

PrinterCapability PrintSession::parseCapabilityMap(const QVariantMap &capabilityMap)
{
    PrinterCapability capability;
    capability.printerId = capabilityMap.value(QStringLiteral("printerId")).toString();
    capability.supportsPdfDirect = capabilityMap.value(QStringLiteral("supportsPdfDirect"), true).toBool();
    capability.supportsColor = capabilityMap.value(QStringLiteral("supportsColor"), true).toBool();
    capability.supportsDuplex = capabilityMap.value(QStringLiteral("supportsDuplex"), true).toBool();
    capability.paperSizes = capabilityMap.value(QStringLiteral("paperSizes")).toStringList();
    capability.qualityModes = capabilityMap.value(QStringLiteral("qualityModes")).toStringList();
    capability.vendorExtensions = capabilityMap.value(QStringLiteral("vendorExtensions")).toMap();

    const QVariantList dpiValues = capabilityMap.value(QStringLiteral("resolutionsDpi")).toList();
    for (const QVariant &value : dpiValues) {
        capability.resolutionsDpi.append(value.toInt());
    }
    return capability;
}

} // namespace Slm::Print
