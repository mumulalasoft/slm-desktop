#include "powerbridge.h"

#include <QProcess>
#include <QProcessEnvironment>

PowerBridge::PowerBridge(QObject *parent)
    : QObject(parent)
{
}

bool PowerBridge::canSuspend()  const { return systemctlCan(QStringLiteral("suspend")); }
bool PowerBridge::canReboot()   const { return systemctlCan(QStringLiteral("reboot")); }
bool PowerBridge::canPowerOff() const { return systemctlCan(QStringLiteral("poweroff")); }

void PowerBridge::sleep()
{
    QProcess::startDetached(QStringLiteral("systemctl"),
                            {QStringLiteral("suspend")});
}

void PowerBridge::reboot()
{
    QProcess::startDetached(QStringLiteral("systemctl"),
                            {QStringLiteral("reboot")});
}

void PowerBridge::shutdown()
{
    QProcess::startDetached(QStringLiteral("systemctl"),
                            {QStringLiteral("poweroff")});
}

void PowerBridge::logout()
{
    // Terminate the current login session via loginctl.
    // $XDG_SESSION_ID is set by the session manager for the running desktop.
    const QString sessionId = QProcessEnvironment::systemEnvironment()
                                  .value(QStringLiteral("XDG_SESSION_ID"));
    if (!sessionId.isEmpty()) {
        QProcess::startDetached(QStringLiteral("loginctl"),
                                {QStringLiteral("terminate-session"), sessionId});
        return;
    }
    // Fallback: terminate by user name.
    const QString user = QProcessEnvironment::systemEnvironment()
                             .value(QStringLiteral("USER"));
    if (!user.isEmpty()) {
        QProcess::startDetached(QStringLiteral("loginctl"),
                                {QStringLiteral("terminate-user"), user});
    }
}

bool PowerBridge::systemctlCan(const QString &verb)
{
    QProcess p;
    p.start(QStringLiteral("systemctl"), {QStringLiteral("can-") + verb});
    p.waitForFinished(2000);
    return p.exitCode() == 0;
}
