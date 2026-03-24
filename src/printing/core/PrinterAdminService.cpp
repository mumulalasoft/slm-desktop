#include "PrinterAdminService.h"

#include <QProcess>
#include <QRegularExpression>

namespace Slm::Print {

// Printer queue names must be alphanumeric plus underscore and hyphen.
// Spaces and shell-significant characters are never allowed.
static bool isValidPrinterName(const QString &name)
{
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9_\\-]{1,128}$"));
    return re.match(name).hasMatch();
}

PrinterAdminService::PrinterAdminService(QObject *parent)
    : QObject(parent)
{
}

void PrinterAdminService::setBusy(bool v)
{
    if (m_busy == v) return;
    m_busy = v;
    emit busyChanged();
}

// ── Public operations ─────────────────────────────────────────────────────────

void PrinterAdminService::removePrinter(const QString &printerId)
{
    if (printerId.trimmed().isEmpty()) return;

    auto *proc = new QProcess(this);
    proc->setProgram(QStringLiteral("lpadmin"));
    proc->setArguments({ QStringLiteral("-x"), printerId.trimmed() });
    proc->setProcessChannelMode(QProcess::MergedChannels);

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, printerId](int exitCode, QProcess::ExitStatus) {
        const bool ok = (exitCode == 0);
        const QString err = ok ? QString()
                                : QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();
        emit printerRemoved(ok, printerId, err);
    });

    proc->start();
}

void PrinterAdminService::setDefaultPrinter(const QString &printerId)
{
    if (printerId.trimmed().isEmpty()) return;

    auto *proc = new QProcess(this);
    proc->setProgram(QStringLiteral("lpoptions"));
    proc->setArguments({ QStringLiteral("-d"), printerId.trimmed() });
    proc->setProcessChannelMode(QProcess::MergedChannels);

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, printerId](int exitCode, QProcess::ExitStatus) {
        const bool ok = (exitCode == 0);
        const QString err = ok ? QString()
                                : QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();
        emit defaultPrinterSet(ok, printerId, err);
    });

    proc->start();
}

void PrinterAdminService::addPrinter(const QString &name,
                                      const QString &deviceUri,
                                      const QString &ppd)
{
    const QString trimmedName = name.trimmed();
    const QString trimmedUri  = deviceUri.trimmed();

    if (trimmedName.isEmpty() || trimmedUri.isEmpty()) {
        emit printerAdded(false, name, QStringLiteral("Printer name and device address are required."));
        return;
    }
    if (!isValidPrinterName(trimmedName)) {
        emit printerAdded(false, name,
                          QStringLiteral("Printer name may only contain letters, digits, "
                                         "hyphens, and underscores (max 128 characters)."));
        return;
    }

    // Build lpadmin arguments:
    //   lpadmin -p <name> -E -v <uri> [-P <ppd-path> | -m <driver-name> | -m everywhere]
    QStringList args;
    args << QStringLiteral("-p") << trimmedName
         << QStringLiteral("-E")
         << QStringLiteral("-v") << trimmedUri;

    const QString trimmedPpd = ppd.trimmed();
    if (trimmedPpd.isEmpty()) {
        // Use IPP Everywhere — works with most modern network printers.
        args << QStringLiteral("-m") << QStringLiteral("everywhere");
    } else if (trimmedPpd.startsWith(QLatin1Char('/'))) {
        // Absolute path → treat as PPD file.
        args << QStringLiteral("-P") << trimmedPpd;
    } else {
        // Driver name / model identifier.
        args << QStringLiteral("-m") << trimmedPpd;
    }

    auto *proc = new QProcess(this);
    proc->setProgram(QStringLiteral("lpadmin"));
    proc->setArguments(args);
    proc->setProcessChannelMode(QProcess::MergedChannels);

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, trimmedName](int exitCode, QProcess::ExitStatus) {
        const bool ok = (exitCode == 0);
        const QString err = ok ? QString()
                                : QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();
        emit printerAdded(ok, trimmedName, err);
    });

    proc->start();
}

void PrinterAdminService::discoverDevices()
{
    if (m_busy) return;
    setBusy(true);
    m_discoveredDevices.clear();
    emit discoveredDevicesChanged();

    // `lpinfo -v` lists URIs reported by each backend immediately (no network scan).
    // Filter out raw backend entries (e.g. "file /dev/null") and keep only
    // addressable network/USB device URIs.
    auto *proc = new QProcess(this);
    proc->setProgram(QStringLiteral("lpinfo"));
    proc->setArguments({ QStringLiteral("-v") });
    proc->setProcessChannelMode(QProcess::MergedChannels);

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int /*exitCode*/, QProcess::ExitStatus) {
        const QString output = QString::fromUtf8(proc->readAllStandardOutput());
        proc->deleteLater();

        QVariantList devices;
        for (const QString &raw : output.split(QLatin1Char('\n'))) {
            const QString line = raw.trimmed();
            if (line.isEmpty()) continue;

            // Format: "<type> <uri>" or "<type> <uri> <info...>"
            const int firstSpace = line.indexOf(QLatin1Char(' '));
            if (firstSpace < 0) continue;

            const QString type = line.left(firstSpace);
            const QString rest = line.mid(firstSpace + 1).trimmed();
            if (rest.isEmpty()) continue;

            // Skip raw single-word entries (e.g. "network http") — no real URI.
            if (!rest.contains(QLatin1String("://"))) continue;

            const int uriEnd = rest.indexOf(QLatin1Char(' '));
            const QString uri  = (uriEnd > 0) ? rest.left(uriEnd) : rest;
            const QString info = (uriEnd > 0) ? rest.mid(uriEnd + 1).trimmed() : QString();

            QVariantMap entry;
            entry.insert(QStringLiteral("uri"),  uri);
            entry.insert(QStringLiteral("type"), type);
            entry.insert(QStringLiteral("info"), info);
            devices.append(entry);
        }

        m_discoveredDevices = devices;
        emit discoveredDevicesChanged();
        setBusy(false);
    });

    proc->start();
}

} // namespace Slm::Print
