#include "SlmCommand.h"

#include <QProcess>

namespace Slm::Installer {

CommandResult SlmCommand::run(const QString &program, const QStringList &args,
                              int timeoutMs)
{
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(program, args);

    CommandResult out;
    if (!proc.waitForStarted(3000)) {
        return out;
    }
    out.started = true;

    if (!proc.waitForFinished(timeoutMs)) {
        proc.kill();
        proc.waitForFinished(2000);
        out.exitCode = -2;
        out.output = QString::fromUtf8(proc.readAll());
        return out;
    }
    out.exitCode = proc.exitCode();
    out.output = QString::fromUtf8(proc.readAll());
    return out;
}

} // namespace Slm::Installer
