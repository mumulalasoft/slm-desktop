#include "CupsStatusWatcher.h"

#include <QProcess>
#include <QStandardPaths>

namespace Slm::Print {

static constexpr int kDefaultPollMs    = 2000;
static constexpr int kMissThreshold    = 2;   // polls without finding a handle → treat as complete

CupsStatusWatcher::CupsStatusWatcher(QObject *parent)
    : QObject(parent)
{
    m_pollTimer.setInterval(kDefaultPollMs);
    m_pollTimer.setSingleShot(false);
    connect(&m_pollTimer, &QTimer::timeout, this, &CupsStatusWatcher::poll);
}

void CupsStatusWatcher::watchJob(const QString &jobHandle)
{
    if (jobHandle.isEmpty()) return;
    m_watched.insert(jobHandle);
    m_missCount.remove(jobHandle);
    m_lastStatus.remove(jobHandle);
    if (!m_pollTimer.isActive()) {
        m_pollTimer.start();
    }
}

void CupsStatusWatcher::unwatchJob(const QString &jobHandle)
{
    m_watched.remove(jobHandle);
    m_missCount.remove(jobHandle);
    m_lastStatus.remove(jobHandle);
    if (m_watched.isEmpty()) {
        m_pollTimer.stop();
    }
}

void CupsStatusWatcher::clearAll()
{
    m_pollTimer.stop();
    m_watched.clear();
    m_missCount.clear();
    m_lastStatus.clear();
}

void CupsStatusWatcher::setPollIntervalMs(int ms)
{
    m_pollTimer.setInterval(qMax(500, ms));
}

// ── Polling ───────────────────────────────────────────────────────────────────

void CupsStatusWatcher::poll()
{
    if (m_watched.isEmpty()) {
        m_pollTimer.stop();
        return;
    }

    // Guard: if lpstat is not installed, treat all watched jobs as complete
    // immediately so the UI is not left in a permanent "printing" state.
    if (QStandardPaths::findExecutable(QStringLiteral("lpstat")).isEmpty()) {
        const QList<QString> snapshot(m_watched.begin(), m_watched.end());
        for (const QString &handle : snapshot) {
            emit jobStatusChanged(handle, QStringLiteral("complete"), QString());
            unwatchJob(handle);
        }
        return;
    }

    // Run `lpstat -W all` to get all active and recently completed jobs.
    QProcess lpstat;
    lpstat.setProgram(QStringLiteral("lpstat"));
    lpstat.setArguments({ QStringLiteral("-W"), QStringLiteral("all") });
    lpstat.setProcessChannelMode(QProcess::MergedChannels);
    lpstat.start();
    if (!lpstat.waitForFinished(5000)) {
        lpstat.kill();
        return;
    }

    // Build a map: jobHandle → status bracket text (lower-cased).
    // lpstat output line format:
    //   printer-N   owner   size   date   [job N status-text]
    QHash<QString, QString> statusMap;
    const QString output = QString::fromUtf8(lpstat.readAllStandardOutput());
    for (const QString &line : output.split(QLatin1Char('\n'))) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;

        // First whitespace-delimited token is the job handle.
        const int spaceIdx = trimmed.indexOf(QLatin1Char(' '));
        const QString handle = (spaceIdx > 0) ? trimmed.left(spaceIdx) : trimmed;
        if (handle.isEmpty()) continue;

        // Status is inside the last [...] on the line.
        const int bracketOpen  = trimmed.lastIndexOf(QLatin1Char('['));
        const int bracketClose = trimmed.lastIndexOf(QLatin1Char(']'));
        QString bracketContent;
        if (bracketOpen >= 0 && bracketClose > bracketOpen) {
            bracketContent = trimmed.mid(bracketOpen + 1, bracketClose - bracketOpen - 1)
                                    .toLower();
        }

        statusMap.insert(handle, bracketContent);
    }

    // Process each watched handle.
    const QList<QString> snapshot(m_watched.begin(), m_watched.end());
    for (const QString &handle : snapshot) {
        QString status;
        QString detail;

        if (statusMap.contains(handle)) {
            m_missCount.remove(handle);
            status = neutralStatus(statusMap.value(handle));
        } else {
            // Job not found in lpstat — it may have been flushed after completion.
            const int misses = m_missCount.value(handle, 0) + 1;
            m_missCount.insert(handle, misses);
            if (misses < kMissThreshold) {
                continue; // wait one more poll before concluding
            }
            // Flushed from queue — assume completed successfully.
            status = QStringLiteral("complete");
        }

        // Suppress duplicate status emissions.
        if (m_lastStatus.value(handle) == status) {
            if (status != QStringLiteral("complete") && status != QStringLiteral("failed")) {
                continue;
            }
        }
        m_lastStatus.insert(handle, status);

        emit jobStatusChanged(handle, status, detail);

        // Auto-unwatch on terminal states.
        if (status == QStringLiteral("complete") || status == QStringLiteral("failed")) {
            unwatchJob(handle);
        }
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────

// Maps the text inside [...] in lpstat output to neutral status vocabulary.
QString CupsStatusWatcher::neutralStatus(const QString &bracketText)
{
    if (bracketText.contains(QLatin1String("processing")))  return QStringLiteral("printing");
    if (bracketText.contains(QLatin1String("pending")))     return QStringLiteral("queued");
    if (bracketText.contains(QLatin1String("completed")))   return QStringLiteral("complete");
    if (bracketText.contains(QLatin1String("aborted")))     return QStringLiteral("failed");
    if (bracketText.contains(QLatin1String("cancelled")))   return QStringLiteral("failed");
    if (bracketText.contains(QLatin1String("stopped")))     return QStringLiteral("failed");
    // Unknown but still visible in queue — treat as active/printing.
    return QStringLiteral("printing");
}

} // namespace Slm::Print
