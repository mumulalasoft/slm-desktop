#pragma once

// ── CUPS ISOLATION BOUNDARY ────────────────────────────────────────────────
// This file and all files in adapter/ are the ONLY place in the codebase
// that are permitted to include CUPS headers or reference CUPS types.
//
// DO NOT include this header from any file outside src/printing/adapter/.
// All callers outside adapter/ must use IBackendAdapter* exclusively.
// ──────────────────────────────────────────────────────────────────────────

#include "../core/IBackendAdapter.h"
#include "CupsErrorTranslator.h"

#include <QObject>
#include <memory>

namespace Slm::Print::Adapter {

// CupsAdapter — concrete implementation of IBackendAdapter backed by CUPS.
//
// Wraps lpstat-based printer discovery (CupsDestResolver), IPP capability
// queries (IppCapabilityMapper), and job submission / status monitoring.
//
// This class is instantiated once by the print service startup code and
// handed to core layer objects as an IBackendAdapter*. No other code
// outside adapter/ holds a reference to this concrete type.
class CupsAdapter : public QObject,
                    public Desktop::Print::Internal::IBackendAdapter
{
    Q_OBJECT

public:
    explicit CupsAdapter(QObject *parent = nullptr);
    ~CupsAdapter() override;

    // ── IBackendAdapter ──────────────────────────────────────────────────

    QVariantList listPrinters()                                    override;
    QString      defaultPrinterId()                                override;

    Slm::Print::PrinterCapability queryCapability(const QString &printerId) override;
    Slm::Print::PrinterStatus     printerStatus(const QString &printerId)   override;

    QString submitJob(const Slm::Print::PrintSettings &settings,
                      const QString &documentUri,
                      QString &errorOut)                           override;

    bool cancelJob(const QString &jobHandle)                       override;
    bool pauseJob(const QString &jobHandle)                        override;
    bool resumeJob(const QString &jobHandle)                       override;

    Slm::Print::PrintJobStatus jobStatus(const QString &jobHandle) override;
    QVariantList               activeJobs()                        override;

    // ── CUPS-specific (not part of IBackendAdapter) ───────────────────
    // Called by the print service to refresh the printer list.
    void reloadPrinters();
    bool isPrintingAvailable() const;

signals:
    void printingAvailableChanged(bool available);
    void printersChanged();
    void jobStatusChanged(const QString &jobHandle,
                          Slm::Print::PrintJobStatus status,
                          const QString &detail);

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace Slm::Print::Adapter
