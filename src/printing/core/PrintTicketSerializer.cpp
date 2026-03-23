#include "PrintTicketSerializer.h"

namespace Slm::Print {

QVariantMap PrintTicketSerializer::toVariantMap(const PrintTicket &ticket)
{
    QVariantMap payload;
    payload.insert(QStringLiteral("printerId"), ticket.printerId);
    payload.insert(QStringLiteral("copies"), ticket.copies);
    payload.insert(QStringLiteral("pageRange"), ticket.pageRange);
    payload.insert(QStringLiteral("paperSize"), ticket.paperSize);
    payload.insert(QStringLiteral("orientation"), toString(ticket.orientation));
    payload.insert(QStringLiteral("duplex"), toString(ticket.duplex));
    payload.insert(QStringLiteral("colorMode"), toString(ticket.colorMode));
    payload.insert(QStringLiteral("quality"), ticket.quality);
    payload.insert(QStringLiteral("scale"), ticket.scale);
    payload.insert(QStringLiteral("pluginFeatures"), ticket.pluginFeatures);
    return payload;
}

PrintTicket PrintTicketSerializer::fromVariantMap(const QVariantMap &payload)
{
    PrintTicket ticket;
    ticket.printerId = payload.value(QStringLiteral("printerId")).toString();
    ticket.copies = qMax(1, payload.value(QStringLiteral("copies"), 1).toInt());
    ticket.pageRange = payload.value(QStringLiteral("pageRange")).toString();
    ticket.paperSize = payload.value(QStringLiteral("paperSize"), QStringLiteral("A4")).toString();
    ticket.orientation =
        orientationFromString(payload.value(QStringLiteral("orientation"), QStringLiteral("portrait")).toString());
    ticket.duplex = duplexFromString(payload.value(QStringLiteral("duplex"), QStringLiteral("one-sided")).toString());
    ticket.colorMode = colorModeFromString(payload.value(QStringLiteral("colorMode"), QStringLiteral("color")).toString());
    ticket.quality = payload.value(QStringLiteral("quality"), QStringLiteral("normal")).toString();
    ticket.scale = payload.value(QStringLiteral("scale"), 100.0).toDouble();
    ticket.pluginFeatures = payload.value(QStringLiteral("pluginFeatures")).toMap();
    return ticket;
}

QVariantMap PrintTicketSerializer::toIppAttributes(const PrintTicket &ticket)
{
    QVariantMap attrs;
    attrs.insert(QStringLiteral("copies"), qMax(1, ticket.copies));
    attrs.insert(QStringLiteral("media"), ticket.paperSize);
    attrs.insert(QStringLiteral("orientation-requested"),
                 ticket.orientation == Orientation::Landscape ? QStringLiteral("4") : QStringLiteral("3"));
    attrs.insert(QStringLiteral("sides"), toString(ticket.duplex));
    attrs.insert(QStringLiteral("print-color-mode"), toString(ticket.colorMode));
    attrs.insert(QStringLiteral("print-quality"), ticket.quality);
    attrs.insert(QStringLiteral("page-ranges"), ticket.pageRange);
    attrs.insert(QStringLiteral("print-scaling"), ticket.scale);

    for (auto it = ticket.pluginFeatures.constBegin(); it != ticket.pluginFeatures.constEnd(); ++it) {
        attrs.insert(QStringLiteral("x-slm-feature-%1").arg(it.key()), it.value());
    }
    return attrs;
}

} // namespace Slm::Print
