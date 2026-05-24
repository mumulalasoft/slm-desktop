#pragma once

#include <QString>
#include <QStringList>

namespace Slm::Installer {

// Outcome of a SlmCommand::run() invocation. Merged stdout+stderr lives
// in `output` so callers can quote the program's noise back to the user
// in a JobResult details string without a second QProcess call.
struct CommandResult
{
    bool    started = false;   // false → QProcess::waitForStarted timed out
    int     exitCode = -1;     // -2 → waitForFinished timeout (process killed)
    QString output;            // merged stdout+stderr, trimmed by caller
};

// Thin synchronous wrapper around QProcess. The four destructive
// installer modules (slm-snapshot, slm-partition, slm-btrfs-layout,
// slm-recovery) each carried a verbatim copy of this shape before it
// was extracted here.
//
// Defaults to 60s — mkfs.btrfs on a slow rotating disk can take ~30s
// and the partprobe → mkfs.* race window benefits from a generous
// ceiling. Pass a shorter timeoutMs for fast probes (smartctl, lsblk).
class SlmCommand
{
public:
    static CommandResult run(const QString &program,
                             const QStringList &args,
                             int timeoutMs = 60000);
};

} // namespace Slm::Installer
