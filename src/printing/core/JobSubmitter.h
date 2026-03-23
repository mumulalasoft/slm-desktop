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

    static QString parseJobId(const QString &lpOutput);

signals:
    void submissionFinished(const QVariantMap &result);

private:
    QString m_lastJobId;
    QString m_lastError;
};

} // namespace Slm::Print
