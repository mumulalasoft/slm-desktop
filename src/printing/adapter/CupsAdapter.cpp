#include "CupsAdapter.h"
#include "CupsErrorTranslator.h"

// ── CUPS ISOLATION BOUNDARY ────────────────────────────────────────────────
// CUPS headers are allowed ONLY here and in other files inside adapter/.
// Any CUPS type that needs to cross the boundary must be translated to a
// neutral Desktop::Print type before leaving this file.
// ──────────────────────────────────────────────────────────────────────────

#include "../core/PrinterCapabilityProvider.h"
#include "../core/PrinterManager.h"
#include "../core/PrintTicketSerializer.h"

#include <QProcess>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcCupsAdapter, "slm.print.adapter.cups")

namespace Slm::Print::Adapter {

// ── Internal implementation ────────────────────────────────────────────────

struct CupsAdapter::Impl {
    // Reuse the existing PrinterManager and PrinterCapabilityProvider for
    // CUPS communication. These live inside adapter/ conceptually — they
    // contain the lpstat / IPP query logic.
    PrinterManager           *printerManager   = nullptr;
    PrinterCapabilityProvider capabilityProvider;

    // Map opaque jobHandle → CUPS integer job ID (internal only).
    // Callers never see the integer; they only hold the opaque string.
    QHash<QString, int> jobHandleToId;
    int nextHandle = 1;

    QString makeHandle(int cupsJobId) {
        const QString handle = QStringLiteral("job-%1").arg(nextHandle++);
        jobHandleToId[handle] = cupsJobId;
        return handle;
    }

    int resolveHandle(const QString &handle) const {
        return jobHandleToId.value(handle, -1);
    }

    static QString runCommand(const QString &program,
                              const QStringList &args,
                              int timeoutMs = 3000)
    {
        QProcess proc;
        proc.setProgram(program);
        proc.setArguments(args);
        proc.setProcessChannelMode(QProcess::MergedChannels);
        proc.start();
        if (!proc.waitForFinished(timeoutMs)
            || proc.exitStatus() != QProcess::NormalExit
            || proc.exitCode() != 0)
        {
            return {};
        }
        return QString::fromUtf8(proc.readAllStandardOutput());
    }
};

// ── Construction ──────────────────────────────────────────────────────────

CupsAdapter::CupsAdapter(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Impl>())
{
    d->printerManager = new PrinterManager(this);

    // Forward the renamed signal outward.
    connect(d->printerManager, &PrinterManager::printingAvailableChanged,
            this, [this](){ emit printingAvailableChanged(d->printerManager->printingAvailable()); });
    connect(d->printerManager, &PrinterManager::printersChanged,
            this, &CupsAdapter::printersChanged);
}

CupsAdapter::~CupsAdapter() = default;

// ── IBackendAdapter: printer discovery ────────────────────────────────────

QVariantList CupsAdapter::listPrinters()
{
    return d->printerManager->printers();
}

QString CupsAdapter::defaultPrinterId()
{
    return d->printerManager->defaultPrinterId();
}

// ── IBackendAdapter: capability ───────────────────────────────────────────

Slm::Print::PrinterCapability CupsAdapter::queryCapability(const QString &printerId)
{
    const QVariantMap capMap = d->printerManager->capabilities(printerId);

    // Translate the QVariantMap produced by PrinterCapabilityProvider into a
    // typed PrinterCapability. This is the adapter→core translation point.
    PrinterCapability cap;
    cap.printerId       = printerId;
    cap.supportsPdfDirect = capMap.value(QStringLiteral("supportsPdfDirect"), true).toBool();
    cap.supportsColor   = capMap.value(QStringLiteral("supportsColor"), true).toBool();
    cap.supportsDuplex  = capMap.value(QStringLiteral("supportsDuplex"), false).toBool();
    cap.supportsStaple  = capMap.value(QStringLiteral("supportsStaple"), false).toBool();
    cap.supportsPunch   = capMap.value(QStringLiteral("supportsPunch"), false).toBool();

    const QVariantList paperSizesRaw = capMap.value(QStringLiteral("paperSizes")).toList();
    for (const QVariant &v : paperSizesRaw)
        cap.paperSizes.append(v.toString());

    const QVariantList qualityRaw = capMap.value(QStringLiteral("qualityModes")).toList();
    for (const QVariant &v : qualityRaw)
        cap.qualityModes.append(v.toString());

    const QVariantList resRaw = capMap.value(QStringLiteral("resolutionsDpi")).toList();
    for (const QVariant &v : resRaw)
        cap.resolutionsDpi.append(v.toInt());

    const QVariantList sourcesRaw = capMap.value(QStringLiteral("mediaSources")).toList();
    for (const QVariant &sv : sourcesRaw) {
        const QVariantMap sm = sv.toMap();
        MediaSource src;
        src.id    = sm.value(QStringLiteral("id")).toString();
        src.label = sm.value(QStringLiteral("label")).toString();
        cap.paperSources.append(src);
    }

    cap.vendorExtensions = capMap.value(QStringLiteral("vendorExtensions")).toMap();
    return cap;
}

Slm::Print::PrinterStatus CupsAdapter::printerStatus(const QString &printerId)
{
    const QVariantMap printer = d->printerManager->printerById(printerId);
    if (printer.isEmpty())
        return PrinterStatus::Unknown;

    if (!printer.value(QStringLiteral("isAvailable"), true).toBool())
        return PrinterStatus::Unavailable;

    const QString statusText =
        printer.value(QStringLiteral("statusText")).toString().toLower();

    if (statusText.contains(QLatin1String("idle")))
        return PrinterStatus::Ready;
    if (statusText.contains(QLatin1String("processing")))
        return PrinterStatus::Printing;
    if (statusText.contains(QLatin1String("disabled"))
        || statusText.contains(QLatin1String("paused")))
        return PrinterStatus::Paused;
    if (statusText.contains(QLatin1String("error")))
        return PrinterStatus::Error;

    return PrinterStatus::Ready;
}

// ── IBackendAdapter: job submission ───────────────────────────────────────

QString CupsAdapter::submitJob(const PrintSettings &settings,
                               const QString &documentUri,
                               QString &errorOut)
{
    // Build lp command arguments from the neutral ticket.
    // Neutral values are mapped to CUPS/IPP values here — nowhere else.
    const QVariantMap ippAttrs = PrintTicketSerializer::toIppAttributes(settings);

    QStringList args;
    args << QStringLiteral("-d") << settings.printerId;
    args << QStringLiteral("-n") << QString::number(qMax(1, settings.copies));

    if (!settings.pageRange.isEmpty())
        args << QStringLiteral("-P") << settings.pageRange;

    // Map neutral duplex → lp -o sides=
    const QString sides = ippAttrs.value(QStringLiteral("sides")).toString();
    if (!sides.isEmpty())
        args << QStringLiteral("-o") << QStringLiteral("sides=%1").arg(sides);

    // Map neutral colorMode → lp -o print-color-mode=
    const QString colorMode = ippAttrs.value(QStringLiteral("print-color-mode")).toString();
    if (!colorMode.isEmpty())
        args << QStringLiteral("-o")
             << QStringLiteral("print-color-mode=%1").arg(colorMode);

    // Map quality → lp -o print-quality=
    const QString quality = ippAttrs.value(QStringLiteral("print-quality")).toString();
    if (!quality.isEmpty())
        args << QStringLiteral("-o") << QStringLiteral("print-quality=%1").arg(quality);

    // Paper size
    if (!settings.paperSize.isEmpty())
        args << QStringLiteral("-o") << QStringLiteral("media=%1").arg(settings.paperSize);

    // Media source (only when not "auto")
    if (!settings.paperSource.isEmpty()
        && settings.paperSource != QLatin1String("auto"))
    {
        args << QStringLiteral("-o")
             << QStringLiteral("media-source=%1").arg(settings.paperSource);
    }

    args << documentUri;

    qCDebug(lcCupsAdapter) << "submitJob: lp" << args;

    // Run lp and parse job ID from output ("request id is <queue>-<id> (1 file)")
    QProcess proc;
    proc.setProgram(QStringLiteral("lp"));
    proc.setArguments(args);
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start();

    if (!proc.waitForFinished(10000) || proc.exitStatus() != QProcess::NormalExit) {
        errorOut = CupsErrorTranslator::connectionFailed();
        return {};
    }
    if (proc.exitCode() != 0) {
        errorOut = QStringLiteral("Printing failed. Check the printer and try again.");
        qCWarning(lcCupsAdapter) << "lp failed:" << proc.readAllStandardOutput();
        return {};
    }

    // Parse job ID: "request id is <dest>-<id> (N file(s))"
    const QString output = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    static const QRegularExpression jobIdRe(QStringLiteral("-(\\d+)\\s+\\("));
    const QRegularExpressionMatch m = jobIdRe.match(output);
    const int cupsJobId = m.hasMatch() ? m.captured(1).toInt() : 0;

    const QString handle = d->makeHandle(cupsJobId > 0 ? cupsJobId : d->nextHandle);
    qCDebug(lcCupsAdapter) << "submitJob: handle=" << handle << "cupsJobId=" << cupsJobId;
    return handle;
}

// ── IBackendAdapter: job control ──────────────────────────────────────────

bool CupsAdapter::cancelJob(const QString &jobHandle)
{
    const int jobId = d->resolveHandle(jobHandle);
    if (jobId <= 0) {
        qCWarning(lcCupsAdapter) << "cancelJob: unknown handle" << jobHandle;
        return false;
    }
    const QString out = Impl::runCommand(
        QStringLiteral("cancel"), { QString::number(jobId) });
    return !out.isNull(); // runCommand returns {} on failure
}

bool CupsAdapter::pauseJob(const QString &jobHandle)
{
    const int jobId = d->resolveHandle(jobHandle);
    if (jobId <= 0)
        return false;
    const QString out = Impl::runCommand(
        QStringLiteral("lp"),
        { QStringLiteral("-i"), QString::number(jobId),
          QStringLiteral("-H"), QStringLiteral("hold") });
    return !out.isNull();
}

bool CupsAdapter::resumeJob(const QString &jobHandle)
{
    const int jobId = d->resolveHandle(jobHandle);
    if (jobId <= 0)
        return false;
    const QString out = Impl::runCommand(
        QStringLiteral("lp"),
        { QStringLiteral("-i"), QString::number(jobId),
          QStringLiteral("-H"), QStringLiteral("resume") });
    return !out.isNull();
}

// ── IBackendAdapter: job monitoring ───────────────────────────────────────

Slm::Print::PrintJobStatus CupsAdapter::jobStatus(const QString &jobHandle)
{
    const int jobId = d->resolveHandle(jobHandle);
    if (jobId <= 0)
        return PrintJobStatus::Unknown;

    // `lpstat -l -j <id>` outputs job details including state.
    const QString out = Impl::runCommand(
        QStringLiteral("lpstat"),
        { QStringLiteral("-l"), QStringLiteral("-j"), QString::number(jobId) });

    if (out.isEmpty())
        return PrintJobStatus::Unknown;

    const QString lower = out.toLower();
    if (lower.contains(QLatin1String("processing")))
        return PrintJobStatus::Printing;
    if (lower.contains(QLatin1String("held")))
        return PrintJobStatus::Paused;
    if (lower.contains(QLatin1String("pending")))
        return PrintJobStatus::Queued;
    if (lower.contains(QLatin1String("completed")))
        return PrintJobStatus::Complete;
    if (lower.contains(QLatin1String("aborted"))
        || lower.contains(QLatin1String("canceled")))
        return PrintJobStatus::Failed;

    return PrintJobStatus::Unknown;
}

QVariantList CupsAdapter::activeJobs()
{
    // `lpstat -o` lists active jobs for all printers.
    const QString out = Impl::runCommand(
        QStringLiteral("lpstat"), { QStringLiteral("-o") });

    QVariantList result;
    if (out.isEmpty())
        return result;

    // Each line: "<printer>-<id> <user> <size> <date> <time>"
    static const QRegularExpression lineRe(
        QStringLiteral("^(\\S+)-(\\d+)\\s+(\\S+)\\s+(\\d+)\\s+(.+)$"));

    for (const QString &line : out.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const QRegularExpressionMatch m = lineRe.match(line.trimmed());
        if (!m.hasMatch())
            continue;

        const QString printerName = m.captured(1);
        const int     cupsJobId   = m.captured(2).toInt();
        const QString title       = m.captured(3); // often the filename

        QVariantMap job;
        job[QStringLiteral("jobHandle")]           = d->makeHandle(cupsJobId);
        job[QStringLiteral("documentTitle")]       = title;
        job[QStringLiteral("printerDisplayName")]  = printerName;
        job[QStringLiteral("status")]              = toString(PrintJobStatus::Queued);
        job[QStringLiteral("pagesSent")]           = 0;
        job[QStringLiteral("totalPages")]          = 0;
        result.append(job);
    }
    return result;
}

// ── CUPS-specific helpers ─────────────────────────────────────────────────

void CupsAdapter::reloadPrinters()
{
    d->printerManager->reload();
}

bool CupsAdapter::isPrintingAvailable() const
{
    return d->printerManager->printingAvailable();
}

} // namespace Slm::Print::Adapter
