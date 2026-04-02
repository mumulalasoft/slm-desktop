#pragma once

#include <QString>

// CupsErrorTranslator — internal to src/printing/adapter/ only.
//
// Translates CUPS / IPP error conditions into user-facing desktop strings.
// Output strings must contain no references to "CUPS", "IPP", protocol
// identifiers, or numeric error codes. They must be suitable for direct
// display in the print dialog error banner or system notification.
//
// This header must NOT be included from any file outside adapter/.
namespace Slm::Print::Adapter {

class CupsErrorTranslator
{
public:
    // Translate a CUPS/IPP status code (ipp_status_e integer value) to a
    // user-facing message string.
    static QString fromIppStatus(int ippStatus);

    // Translate a printer-state-reasons token (e.g. "media-jam",
    // "toner-low") to a user-facing message string.
    // Returns empty string for unknown/benign reasons.
    static QString fromPrinterStateReason(const QString &reason);

    // Translate a QProcess / socket connection failure (cupsd not running or
    // unreachable) to a user-facing message string.
    static QString connectionFailed();
};

} // namespace Slm::Print::Adapter
