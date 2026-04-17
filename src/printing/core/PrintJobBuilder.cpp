#include "PrintJobBuilder.h"

namespace Slm::Print {

namespace {
PrintTicket applyCapabilityConstraints(PrintTicket ticket, const QVariantMap &rawCapability)
{
    const bool supportsColor = rawCapability.value(QStringLiteral("supportsColor"), true).toBool();
    const bool supportsDuplex = rawCapability.value(QStringLiteral("supportsDuplex"), true).toBool();
    const QStringList paperSizes = rawCapability.value(QStringLiteral("paperSizes")).toStringList();
    const QStringList qualityModes = rawCapability.value(QStringLiteral("qualityModes")).toStringList();

    if (!supportsColor) {
        ticket.colorMode = ColorMode::Grayscale;
    }
    if (!supportsDuplex) {
        ticket.duplex = DuplexMode::Off;
    }
    if (!paperSizes.isEmpty() && !paperSizes.contains(ticket.paperSize)) {
        ticket.paperSize = paperSizes.first();
    }
    if (!qualityModes.isEmpty() && !qualityModes.contains(ticket.quality)) {
        ticket.quality = qualityModes.first();
    }
    ticket.copies = qMax(1, ticket.copies);
    return ticket;
}
} // namespace

QVariantMap PrintJobBuilder::build(const PrintSession *session)
{
    QVariantMap result;
    if (!session) {
        result.insert(QStringLiteral("success"), false);
        result.insert(QStringLiteral("error"), QStringLiteral("print session is null"));
        return result;
    }

    const PrintSettingsModel *settings = session->settingsModel();
    if (!settings) {
        result.insert(QStringLiteral("success"), false);
        result.insert(QStringLiteral("error"), QStringLiteral("settings model is missing"));
        return result;
    }

    PrintTicket ticket = settings->ticket();
    ticket = applyCapabilityConstraints(ticket, session->printerCapability());

    if (ticket.printerId.trimmed().isEmpty()) {
        result.insert(QStringLiteral("success"), false);
        result.insert(QStringLiteral("error"), QStringLiteral("printer is not selected"));
        return result;
    }
    if (session->documentUri().trimmed().isEmpty()) {
        result.insert(QStringLiteral("success"), false);
        result.insert(QStringLiteral("error"), QStringLiteral("document uri is empty"));
        return result;
    }

    result.insert(QStringLiteral("success"), true);
    result.insert(QStringLiteral("error"), QString());
    result.insert(QStringLiteral("documentUri"), session->documentUri());
    result.insert(QStringLiteral("documentTitle"), session->documentTitle());
    result.insert(QStringLiteral("printerId"), ticket.printerId);
    result.insert(QStringLiteral("ticket"), PrintTicketSerializer::toVariantMap(ticket));
    result.insert(QStringLiteral("jobAttributes"), PrintTicketSerializer::toIppAttributes(ticket));
    return result;
}

} // namespace Slm::Print
