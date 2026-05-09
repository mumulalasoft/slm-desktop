#pragma once

#include <QObject>
#include <QString>

class QDBusServiceWatcher;

namespace Slm::Lockd {

// ShellLiveness — watches the slm-desktop process via its well-known DBus
// name (org.slm.Desktop). Emits shellLost() when the name owner disappears
// and shellReturned() when it comes back.
//
// This is the failsafe trigger for the slm-lockd supervisor. When the shell
// dies, the supervisor must call SessionState.ForceLocked so the daemon-side
// authoritative lock state stays Locked even though the UI is gone. The
// existing login subsystem (sessionwatchdog) is responsible for actually
// restarting slm-desktop.
class ShellLiveness : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool shellAlive READ shellAlive NOTIFY shellAliveChanged)

public:
    explicit ShellLiveness(QString shellBusName, QObject *parent = nullptr);

    bool shellAlive() const { return m_shellAlive; }
    QString shellBusName() const { return m_shellBusName; }

signals:
    void shellLost(const QString &reason);
    void shellReturned();
    void shellAliveChanged(bool alive);

private slots:
    void onServiceOwnerChanged(const QString &name,
                               const QString &oldOwner,
                               const QString &newOwner);

private:
    void primeFromCurrentOwner();
    void setShellAlive(bool alive);

    QString m_shellBusName;
    bool m_shellAlive = false;
    QDBusServiceWatcher *m_watcher = nullptr;
};

} // namespace Slm::Lockd
