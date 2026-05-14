#include "cupsadapter.h"

#include <QProcess>
#include <QStandardPaths>

CupsAdapter::CupsAdapter(QObject *parent)
    : ISharingAdapter(parent)
{}

ISharingAdapter::Status CupsAdapter::probe()
{
    return cupsctlAvailable() ? Status::Available : Status::Unavailable;
}

bool CupsAdapter::activate()
{
    if (!cupsctlAvailable())
        return false;
    // Enable CUPS server-side sharing
    const bool ok = runCupsCtl({QStringLiteral("--share-printers")});
    m_active = ok;
    if (ok)
        emit statusChanged(Status::Available);
    return ok;
}

bool CupsAdapter::deactivate()
{
    runCupsCtl({QStringLiteral("--no-share-printers")});
    m_active = false;
    emit statusChanged(Status::Unavailable);
    return true;
}

bool CupsAdapter::recover()
{
    emit capabilityEvent(QStringLiteral("recovery-suggestion"), {
        {QStringLiteral("issue"), QStringLiteral("cups-not-found")},
        {QStringLiteral("package"), QStringLiteral("cups")},
        {QStringLiteral("message"), QStringLiteral("Printer sharing requires CUPS.")},
    });
    return false;
}

QVariantMap CupsAdapter::statusDetail() const
{
    return {
        {QStringLiteral("cupsctlAvailable"), cupsctlAvailable()},
        {QStringLiteral("active"), m_active},
    };
}

QVariantList CupsAdapter::listLocalPrinters() const
{
    QVariantList result;
    QProcess p;
    p.start(QStringLiteral("lpstat"), {QStringLiteral("-p")});
    if (!p.waitForFinished(3000))
        return result;

    const auto lines = QString::fromLocal8Bit(p.readAllStandardOutput()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const auto &line : lines) {
        if (line.startsWith(QLatin1String("printer "))) {
            const auto parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                result.append(QVariantMap{{QStringLiteral("id"), parts[1]},
                                          {QStringLiteral("name"), parts[1]}});
            }
        }
    }
    return result;
}

bool CupsAdapter::sharePrinter(const QString &printerId, bool shared)
{
    if (!m_active)
        return false;
    const QString flag = shared ? QStringLiteral("-o printer-is-shared=true")
                                : QStringLiteral("-o printer-is-shared=false");
    QProcess p;
    p.start(QStringLiteral("lpadmin"), {QStringLiteral("-p"), printerId, flag});
    return p.waitForFinished(5000) && p.exitCode() == 0;
}

bool CupsAdapter::cupsctlAvailable() const
{
    return !QStandardPaths::findExecutable(QStringLiteral("cupsctl")).isEmpty();
}

bool CupsAdapter::runCupsCtl(const QStringList &args) const
{
    QProcess p;
    p.start(QStringLiteral("cupsctl"), args);
    return p.waitForFinished(5000) && p.exitCode() == 0;
}
