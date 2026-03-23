#include "JobSubmitter.h"

#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QUrl>

namespace Slm::Print {

JobSubmitter::JobSubmitter(QObject *parent)
    : QObject(parent)
{
}

QVariantMap JobSubmitter::submit(const QVariantMap &jobPayload)
{
    QVariantMap result;
    const bool ok = jobPayload.value(QStringLiteral("success")).toBool();
    if (!ok) {
        m_lastError = jobPayload.value(QStringLiteral("error")).toString();
        m_lastJobId.clear();
        result.insert(QStringLiteral("success"), false);
        result.insert(QStringLiteral("error"), m_lastError);
        emit submissionFinished(result);
        return result;
    }

    const QString printerId = jobPayload.value(QStringLiteral("printerId")).toString();
    const QString documentUri = jobPayload.value(QStringLiteral("documentUri")).toString();
    const QVariantMap ippAttributes = jobPayload.value(QStringLiteral("ippAttributes")).toMap();

    const QUrl url(documentUri);
    const QString localPath = url.isLocalFile() ? url.toLocalFile() : documentUri;
    if (localPath.isEmpty() || !QFileInfo::exists(localPath)) {
        m_lastError = QStringLiteral("document file not found");
        m_lastJobId.clear();
        result.insert(QStringLiteral("success"), false);
        result.insert(QStringLiteral("error"), m_lastError);
        emit submissionFinished(result);
        return result;
    }

    QStringList args;
    args << QStringLiteral("-d") << printerId;
    if (ippAttributes.contains(QStringLiteral("copies"))) {
        args << QStringLiteral("-n") << QString::number(qMax(1, ippAttributes.value(QStringLiteral("copies")).toInt()));
    }

    const auto pushOption = [&](const QString &key, const QString &value) {
        if (value.trimmed().isEmpty()) {
            return;
        }
        args << QStringLiteral("-o") << QStringLiteral("%1=%2").arg(key, value);
    };
    pushOption(QStringLiteral("media"), ippAttributes.value(QStringLiteral("media")).toString());
    pushOption(QStringLiteral("sides"), ippAttributes.value(QStringLiteral("sides")).toString());
    pushOption(QStringLiteral("print-color-mode"), ippAttributes.value(QStringLiteral("print-color-mode")).toString());
    pushOption(QStringLiteral("print-quality"), ippAttributes.value(QStringLiteral("print-quality")).toString());
    pushOption(QStringLiteral("page-ranges"), ippAttributes.value(QStringLiteral("page-ranges")).toString());

    args << localPath;

    QProcess process;
    process.setProgram(QStringLiteral("lp"));
    process.setArguments(args);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();
    if (!process.waitForFinished(5000) || process.exitStatus() != QProcess::NormalExit) {
        m_lastError = QStringLiteral("failed to execute lp");
        m_lastJobId.clear();
        result.insert(QStringLiteral("success"), false);
        result.insert(QStringLiteral("error"), m_lastError);
        result.insert(QStringLiteral("exitCode"), process.exitCode());
        result.insert(QStringLiteral("stdout"), QString::fromUtf8(process.readAllStandardOutput()));
        emit submissionFinished(result);
        return result;
    }

    const QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    if (process.exitCode() != 0) {
        m_lastError = output.isEmpty() ? QStringLiteral("lp returned non-zero exit code") : output;
        m_lastJobId.clear();
        result.insert(QStringLiteral("success"), false);
        result.insert(QStringLiteral("error"), m_lastError);
        result.insert(QStringLiteral("exitCode"), process.exitCode());
        result.insert(QStringLiteral("stdout"), output);
        emit submissionFinished(result);
        return result;
    }

    m_lastJobId = parseJobId(output);
    m_lastError.clear();
    result.insert(QStringLiteral("success"), true);
    result.insert(QStringLiteral("error"), QString());
    result.insert(QStringLiteral("jobId"), m_lastJobId);
    result.insert(QStringLiteral("exitCode"), process.exitCode());
    result.insert(QStringLiteral("stdout"), output);
    emit submissionFinished(result);
    return result;
}

QString JobSubmitter::parseJobId(const QString &lpOutput)
{
    // Example: "request id is Office_Printer-42 (1 file(s))"
    static const QRegularExpression re(QStringLiteral("request\\s+id\\s+is\\s+([^\\s]+)"),
                                       QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(lpOutput);
    if (!m.hasMatch()) {
        return QString();
    }
    return m.captured(1).trimmed();
}

} // namespace Slm::Print
