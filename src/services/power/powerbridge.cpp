#include "powerbridge.h"

#include <QDebug>
#include <QProcess>
#include <QProcessEnvironment>

PowerBridge::PowerBridge(QObject *parent)
    : QObject(parent)
{
}

bool PowerBridge::canSuspend()  const { return systemctlCan(QStringLiteral("suspend")); }
bool PowerBridge::canReboot()   const { return systemctlCan(QStringLiteral("reboot")); }
bool PowerBridge::canPowerOff() const { return systemctlCan(QStringLiteral("poweroff")); }

bool PowerBridge::sleep()
{
    return startSystemctlAction(QStringLiteral("suspend"));
}

bool PowerBridge::reboot()
{
    return startSystemctlAction(QStringLiteral("reboot"));
}

bool PowerBridge::powerOff()
{
    return startSystemctlAction(QStringLiteral("poweroff"));
}

bool PowerBridge::shutdown()
{
    return powerOff();
}

bool PowerBridge::logout()
{
    // Terminate the current login session via loginctl.
    // $XDG_SESSION_ID is set by the session manager for the running desktop.
    const QString sessionId = QProcessEnvironment::systemEnvironment()
                                  .value(QStringLiteral("XDG_SESSION_ID"));
    if (!sessionId.isEmpty()) {
        const bool started = QProcess::startDetached(QStringLiteral("loginctl"),
                                                     {QStringLiteral("terminate-session"), sessionId});
        if (!started) {
            qWarning().noquote() << "[PowerBridge] failed to start loginctl terminate-session";
        }
        return started;
    }
    // Fallback: terminate by user name.
    const QString user = QProcessEnvironment::systemEnvironment()
                             .value(QStringLiteral("USER"));
    if (!user.isEmpty()) {
        const bool started = QProcess::startDetached(QStringLiteral("loginctl"),
                                                     {QStringLiteral("terminate-user"), user});
        if (!started) {
            qWarning().noquote() << "[PowerBridge] failed to start loginctl terminate-user";
        }
        return started;
    }
    qWarning().noquote() << "[PowerBridge] logout requested without XDG_SESSION_ID or USER";
    return false;
}

bool PowerBridge::systemctlCan(const QString &verb)
{
    QProcess p;
    p.start(QStringLiteral("systemctl"), {QStringLiteral("can-") + verb});
    p.waitForFinished(2000);
    return p.exitCode() == 0;
}

bool PowerBridge::startSystemctlAction(const QString &verb)
{
    const QString action = verb.trimmed();
    if (action.isEmpty()) {
        return false;
    }

    qInfo().noquote() << "[PowerBridge] requesting systemctl" << action;
    const bool started = QProcess::startDetached(QStringLiteral("systemctl"), {action});
    if (!started) {
        qWarning().noquote() << "[PowerBridge] failed to start systemctl" << action;
    }
    return started;
}
