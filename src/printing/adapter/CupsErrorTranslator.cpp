#include "CupsErrorTranslator.h"

// IPP status code constants (from cups/ipp.h) are reproduced here as plain
// integer literals so this file compiles without exposing cups headers to the
// rest of the codebase. Values taken from RFC 8011 / CUPS source.
//
// Only the subset needed for user-facing error mapping is listed.
namespace {

// IPP status code ranges (hex)
constexpr int kIppOk                        = 0x0000;
constexpr int kIppRedirectionOtherSite      = 0x0200;
constexpr int kIppClientErrorBadRequest     = 0x0400;
constexpr int kIppClientErrorForbidden      = 0x0401;
constexpr int kIppClientErrorNotAuth        = 0x0403;
constexpr int kIppClientErrorNotFound       = 0x0406;
constexpr int kIppClientErrorNotPossible    = 0x040b;
constexpr int kIppClientErrorTooLarge       = 0x0413;
constexpr int kIppServerErrorInternalError  = 0x0500;
constexpr int kIppServerErrorBusy           = 0x0507;
constexpr int kIppServerErrorNotAccepting   = 0x0508;

} // anonymous namespace

namespace Slm::Print::Adapter {

QString CupsErrorTranslator::fromIppStatus(int ippStatus)
{
    if (ippStatus == kIppOk)
        return QString(); // success — no error message needed

    // Not authorised
    if (ippStatus == kIppClientErrorForbidden
        || ippStatus == kIppClientErrorNotAuth)
    {
        return QStringLiteral(
            "You don\u2019t have permission to print on this printer. "
            "Contact your administrator if you think this is a mistake.");
    }

    // Printer or document not found
    if (ippStatus == kIppClientErrorNotFound)
    {
        return QStringLiteral(
            "The printer couldn\u2019t be found. "
            "Make sure it\u2019s connected and try again.");
    }

    // Operation not currently possible (e.g. printer paused)
    if (ippStatus == kIppClientErrorNotPossible)
    {
        return QStringLiteral(
            "The printer is not ready to accept jobs right now. "
            "Check the printer and try again.");
    }

    // Document too large
    if (ippStatus == kIppClientErrorTooLarge)
    {
        return QStringLiteral(
            "The document is too large to print. "
            "Try reducing the file size or printing fewer pages.");
    }

    // Server busy
    if (ippStatus == kIppServerErrorBusy)
    {
        return QStringLiteral(
            "The printer is busy. Your print job will be sent when it\u2019s ready.");
    }

    // Not accepting jobs
    if (ippStatus == kIppServerErrorNotAccepting)
    {
        return QStringLiteral(
            "The printer is not accepting print jobs at the moment. "
            "Check the printer status and try again.");
    }

    // Generic server-side failure
    if (ippStatus >= kIppServerErrorInternalError)
    {
        return QStringLiteral(
            "A printer error occurred. Check the printer and try again.");
    }

    // Generic client-side failure fallback
    return QStringLiteral("Printing failed. Check the printer and try again.");
}

QString CupsErrorTranslator::fromPrinterStateReason(const QString &reason)
{
    if (reason.isEmpty())
        return QString();

    // Paper feed issues
    if (reason.startsWith(QLatin1String("media-jam")))
        return QStringLiteral("Paper jam. Clear the paper path and try again.");

    if (reason.startsWith(QLatin1String("media-empty"))
        || reason.startsWith(QLatin1String("media-needed")))
        return QStringLiteral("Out of paper. Add paper to continue.");

    if (reason.startsWith(QLatin1String("media-low")))
        return QStringLiteral("Paper is running low.");

    // Consumables
    if (reason.startsWith(QLatin1String("toner-empty"))
        || reason.startsWith(QLatin1String("ink-empty")))
        return QStringLiteral("Ink or toner is empty. Replace the cartridge to continue.");

    if (reason.startsWith(QLatin1String("toner-low"))
        || reason.startsWith(QLatin1String("ink-low")))
        return QStringLiteral("Ink or toner is running low.");

    // Hardware
    if (reason.startsWith(QLatin1String("door-open"))
        || reason.startsWith(QLatin1String("cover-open")))
        return QStringLiteral("The printer cover is open. Close it and try again.");

    if (reason.startsWith(QLatin1String("output-area-full")))
        return QStringLiteral("The output tray is full. Remove the printed pages to continue.");

    // Connectivity
    if (reason.startsWith(QLatin1String("offline"))
        || reason.startsWith(QLatin1String("connecting-to-device")))
        return QStringLiteral("The printer is offline or unreachable. Check the connection.");

    if (reason.startsWith(QLatin1String("paused")))
        return QStringLiteral("The printer is paused.");

    // Benign / informational reasons — return nothing
    if (reason == QLatin1String("none"))
        return QString();

    return QString(); // unknown reason — don't surface raw token to user
}

QString CupsErrorTranslator::connectionFailed()
{
    return QStringLiteral("Printing is not available right now. "
                          "Check that your printer is connected and try again.");
}

} // namespace Slm::Print::Adapter
