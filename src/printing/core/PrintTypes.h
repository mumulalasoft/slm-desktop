#pragma once

#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QList>

// ── Naming contract ────────────────────────────────────────────────────────────
// All public string values use neutral desktop product language, not IPP/CUPS terms.
// The adapter layer (src/printing/adapter/) is solely responsible for mapping
// these neutral values to backend-specific protocol attributes.
//
// Neutral → IPP mapping reference (internal to adapter):
//   colorMode:  "color"       → "color"
//               "grayscale"   → "monochrome"
//   duplex:     "off"         → "one-sided"
//               "long-edge"   → "two-sided-long-edge"
//               "short-edge"  → "two-sided-short-edge"
//   quality:    "draft"       → print-quality=3
//               "standard"    → print-quality=4
//               "high"        → print-quality=5
// ──────────────────────────────────────────────────────────────────────────────

namespace Slm::Print {

// ── Orientation ───────────────────────────────────────────────────────────────

enum class Orientation {
    Portrait,
    Landscape
};

// ── Duplex mode ───────────────────────────────────────────────────────────────

enum class DuplexMode {
    Off,         // single-sided
    LongEdge,    // two-sided, flip on long edge (book-style)
    ShortEdge    // two-sided, flip on short edge (notepad-style)
};

// ── Color mode ────────────────────────────────────────────────────────────────

enum class ColorMode {
    Color,
    Grayscale
};

// ── Printer status (neutral — no IPP state integers) ─────────────────────────

enum class PrinterStatus {
    Ready,
    Printing,
    Paused,
    Unavailable,
    Error,
    Unknown
};

// ── Print job status (neutral — no IPP job-state integers) ───────────────────

enum class PrintJobStatus {
    Queued,
    Preparing,
    Printing,
    Paused,
    Cancelling,
    Complete,
    Failed,
    Unknown
};

// ── Paper source ──────────────────────────────────────────────────────────────

struct MediaSource {
    QString id;           // opaque internal key (adapter translates to IPP)
    QString label;        // display label: "Main Tray", "Manual Feed"
};

// ── Printer capability (neutral — no IPP attribute names) ────────────────────

struct PrinterCapability {
    QString printerId;

    // Output modes
    bool supportsPdfDirect = true;  // internal: adapter uses PDF-direct path
    bool supportsColor     = true;
    bool supportsDuplex    = true;
    bool supportsStaple    = false;
    bool supportsPunch     = false;

    // Paper
    QStringList paperSizes;         // ISO/ANSI names: "A4", "Letter", "A3"

    // Quality levels: "draft", "standard", "high"
    QStringList qualityModes;

    QList<int>          resolutionsDpi;
    QList<MediaSource>  paperSources;

    // Vendor/plugin features (declarative descriptor JSON stored here)
    QVariantMap vendorExtensions;
};

// ── Print settings (neutral public model) ────────────────────────────────────
// These are the canonical settings. The adapter maps them to backend attributes.

struct PrintTicket {
    QString printerId;
    int     copies      = 1;
    QString pageRange;                               // "" = all pages
    QString paperSize   = QStringLiteral("A4");
    Orientation orientation = Orientation::Portrait;
    DuplexMode  duplex      = DuplexMode::Off;
    ColorMode   colorMode   = ColorMode::Color;
    QString quality     = QStringLiteral("standard"); // "draft"/"standard"/"high"
    double  scale       = 100.0;
    QString paperSource = QStringLiteral("auto");
    int     resolutionDpi = 0;                       // 0 = printer default
    QVariantMap pluginFeatures;
};

// Type alias — prefer this name in new code.
using PrintSettings = PrintTicket;

// ── String conversion (neutral values) ───────────────────────────────────────

inline QString toString(Orientation value)
{
    return value == Orientation::Landscape
        ? QStringLiteral("landscape")
        : QStringLiteral("portrait");
}

inline QString toString(DuplexMode value)
{
    switch (value) {
    case DuplexMode::LongEdge:  return QStringLiteral("long-edge");
    case DuplexMode::ShortEdge: return QStringLiteral("short-edge");
    default:                    return QStringLiteral("off");
    }
}

inline QString toString(ColorMode value)
{
    return value == ColorMode::Grayscale
        ? QStringLiteral("grayscale")
        : QStringLiteral("color");
}

inline QString toString(PrinterStatus value)
{
    switch (value) {
    case PrinterStatus::Ready:       return QStringLiteral("ready");
    case PrinterStatus::Printing:    return QStringLiteral("printing");
    case PrinterStatus::Paused:      return QStringLiteral("paused");
    case PrinterStatus::Unavailable: return QStringLiteral("unavailable");
    case PrinterStatus::Error:       return QStringLiteral("error");
    default:                         return QStringLiteral("unknown");
    }
}

inline QString toString(PrintJobStatus value)
{
    switch (value) {
    case PrintJobStatus::Queued:      return QStringLiteral("queued");
    case PrintJobStatus::Preparing:   return QStringLiteral("preparing");
    case PrintJobStatus::Printing:    return QStringLiteral("printing");
    case PrintJobStatus::Paused:      return QStringLiteral("paused");
    case PrintJobStatus::Cancelling:  return QStringLiteral("cancelling");
    case PrintJobStatus::Complete:    return QStringLiteral("complete");
    case PrintJobStatus::Failed:      return QStringLiteral("failed");
    default:                          return QStringLiteral("unknown");
    }
}

// ── fromString helpers (accept both new neutral and legacy IPP values) ────────

inline Orientation orientationFromString(const QString &value)
{
    return value.trimmed().compare(QStringLiteral("landscape"), Qt::CaseInsensitive) == 0
        ? Orientation::Landscape
        : Orientation::Portrait;
}

inline DuplexMode duplexFromString(const QString &value)
{
    const QString lower = value.trimmed().toLower();
    // New neutral values
    if (lower == QStringLiteral("long-edge"))  return DuplexMode::LongEdge;
    if (lower == QStringLiteral("short-edge")) return DuplexMode::ShortEdge;
    // Legacy IPP values (stored settings backward-compat)
    if (lower == QStringLiteral("two-sided-long-edge"))  return DuplexMode::LongEdge;
    if (lower == QStringLiteral("two-sided-short-edge")) return DuplexMode::ShortEdge;
    return DuplexMode::Off;
}

inline ColorMode colorModeFromString(const QString &value)
{
    const QString lower = value.trimmed().toLower();
    // New neutral value
    if (lower == QStringLiteral("grayscale")) return ColorMode::Grayscale;
    // Legacy IPP value (backward-compat)
    if (lower == QStringLiteral("monochrome")) return ColorMode::Grayscale;
    return ColorMode::Color;
}

} // namespace Slm::Print

Q_DECLARE_METATYPE(Slm::Print::Orientation)
Q_DECLARE_METATYPE(Slm::Print::DuplexMode)
Q_DECLARE_METATYPE(Slm::Print::ColorMode)
Q_DECLARE_METATYPE(Slm::Print::PrinterStatus)
Q_DECLARE_METATYPE(Slm::Print::PrintJobStatus)
Q_DECLARE_METATYPE(Slm::Print::MediaSource)
Q_DECLARE_METATYPE(Slm::Print::PrinterCapability)
Q_DECLARE_METATYPE(Slm::Print::PrintTicket)
