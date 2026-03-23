#pragma once

#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QList>

namespace Slm::Print {

enum class Orientation {
    Portrait,
    Landscape
};

enum class DuplexMode {
    OneSided,
    LongEdge,
    ShortEdge
};

enum class ColorMode {
    Color,
    Monochrome
};

struct PrinterCapability {
    QString printerId;
    bool supportsPdfDirect = true;
    bool supportsColor = true;
    bool supportsDuplex = true;
    QStringList paperSizes;
    QStringList qualityModes;
    QList<int> resolutionsDpi;
    QVariantMap vendorExtensions;
};

struct PrintTicket {
    QString printerId;
    int copies = 1;
    QString pageRange;
    QString paperSize = QStringLiteral("A4");
    Orientation orientation = Orientation::Portrait;
    DuplexMode duplex = DuplexMode::OneSided;
    ColorMode colorMode = ColorMode::Color;
    QString quality = QStringLiteral("normal");
    double scale = 100.0;
    QVariantMap pluginFeatures;
};

inline QString toString(Orientation value)
{
    return value == Orientation::Landscape ? QStringLiteral("landscape") : QStringLiteral("portrait");
}

inline QString toString(DuplexMode value)
{
    switch (value) {
    case DuplexMode::LongEdge:
        return QStringLiteral("two-sided-long-edge");
    case DuplexMode::ShortEdge:
        return QStringLiteral("two-sided-short-edge");
    default:
        return QStringLiteral("one-sided");
    }
}

inline QString toString(ColorMode value)
{
    return value == ColorMode::Monochrome ? QStringLiteral("monochrome") : QStringLiteral("color");
}

inline Orientation orientationFromString(const QString &value)
{
    return value.trimmed().compare(QStringLiteral("landscape"), Qt::CaseInsensitive) == 0
               ? Orientation::Landscape
               : Orientation::Portrait;
}

inline DuplexMode duplexFromString(const QString &value)
{
    const QString lower = value.trimmed().toLower();
    if (lower == QStringLiteral("two-sided-long-edge")) {
        return DuplexMode::LongEdge;
    }
    if (lower == QStringLiteral("two-sided-short-edge")) {
        return DuplexMode::ShortEdge;
    }
    return DuplexMode::OneSided;
}

inline ColorMode colorModeFromString(const QString &value)
{
    return value.trimmed().compare(QStringLiteral("monochrome"), Qt::CaseInsensitive) == 0
               ? ColorMode::Monochrome
               : ColorMode::Color;
}

} // namespace Slm::Print

Q_DECLARE_METATYPE(Slm::Print::Orientation)
Q_DECLARE_METATYPE(Slm::Print::DuplexMode)
Q_DECLARE_METATYPE(Slm::Print::ColorMode)
Q_DECLARE_METATYPE(Slm::Print::PrinterCapability)
Q_DECLARE_METATYPE(Slm::Print::PrintTicket)
