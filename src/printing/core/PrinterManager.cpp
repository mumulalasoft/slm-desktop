#include "PrinterManager.h"

#include <QProcess>
#include <QRegularExpression>
#include <QSet>

namespace Slm::Print {

PrinterManager::PrinterManager(QObject *parent)
    : QObject(parent)
    , m_capabilityProvider(this)
{
}

void PrinterManager::reload()
{
    // Detect CUPS availability before querying printers.
    const bool cupsNowAvailable = probeScheduler();
    if (cupsNowAvailable != m_cupsAvailable) {
        m_cupsAvailable = cupsNowAvailable;
        emit printingAvailableChanged();
    }

    if (!m_cupsAvailable) {
        // Clear printer list while CUPS is down.
        if (!m_printers.isEmpty() || !m_defaultPrinterId.isEmpty()) {
            m_printers.clear();
            m_defaultPrinterId.clear();
            emit printersChanged();
        }
        return;
    }

    const QString defaultRaw = runCommand(QStringLiteral("lpstat"), { QStringLiteral("-d") });
    const QString printersRaw = runCommand(QStringLiteral("lpstat"), { QStringLiteral("-e") });
    const QString statusesRaw = runCommand(QStringLiteral("lpstat"), { QStringLiteral("-p") });

    const QString parsedDefault = parseDefaultPrinter(defaultRaw);
    const QVariantList parsedPrinters = parsePrinterList(printersRaw, statusesRaw, parsedDefault);

    if (m_defaultPrinterId == parsedDefault && m_printers == parsedPrinters) {
        return;
    }
    m_defaultPrinterId = parsedDefault;
    m_printers = parsedPrinters;
    emit printersChanged();
}

QVariantMap PrinterManager::printerById(const QString &printerId) const
{
    for (const QVariant &value : m_printers) {
        const QVariantMap item = value.toMap();
        if (item.value(QStringLiteral("id")).toString() == printerId) {
            return item;
        }
    }
    return {};
}

QVariantMap PrinterManager::capabilities(const QString &printerId) const
{
    return m_capabilityProvider.capabilityMapForPrinter(printerId);
}

QString PrinterManager::parseDefaultPrinter(const QString &lpstatDefaultOutput)
{
    // Example: "system default destination: Office_Printer"
    static const QRegularExpression re(
        QStringLiteral("^\\s*system\\s+default\\s+destination\\s*:\\s*(\\S+)\\s*$"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption);
    const QRegularExpressionMatch match = re.match(lpstatDefaultOutput);
    if (!match.hasMatch()) {
        return QString();
    }
    return match.captured(1).trimmed();
}

QVariantList PrinterManager::parsePrinterList(const QString &lpstatEOutput,
                                              const QString &lpstatPOutput,
                                              const QString &defaultPrinter)
{
    QSet<QString> ids;
    QVariantMap statusByPrinter;

    const QStringList eLines = lpstatEOutput.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : eLines) {
        const QString id = line.trimmed().section(QLatin1Char(' '), 0, 0).trimmed();
        if (!id.isEmpty()) {
            ids.insert(id);
        }
    }

    // Example lines:
    // "printer Office_Printer is idle. enabled since ..."
    // "printer Legacy_Printer disabled since ..."
    static const QRegularExpression pRe(QStringLiteral("^\\s*printer\\s+(\\S+)\\s+(.*)$"),
                                        QRegularExpression::CaseInsensitiveOption);
    const QStringList pLines = lpstatPOutput.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : pLines) {
        const auto match = pRe.match(line);
        if (!match.hasMatch()) {
            continue;
        }
        const QString id = match.captured(1).trimmed();
        const QString rest = match.captured(2).trimmed();
        if (id.isEmpty()) {
            continue;
        }
        ids.insert(id);

        QVariantMap statusMap;
        statusMap.insert(QStringLiteral("statusText"), rest);
        statusMap.insert(QStringLiteral("isAvailable"), !rest.contains(QStringLiteral("disabled"), Qt::CaseInsensitive));
        statusByPrinter.insert(id, statusMap);
    }

    QStringList ordered = ids.values();
    ordered.sort(Qt::CaseInsensitive);

    QVariantList printers;
    for (const QString &id : ordered) {
        QVariantMap item;
        item.insert(QStringLiteral("id"), id);
        item.insert(QStringLiteral("name"), id);
        item.insert(QStringLiteral("isDefault"), id == defaultPrinter);

        const QVariantMap statusMap = statusByPrinter.value(id).toMap();
        item.insert(QStringLiteral("statusText"),
                    statusMap.value(QStringLiteral("statusText"), QStringLiteral("unknown")).toString());
        item.insert(QStringLiteral("isAvailable"), statusMap.value(QStringLiteral("isAvailable"), true).toBool());
        printers.append(item);
    }
    return printers;
}

bool PrinterManager::probeScheduler()
{
    // `lpstat -r` exits 0 with "scheduler is running" when CUPS is up,
    // exits 1 (or fails to start) when the scheduler is stopped/absent.
    QProcess process;
    process.setProgram(QStringLiteral("lpstat"));
    process.setArguments({ QStringLiteral("-r") });
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();
    if (!process.waitForFinished(2000) || process.exitStatus() != QProcess::NormalExit) {
        return false;
    }
    return process.exitCode() == 0;
}

QString PrinterManager::runCommand(const QString &program, const QStringList &arguments)
{
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();
    if (!process.waitForFinished(2000) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return QString();
    }
    return QString::fromUtf8(process.readAllStandardOutput());
}

} // namespace Slm::Print
