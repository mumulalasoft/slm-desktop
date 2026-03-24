#include "PrintDialogController.h"

#include "../core/JobSubmitter.h"
#include "../core/PrintPreviewModel.h"
#include "../core/PrintSession.h"
#include "../core/PrinterManager.h"
#include "../core/CupsStatusWatcher.h"
#include "../persistence/PrinterSettingsStore.h"
#include "../plugin/PluginFeatureResolver.h"
#include "../preview/PopplerPdfRenderer.h"

#include <QVariantMap>

namespace Slm::Print {

struct PrintDialogController::Impl {
    PrinterManager        *printerManager        = nullptr;
    PrintSession          *printSession          = nullptr;
    PrintPreviewModel     *previewModel          = nullptr;
    JobSubmitter          *jobSubmitter          = nullptr;
    PluginFeatureResolver *pluginFeatureResolver = nullptr;
    CupsStatusWatcher     *statusWatcher         = nullptr;
    PrinterSettingsStore   settingsStore;
    PopplerPdfRenderer     renderer;
};

PrintDialogController::PrintDialogController(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Impl>())
{
    d->printerManager        = new PrinterManager(this);
    d->printSession          = new PrintSession(this);
    d->previewModel          = new PrintPreviewModel(this);
    d->jobSubmitter          = new JobSubmitter(this);
    d->pluginFeatureResolver = new PluginFeatureResolver(this);
    d->statusWatcher         = new CupsStatusWatcher(this);

    d->previewModel->setRenderer(&d->renderer);

    connect(d->jobSubmitter, &JobSubmitter::submissionFinished,
            this, &PrintDialogController::onSubmissionFinished);

    connect(d->statusWatcher, &CupsStatusWatcher::jobStatusChanged,
            this, &PrintDialogController::jobStatusUpdated);
}

PrintDialogController::~PrintDialogController() = default;

PrinterManager        *PrintDialogController::printerManager()       const { return d->printerManager; }
PrintSession          *PrintDialogController::printSession()         const { return d->printSession; }
PrintPreviewModel     *PrintDialogController::previewModel()         const { return d->previewModel; }
JobSubmitter          *PrintDialogController::jobSubmitter()         const { return d->jobSubmitter; }
PluginFeatureResolver *PrintDialogController::pluginFeatureResolver() const { return d->pluginFeatureResolver; }

void PrintDialogController::cancelJob(const QString &jobHandle)
{
    if (jobHandle.isEmpty()) return;
    d->statusWatcher->unwatchJob(jobHandle);
    d->jobSubmitter->cancel(jobHandle);
}

void PrintDialogController::openForDocument(const QString &documentUri,
                                             const QString &documentTitle)
{
    setLastError(QString());
    setLastJobId(QString());
    setIsSubmitting(false);
    setActiveDocumentTitle(documentTitle);

    d->printerManager->reload();
    d->printSession->begin(documentUri, documentTitle);

    // Select default printer if none is set yet.
    const QString currentPrinter = d->printSession->settingsModel()->printerId();
    if (currentPrinter.isEmpty()) {
        const QString defaultId = d->printerManager->defaultPrinterId();
        if (!defaultId.isEmpty()) {
            selectPrinter(defaultId);
        }
    } else {
        selectPrinter(currentPrinter);
    }

    // Set up the preview model.
    d->previewModel->reset();
    d->previewModel->setDocumentUri(documentUri);
    d->previewModel->applySettingsDigest(d->printSession->settingsModel()->serialize());
}

void PrintDialogController::submit()
{
    if (m_isSubmitting) {
        return;
    }
    setLastError(QString());

    const QVariantMap payload = d->printSession->buildJobPayload();
    if (!payload.value(QStringLiteral("success")).toBool()) {
        const QString err = payload.value(QStringLiteral("error")).toString();
        setLastError(err.isEmpty() ? QStringLiteral("Unable to build print job.") : err);
        emit submitFailed(m_lastError);
        return;
    }

    setIsSubmitting(true);
    d->jobSubmitter->submit(payload);
}

void PrintDialogController::closeDialog()
{
    // Save current settings before resetting so they persist for next open.
    const QString pid = d->printSession->settingsModel()->printerId();
    if (!pid.isEmpty()) {
        d->settingsStore.save(pid, d->printSession->settingsModel()->serialize());
    }

    d->printSession->reset();
    d->previewModel->reset();
    d->statusWatcher->clearAll();
    setIsSubmitting(false);
    setLastError(QString());
    setLastJobId(QString());
}

void PrintDialogController::selectPrinter(const QString &printerId)
{
    if (printerId.isEmpty()) {
        return;
    }

    // Load persisted settings for this printer before applying capability
    // constraints, so the stored values survive the constraint pass.
    const QVariantMap stored = d->settingsStore.load(printerId);
    if (!stored.isEmpty()) {
        d->printSession->settingsModel()->deserialize(stored);
    }
    d->printSession->settingsModel()->setPrinterId(printerId);

    const QVariantMap cap = d->printerManager->capabilities(printerId);
    d->printSession->setPrinterCapability(cap);

    // Reload plugin descriptor from vendorExtensions whenever the printer changes.
    const QVariantMap vendorExt = cap.value(QStringLiteral("vendorExtensions")).toMap();
    d->pluginFeatureResolver->loadFromVendorExtensions(vendorExt);

    setActivePrinterId(printerId);

    // Sync preview digest so a render will be triggered.
    d->previewModel->applySettingsDigest(d->printSession->settingsModel()->serialize());
}

void PrintDialogController::onSubmissionFinished(const QVariantMap &result)
{
    setIsSubmitting(false);
    if (result.value(QStringLiteral("success")).toBool()) {
        // Persist settings on successful print.
        const QString pid = d->printSession->settingsModel()->printerId();
        if (!pid.isEmpty()) {
            d->settingsStore.save(pid, d->printSession->settingsModel()->serialize());
        }
        const QString jobId = result.value(QStringLiteral("jobId")).toString();
        setLastJobId(jobId);
        if (!jobId.isEmpty()) {
            d->statusWatcher->watchJob(jobId);
        }
        emit submitSucceeded(jobId);
    } else {
        const QString err = result.value(QStringLiteral("error")).toString();
        setLastError(err.isEmpty() ? QStringLiteral("Print job failed.") : err);
        emit submitFailed(m_lastError);
    }
}

void PrintDialogController::setIsSubmitting(bool v)
{
    if (m_isSubmitting == v) return;
    m_isSubmitting = v;
    emit isSubmittingChanged();
}

void PrintDialogController::setLastError(const QString &e)
{
    if (m_lastError == e) return;
    m_lastError = e;
    emit lastErrorChanged();
}

void PrintDialogController::setLastJobId(const QString &id)
{
    if (m_lastJobId == id) return;
    m_lastJobId = id;
    emit lastJobIdChanged();
}

void PrintDialogController::setActivePrinterId(const QString &id)
{
    if (m_activePrinterId == id) return;
    m_activePrinterId = id;
    emit activePrinterIdChanged();
}

void PrintDialogController::setActiveDocumentTitle(const QString &title)
{
    if (m_activeDocumentTitle == title) return;
    m_activeDocumentTitle = title;
    emit activeDocumentTitleChanged();
}

} // namespace Slm::Print
