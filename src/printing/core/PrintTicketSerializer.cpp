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
    payload.insert(QStringLiteral("mediaSource"), ticket.mediaSource);
    payload.insert(QStringLiteral("resolutionDpi"), ticket.resolutionDpi);
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
    ticket.duplex = duplexFromString(payload.value(QStringLiteral("duplex"), QStringLiteral("off")).toString());
    ticket.colorMode = colorModeFromString(payload.value(QStringLiteral("colorMode"), QStringLiteral("color")).toString());
    ticket.quality = payload.value(QStringLiteral("quality"), QStringLiteral("standard")).toString();
    ticket.scale = payload.value(QStringLiteral("scale"), 100.0).toDouble();
    ticket.mediaSource = payload.value(QStringLiteral("mediaSource"), QStringLiteral("auto")).toString();
    ticket.resolutionDpi = qMax(0, payload.value(QStringLiteral("resolutionDpi"), 0).toInt());
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
    // Map neutral public values → IPP attribute values.
    // This is the sole location where neutral → IPP translation occurs.
    const QString ippSides = [&]() -> QString {
        switch (ticket.duplex) {
        case DuplexMode::LongEdge:  return QStringLiteral("two-sided-long-edge");
        case DuplexMode::ShortEdge: return QStringLiteral("two-sided-short-edge");
        default:                    return QStringLiteral("one-sided");
        }
    }();
    const QString ippColorMode =
        ticket.colorMode == ColorMode::Grayscale
            ? QStringLiteral("monochrome")
            : QStringLiteral("color");
    const QString ippQuality = [&]() -> QString {
        if (ticket.quality == QLatin1String("draft"))    return QStringLiteral("3");
        if (ticket.quality == QLatin1String("high"))     return QStringLiteral("5");
        return QStringLiteral("4"); // "standard" → IPP normal
    }();

    attrs.insert(QStringLiteral("sides"), ippSides);
    attrs.insert(QStringLiteral("print-color-mode"), ippColorMode);
    attrs.insert(QStringLiteral("print-quality"), ippQuality);
    attrs.insert(QStringLiteral("page-ranges"), ticket.pageRange);
    attrs.insert(QStringLiteral("print-scaling"), ticket.scale);
    // Only emit media-source when it differs from the default.
    if (!ticket.mediaSource.isEmpty() && ticket.mediaSource != QLatin1String("auto")) {
        attrs.insert(QStringLiteral("media-source"), ticket.mediaSource);
    }
    // Only emit resolution when explicitly set.
    if (ticket.resolutionDpi > 0) {
        attrs.insert(QStringLiteral("printer-resolution"),
                     QStringLiteral("%1dpi").arg(ticket.resolutionDpi));
    }

    for (auto it = ticket.pluginFeatures.constBegin(); it != ticket.pluginFeatures.constEnd(); ++it) {
        attrs.insert(QStringLiteral("x-slm-feature-%1").arg(it.key()), it.value());
    }
    return attrs;
}

} // namespace Slm::Print
