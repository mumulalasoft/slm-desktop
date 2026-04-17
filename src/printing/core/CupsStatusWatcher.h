#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QTimer>

namespace Slm::Print {

// Polls the system print queue and emits jobStatusChanged whenever a tracked
// job's status changes. Uses neutral desktop vocabulary in all signals —
// no CUPS/IPP terms are exposed outside this class.
//
// Usage:
//   watcher->watchJob(jobHandle);          // start tracking
//   connect(watcher, &CupsStatusWatcher::jobStatusChanged, ...)
//   watcher->unwatchJob(jobHandle);        // stop tracking (also called automatically on terminal state)
//
// jobHandle: opaque string returned by JobSubmitter (e.g. "HP_LaserJet-42").
// status values: "queued" | "printing" | "complete" | "failed"
class CupsStatusWatcher : public QObject
{
    Q_OBJECT
public:
    explicit CupsStatusWatcher(QObject *parent = nullptr);

    // Begin tracking a job. Starts the poll timer if not already running.
    void watchJob(const QString &jobHandle);

    // Stop tracking a job. Stops the poll timer when the watch set is empty.
    void unwatchJob(const QString &jobHandle);

    // Remove all watched jobs and stop polling.
    void clearAll();

    int pollIntervalMs() const { return m_pollTimer.interval(); }
    void setPollIntervalMs(int ms);

signals:
    // Emitted whenever a tracked job's status is determined.
    // Terminal statuses ("complete" / "failed") are emitted once, then the
    // handle is automatically removed from the watch set.
    void jobStatusChanged(const QString &jobHandle,
                          const QString &status,
                          const QString &statusDetail);

private:
    void poll();
    static QString neutralStatus(const QString &lpstatBracketText);

    QTimer              m_pollTimer;
    QSet<QString>       m_watched;
    QHash<QString, int> m_missCount;   // consecutive "not in lpstat" count per handle
    QHash<QString, QString> m_lastStatus; // last emitted status per handle (suppress duplicates)
};

} // namespace Slm::Print
