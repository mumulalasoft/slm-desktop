#pragma once

#include <QObject>
#include <QVariantMap>
#include <memory>

namespace Slm::Print {

class PrinterManager;
class PrintSession;
class PrintPreviewModel;
class JobSubmitter;
class IPdfRenderer;
class PluginFeatureResolver;
class CupsStatusWatcher;

// Aggregates the C++ print backend objects and exposes them to QML.
//
// Register as a context property before loading PrintDialog.qml:
//
//   engine.rootContext()->setContextProperty(
//       "PrintDialogController", m_printDialogController);
//
// QML then accesses individual sub-objects via PrintDialogController.printerManager etc.
//
// Lifecycle:
//   openForDocument(uri, title) — begins a session for the given document.
//   submit()                   — builds and submits the print job.
//   closeDialog()              — resets session state.
class PrintDialogController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(Slm::Print::PrinterManager       *printerManager       READ printerManager       CONSTANT)
    Q_PROPERTY(Slm::Print::PrintSession         *printSession         READ printSession         CONSTANT)
    Q_PROPERTY(Slm::Print::PrintPreviewModel    *previewModel         READ previewModel         CONSTANT)
    Q_PROPERTY(Slm::Print::JobSubmitter         *jobSubmitter         READ jobSubmitter         CONSTANT)
    Q_PROPERTY(Slm::Print::PluginFeatureResolver *pluginFeatureResolver READ pluginFeatureResolver CONSTANT)

    Q_PROPERTY(bool    isSubmitting      READ isSubmitting      NOTIFY isSubmittingChanged)
    Q_PROPERTY(QString lastError         READ lastError         NOTIFY lastErrorChanged)
    Q_PROPERTY(QString lastJobId         READ lastJobId         NOTIFY lastJobIdChanged)
    Q_PROPERTY(QString activePrinterId   READ activePrinterId   NOTIFY activePrinterIdChanged)
    Q_PROPERTY(QString activeDocumentTitle READ activeDocumentTitle NOTIFY activeDocumentTitleChanged)

public:
    explicit PrintDialogController(QObject *parent = nullptr);
    ~PrintDialogController() override;

    PrinterManager        *printerManager()       const;
    PrintSession          *printSession()         const;
    PrintPreviewModel     *previewModel()         const;
    JobSubmitter          *jobSubmitter()         const;
    PluginFeatureResolver *pluginFeatureResolver() const;

    bool    isSubmitting()        const { return m_isSubmitting; }
    QString lastError()           const { return m_lastError; }
    QString lastJobId()           const { return m_lastJobId; }
    QString activePrinterId()     const { return m_activePrinterId; }
    QString activeDocumentTitle() const { return m_activeDocumentTitle; }

    // Opens the dialog for the given document URI and title.
    // Reloads printer list, begins a new session, and triggers an initial render.
    Q_INVOKABLE void openForDocument(const QString &documentUri,
                                     const QString &documentTitle);

    // Builds the job payload from the current session and submits it.
    // Emits submitSucceeded(jobId) or submitFailed(error) asynchronously.
    Q_INVOKABLE void submit();

    // Resets session state without destroying objects.
    Q_INVOKABLE void closeDialog();

    // Sets the active printer, loads its capability into the session,
    // and clears the preview cache so the next render uses fresh settings.
    Q_INVOKABLE void selectPrinter(const QString &printerId);

    // Requests cancellation of the job with the given opaque handle.
    // No-op if the handle is unknown or printing has already completed.
    Q_INVOKABLE void cancelJob(const QString &jobHandle);

signals:
    void isSubmittingChanged();
    void lastErrorChanged();
    void lastJobIdChanged();
    void activePrinterIdChanged();
    void activeDocumentTitleChanged();
    void submitSucceeded(const QString &jobId);
    void submitFailed(const QString &error);

    // Emitted when a tracked job's status changes (live queue monitoring).
    // status: "queued" | "printing" | "complete" | "failed"
    void jobStatusUpdated(const QString &jobHandle,
                          const QString &status,
                          const QString &statusDetail);

private:
    void onSubmissionFinished(const QVariantMap &result);
    void setIsSubmitting(bool v);
    void setLastError(const QString &e);
    void setLastJobId(const QString &id);
    void setActivePrinterId(const QString &id);
    void setActiveDocumentTitle(const QString &title);

    struct Impl;
    std::unique_ptr<Impl> d;

    bool    m_isSubmitting = false;
    QString m_lastError;
    QString m_lastJobId;
    QString m_activePrinterId;
};

} // namespace Slm::Print
