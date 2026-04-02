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
        m_lastUsedConversionPipe = false;
        result.insert(QStringLiteral("success"), false);
        result.insert(QStringLiteral("error"), m_lastError);
        emit submissionFinished(result);
        return result;
    }

    const QString printerId   = jobPayload.value(QStringLiteral("printerId")).toString();
    const QString documentUri = jobPayload.value(QStringLiteral("documentUri")).toString();
    const QVariantMap jobAttributes = jobPayload.value(QStringLiteral("jobAttributes")).toMap();
    const bool supportsPdfDirect = jobPayload.value(QStringLiteral("supportsPdfDirect"), true).toBool();

    const QUrl url(documentUri);
    const QString localPath = url.isLocalFile() ? url.toLocalFile() : documentUri;
    if (localPath.isEmpty() || !QFileInfo::exists(localPath)) {
        m_lastError = QStringLiteral("document file not found");
        m_lastJobId.clear();
        m_lastUsedConversionPipe = false;
        result.insert(QStringLiteral("success"), false);
        result.insert(QStringLiteral("error"), m_lastError);
        emit submissionFinished(result);
        return result;
    }

    if (supportsPdfDirect) {
        m_lastUsedConversionPipe = false;
        return submitDirect(printerId, localPath, jobAttributes);
    }

    // Printer does not support PDF natively — convert via pdf2ps/gs.
    const QString converter = detectPsConverter();
    if (converter.isEmpty()) {
        m_lastError = QStringLiteral(
            "Printer does not accept PDF and no converter (pdf2ps/gs) was found. "
            "Install ghostscript to enable printing to this printer.");
        m_lastJobId.clear();
        m_lastUsedConversionPipe = false;
        result.insert(QStringLiteral("success"), false);
        result.insert(QStringLiteral("error"), m_lastError);
        emit submissionFinished(result);
        return result;
    }

    m_lastUsedConversionPipe = true;
    return submitViaPipe(printerId, localPath, jobAttributes);
}

void JobSubmitter::cancel(const QString &jobHandle)
{
    if (jobHandle.isEmpty()) return;
    // jobHandle is the opaque job-id string returned by lp (e.g. "printer-42").
    QProcess cancel;
    cancel.setProgram(QStringLiteral("cancel"));
    cancel.setArguments({ jobHandle });
    cancel.start();
    cancel.waitForFinished(5000);
}

QVariantMap JobSubmitter::submitDirect(const QString &printerId,
                                        const QString &localPath,
                                        const QVariantMap &jobAttributes)
{
    QVariantMap result;
    QStringList args = buildLpArgs(printerId, jobAttributes);
    args << localPath;

    QProcess process;
    process.setProgram(QStringLiteral("lp"));
    process.setArguments(args);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();
    if (!process.waitForFinished(10000) || process.exitStatus() != QProcess::NormalExit) {
        m_lastError = QStringLiteral("lp timed out or failed to start");
        m_lastJobId.clear();
        result.insert(QStringLiteral("success"), false);
        result.insert(QStringLiteral("error"), m_lastError);
        result.insert(QStringLiteral("exitCode"), process.exitCode());
        emit submissionFinished(result);
        return result;
    }

    const QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    if (process.exitCode() != 0) {
        m_lastError = output.isEmpty()
                      ? QStringLiteral("lp returned non-zero exit code")
                      : output;
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
    result.insert(QStringLiteral("exitCode"), 0);
    result.insert(QStringLiteral("stdout"), output);
    emit submissionFinished(result);
    return result;
}

QVariantMap JobSubmitter::submitViaPipe(const QString &printerId,
                                         const QString &localPath,
                                         const QVariantMap &jobAttributes)
{
    // Strategy: pdf2ps <file> - | lp -d <printer> [options] -
    // Using two QProcess instances connected via pipe.
    QVariantMap result;

    const QString converter = detectPsConverter();
    QStringList converterArgs;
    if (converter.endsWith(QLatin1String("pdf2ps"))) {
        converterArgs << localPath << QStringLiteral("-");
    } else {
        // gs path
        converterArgs << QStringLiteral("-dBATCH") << QStringLiteral("-dNOPAUSE")
                      << QStringLiteral("-q") << QStringLiteral("-sDEVICE=ps2write")
                      << QStringLiteral("-sOutputFile=-") << localPath;
    }

    QStringList lpArgs = buildLpArgs(printerId, jobAttributes);
    lpArgs << QStringLiteral("-");  // read from stdin

    QProcess converterProc;
    QProcess lpProc;
    converterProc.setProgram(converter);
    converterProc.setArguments(converterArgs);
    lpProc.setProgram(QStringLiteral("lp"));
    lpProc.setArguments(lpArgs);
    lpProc.setProcessChannelMode(QProcess::MergedChannels);

    converterProc.start();
    if (!converterProc.waitForStarted(3000)) {
        m_lastError = QStringLiteral("Failed to start PDF converter: ") + converter;
        m_lastJobId.clear();
        result.insert(QStringLiteral("success"), false);
        result.insert(QStringLiteral("error"), m_lastError);
        emit submissionFinished(result);
        return result;
    }

    lpProc.start();
    if (!lpProc.waitForStarted(3000)) {
        converterProc.kill();
        m_lastError = QStringLiteral("Failed to start lp for pipe submission");
        m_lastJobId.clear();
        result.insert(QStringLiteral("success"), false);
        result.insert(QStringLiteral("error"), m_lastError);
        emit submissionFinished(result);
        return result;
    }

    // Pump data from converter → lp in chunks.
    while (converterProc.state() != QProcess::NotRunning || converterProc.bytesAvailable() > 0) {
        converterProc.waitForReadyRead(500);
        const QByteArray chunk = converterProc.readAll();
        if (!chunk.isEmpty()) {
            lpProc.write(chunk);
        }
    }
    lpProc.closeWriteChannel();

    converterProc.waitForFinished(30000);
    lpProc.waitForFinished(30000);

    if (converterProc.exitCode() != 0) {
        m_lastError = QStringLiteral("PDF to PS conversion failed (exit %1)")
                      .arg(converterProc.exitCode());
        m_lastJobId.clear();
        result.insert(QStringLiteral("success"), false);
        result.insert(QStringLiteral("error"), m_lastError);
        emit submissionFinished(result);
        return result;
    }

    const QString lpOutput = QString::fromUtf8(lpProc.readAllStandardOutput()).trimmed();
    if (lpProc.exitCode() != 0) {
        m_lastError = lpOutput.isEmpty()
                      ? QStringLiteral("lp (pipe) returned non-zero exit code")
                      : lpOutput;
        m_lastJobId.clear();
        result.insert(QStringLiteral("success"), false);
        result.insert(QStringLiteral("error"), m_lastError);
        result.insert(QStringLiteral("stdout"), lpOutput);
        emit submissionFinished(result);
        return result;
    }

    m_lastJobId = parseJobId(lpOutput);
    m_lastError.clear();
    result.insert(QStringLiteral("success"), true);
    result.insert(QStringLiteral("error"), QString());
    result.insert(QStringLiteral("jobId"), m_lastJobId);
    result.insert(QStringLiteral("usedPipe"), true);
    result.insert(QStringLiteral("stdout"), lpOutput);
    emit submissionFinished(result);
    return result;
}

QStringList JobSubmitter::buildLpArgs(const QString &printerId,
                                       const QVariantMap &jobAttributes) const
{
    QStringList args;
    args << QStringLiteral("-d") << printerId;

    if (jobAttributes.contains(QStringLiteral("copies"))) {
        args << QStringLiteral("-n")
             << QString::number(qMax(1, jobAttributes.value(QStringLiteral("copies")).toInt()));
    }

    const auto pushOption = [&](const QString &key, const QString &value) {
        if (value.trimmed().isEmpty()) return;
        args << QStringLiteral("-o") << QStringLiteral("%1=%2").arg(key, value);
    };

    pushOption(QStringLiteral("media"),
               jobAttributes.value(QStringLiteral("media")).toString());
    pushOption(QStringLiteral("sides"),
               jobAttributes.value(QStringLiteral("sides")).toString());
    pushOption(QStringLiteral("print-color-mode"),
               jobAttributes.value(QStringLiteral("print-color-mode")).toString());
    pushOption(QStringLiteral("print-quality"),
               jobAttributes.value(QStringLiteral("print-quality")).toString());
    pushOption(QStringLiteral("page-ranges"),
               jobAttributes.value(QStringLiteral("page-ranges")).toString());
    pushOption(QStringLiteral("media-source"),
               jobAttributes.value(QStringLiteral("media-source")).toString());
    pushOption(QStringLiteral("printer-resolution"),
               jobAttributes.value(QStringLiteral("printer-resolution")).toString());

    return args;
}

QString JobSubmitter::detectPsConverter()
{
    // Prefer pdf2ps (poppler-utils), fall back to gs (ghostscript).
    for (const QString &prog : { QStringLiteral("pdf2ps"), QStringLiteral("gs") }) {
        QProcess which;
        which.setProgram(QStringLiteral("which"));
        which.setArguments({ prog });
        which.start();
        which.waitForFinished(2000);
        if (which.exitCode() == 0) {
            return prog;
        }
    }
    return QString();
}

QString JobSubmitter::parseJobId(const QString &lpOutput)
{
    static const QRegularExpression re(QStringLiteral("request\\s+id\\s+is\\s+([^\\s]+)"),
                                       QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(lpOutput);
    if (!m.hasMatch()) {
        return QString();
    }
    return m.captured(1).trimmed();
}

} // namespace Slm::Print
