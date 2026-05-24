#include "shellliveness.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusServiceWatcher>
#include <QDebug>

namespace Slm::Lockd {

ShellLiveness::ShellLiveness(QString shellBusName, QObject *parent)
    : QObject(parent)
    , m_shellBusName(std::move(shellBusName))
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    m_watcher = new QDBusServiceWatcher(m_shellBusName,
                                        bus,
                                        QDBusServiceWatcher::WatchForOwnerChange,
                                        this);
    connect(m_watcher, &QDBusServiceWatcher::serviceOwnerChanged,
            this, &ShellLiveness::onServiceOwnerChanged);
    primeFromCurrentOwner();
}

void ShellLiveness::primeFromCurrentOwner()
{
    QDBusConnectionInterface *iface = QDBusConnection::sessionBus().interface();
    const bool present = iface
        ? iface->isServiceRegistered(m_shellBusName).value()
        : false;
    setShellAlive(present);
}

void ShellLiveness::onServiceOwnerChanged(const QString &name,
                                          const QString &oldOwner,
                                          const QString &newOwner)
{
    if (name != m_shellBusName) {
        return;
    }
    Q_UNUSED(oldOwner)
    const bool nowAlive = !newOwner.trimmed().isEmpty();
    if (nowAlive == m_shellAlive) {
        return;
    }
    setShellAlive(nowAlive);
    if (!nowAlive) {
        const QString reason = QStringLiteral("shell-process-died");
        qInfo().noquote() << "[LOCKD] [SUPERVISOR] shell name lost name="
                          << m_shellBusName
                          << "reason=" << reason;
        emit shellLost(reason);
    } else {
        qInfo().noquote() << "[LOCKD] [SUPERVISOR] shell name returned name="
                          << m_shellBusName;
        emit shellReturned();
    }
}

void ShellLiveness::setShellAlive(bool alive)
{
    if (m_shellAlive == alive) {
        return;
    }
    m_shellAlive = alive;
    emit shellAliveChanged(m_shellAlive);
}

} // namespace Slm::Lockd
