#pragma once

#include <QObject>

// PowerBridge — thin QML-accessible wrapper for system power and session actions.
//
// Calls systemctl for hardware-level actions (suspend/reboot/poweroff) and
// loginctl/DBUS_SESSION_BUS_ADDRESS for logout. All operations are fire-and-forget
// via QProcess::startDetached so they do not block the UI thread.
class PowerBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool canSuspend  READ canSuspend  CONSTANT)
    Q_PROPERTY(bool canReboot   READ canReboot   CONSTANT)
    Q_PROPERTY(bool canPowerOff READ canPowerOff CONSTANT)

public:
    explicit PowerBridge(QObject *parent = nullptr);

    bool canSuspend()  const;
    bool canReboot()   const;
    bool canPowerOff() const;

    Q_INVOKABLE bool sleep();
    Q_INVOKABLE bool reboot();
    Q_INVOKABLE bool powerOff();
    Q_INVOKABLE bool shutdown();
    Q_INVOKABLE bool scheduleShutdownAt(const QString &timeText);
    Q_INVOKABLE bool scheduleShutdownAfterMinutes(int minutes);
    Q_INVOKABLE bool logout();

private:
    static bool systemctlCan(const QString &verb);
    static bool startSystemctlAction(const QString &verb);
};
