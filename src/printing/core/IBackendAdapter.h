#pragma once

#include "PrintTypes.h"

#include <QList>
#include <QString>
#include <QVariantList>

namespace Desktop::Print::Internal {

// Pure virtual backend adapter interface.
//
// All printing core logic depends only on this interface — never on any
// concrete backend implementation (e.g. CupsAdapter).
//
// Implementations live exclusively in src/printing/adapter/ and must not
// be referenced from any file outside that directory.
//
// Naming contract:
//   - jobHandle  : opaque string — callers must not parse or log it as a
//                  backend job ID.
//   - printerId  : opaque string — callers must not assume it equals a
//                  backend queue name.
//   - All status values are Desktop::Print enums, not backend integers.
class IBackendAdapter
{
public:
    virtual ~IBackendAdapter() = default;

    // ── Printer discovery ─────────────────────────────────────────────

    // Returns the current list of available printers as neutral PrinterInfo maps.
    // Each map contains: id, displayName, location, modelName, status, isDefault,
    // isRemote, supportsColor, supportsDuplex.
    virtual QVariantList listPrinters() = 0;

    // Returns the opaque printer ID of the user's default printer, or empty string.
    virtual QString defaultPrinterId() = 0;

    // ── Capability ────────────────────────────────────────────────────

    // Returns a neutral PrinterCapability for the given printer.
    // Returns an empty / zeroed capability (with printerId set) on failure.
    virtual Slm::Print::PrinterCapability queryCapability(const QString &printerId) = 0;

    // Returns the current status of the printer as a neutral enum value.
    virtual Slm::Print::PrinterStatus printerStatus(const QString &printerId) = 0;

    // ── Job submission ────────────────────────────────────────────────

    // Submit a print job for the document at documentUri using the given settings.
    // Returns an opaque jobHandle string on success, or an empty string on failure.
    // errorOut is set to a user-facing error message on failure.
    virtual QString submitJob(const Slm::Print::PrintSettings &settings,
                              const QString &documentUri,
                              QString &errorOut) = 0;

    // ── Job control ───────────────────────────────────────────────────

    // Cancel the job identified by jobHandle. Returns true on success.
    virtual bool cancelJob(const QString &jobHandle) = 0;

    // Pause the job. Returns true on success.
    virtual bool pauseJob(const QString &jobHandle) = 0;

    // Resume a paused job. Returns true on success.
    virtual bool resumeJob(const QString &jobHandle) = 0;

    // ── Job monitoring ────────────────────────────────────────────────

    // Returns the current neutral status of a job.
    virtual Slm::Print::PrintJobStatus jobStatus(const QString &jobHandle) = 0;

    // Returns a list of currently active jobs as neutral PrintJobInfo maps.
    // Each map contains: jobHandle, documentTitle, printerDisplayName,
    // status (string from PrintJobStatus), pagesSent, totalPages, submittedAt.
    virtual QVariantList activeJobs() = 0;
};

} // namespace Desktop::Print::Internal
