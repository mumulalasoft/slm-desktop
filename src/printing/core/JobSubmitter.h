#pragma once

#include <QObject>
#include <QVariantMap>

namespace Slm::Print {

class JobSubmitter : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString lastJobId READ lastJobId NOTIFY submissionFinished)
    Q_PROPERTY(QString lastError READ lastError NOTIFY submissionFinished)
public:
    explicit JobSubmitter(QObject *parent = nullptr);

    QString lastJobId() const { return m_lastJobId; }
    QString lastError() const { return m_lastError; }

    // jobPayload is expected from PrintJobBuilder::build(...)
    // Returns:
    // {
    //   success: bool,
    //   error: string,
    //   jobId: string,
    //   stdout: string,
    //   exitCode: int
    // }
    Q_INVOKABLE QVariantMap submit(const QVariantMap &jobPayload);

    // Requests cancellation of the job identified by the opaque handle returned
    // by submit(). No-op if the handle is not a valid job id string.
    Q_INVOKABLE void cancel(const QString &jobHandle);

    // True if the last submission required a pdf→ps conversion pipe.
    Q_PROPERTY(bool lastUsedConversionPipe READ lastUsedConversionPipe NOTIFY submissionFinished)
    bool lastUsedConversionPipe() const { return m_lastUsedConversionPipe; }

    static QString parseJobId(const QString &lpOutput);

    // Returns the path of pdf2ps or gs that can produce PostScript, or empty string.
    static QString detectPsConverter();

signals:
    void submissionFinished(const QVariantMap &result);

private:
    QVariantMap submitDirect(const QString &printerId, const QString &localPath,
                             const QVariantMap &jobAttributes);
    QVariantMap submitViaPipe(const QString &printerId, const QString &localPath,
                              const QVariantMap &jobAttributes);
    QStringList buildLpArgs(const QString &printerId, const QVariantMap &jobAttributes) const;

    QString m_lastJobId;
    QString m_lastError;
    bool m_lastUsedConversionPipe = false;
};

} // namespace Slm::Print
