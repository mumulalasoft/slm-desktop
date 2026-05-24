#include "SessionStateClient.h"

#include <QDebug>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusServiceWatcher>
#include <QDBusReply>
#include <QProcessEnvironment>

namespace Slm::Session {
namespace {
constexpr const char kService[] = "org.slm.SessionState";
constexpr const char kPath[] = "/org/slm/SessionState";
constexpr const char kIface[] = "org.slm.SessionState1";
}

SessionStateClient::SessionStateClient(QObject *parent)
    : QObject(parent)
    , m_iface(new QDBusInterface(QString::fromLatin1(kService),
                                 QString::fromLatin1(kPath),
                                 QString::fromLatin1(kIface),
                                 QDBusConnection::sessionBus(),
                                 this))
    , m_userName(QProcessEnvironment::systemEnvironment().value(QStringLiteral("USER")))
{
    if (m_userName.trimmed().isEmpty()) {
        m_userName = QStringLiteral("User");
    }
    bindSignals();
    refreshServiceAvailability();
    refreshLockStateSnapshot();
}

SessionStateClient::~SessionStateClient() = default;

bool SessionStateClient::serviceAvailable() const
{
    return m_serviceAvailable;
}

bool SessionStateClient::locked() const
{
    return m_locked;
}

QString SessionStateClient::lockState() const
{
    return m_lockState;
}

QString SessionStateClient::userName() const
{
    return m_userName;
}

QString SessionStateClient::lastUnlockError() const
{
    return m_lastUnlockError;
}

int SessionStateClient::lastRetryAfterSec() const
{
    return m_lastRetryAfterSec;
}

void SessionStateClient::lock()
{
    if (m_iface && m_iface->isValid()) {
        m_iface->call(QStringLiteral("Lock"));
    }
    setLocked(true);
    setLockState(QStringLiteral("Locking"));
}

bool SessionStateClient::requestUnlock(const QString &password)
{
    if (password.isEmpty()) {
        if (m_lastUnlockError != QStringLiteral("empty-password") || m_lastRetryAfterSec != 0) {
            m_lastUnlockError = QStringLiteral("empty-password");
            m_lastRetryAfterSec = 0;
            emit lastUnlockResultChanged();
        }
        return false;
    }
    if (m_iface && m_iface->isValid()) {
        QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("Unlock"), password);
        if (reply.isValid()) {
            const QVariantMap out = reply.value();
            const bool ok = out.value(QStringLiteral("ok")).toBool();
            const QString error = out.value(QStringLiteral("error")).toString();
            const int retryAfter = out.value(QStringLiteral("retry_after_sec"), 0).toInt();
            if (m_lastUnlockError != error || m_lastRetryAfterSec != retryAfter) {
                m_lastUnlockError = error;
                m_lastRetryAfterSec = retryAfter;
                emit lastUnlockResultChanged();
            }
            if (ok) {
                return true;
            }
            return false;
        }
        if (m_lastUnlockError != QStringLiteral("dbus-call-failed") || m_lastRetryAfterSec != 0) {
            m_lastUnlockError = QStringLiteral("dbus-call-failed");
            m_lastRetryAfterSec = 0;
            emit lastUnlockResultChanged();
        }
        return false;
    }
    if (m_lastUnlockError != QStringLiteral("service-unavailable") || m_lastRetryAfterSec != 0) {
        m_lastUnlockError = QStringLiteral("service-unavailable");
        m_lastRetryAfterSec = 0;
        emit lastUnlockResultChanged();
    }
    return false;
}

void SessionStateClient::setLocked(bool locked)
{
    if (m_locked == locked) {
        return;
    }
    m_locked = locked;
    emit lockedChanged();
}

void SessionStateClient::setLockState(const QString &state)
{
    const QString normalized = state.trimmed().isEmpty() ? QStringLiteral("Active") : state.trimmed();
    if (m_lockState == normalized) {
        return;
    }
    m_lockState = normalized;
    emit lockStateChanged();
}

void SessionStateClient::onSessionLocked()
{
    setLocked(true);
    if (m_lockState != QStringLiteral("Locked")) {
        setLockState(QStringLiteral("Locked"));
    }
}

void SessionStateClient::onSessionUnlocked()
{
    setLocked(false);
    setLockState(QStringLiteral("Active"));
}

void SessionStateClient::onLockStateChanged(const QString &state)
{
    setLockState(state);
    if (state == QStringLiteral("Locked") || state == QStringLiteral("Locking")) {
        setLocked(true);
    } else if (state == QStringLiteral("Active")) {
        setLocked(false);
    }
}

void SessionStateClient::onResumed()
{
    qInfo().noquote() << "[LOCKSCREEN] [RESUME] desktopd reports system resumed";
    emit resumed();
}

void SessionStateClient::onNameOwnerChanged(const QString &name,
                                            const QString &oldOwner,
                                            const QString &newOwner)
{
    Q_UNUSED(oldOwner)
    if (name != QString::fromLatin1(kService)) {
        return;
    }
    const bool nowAvailable = !newOwner.isEmpty();
    if (m_serviceAvailable != nowAvailable) {
        m_serviceAvailable = nowAvailable;
        emit serviceAvailableChanged();
    }
    if (nowAvailable) {
        refreshLockStateSnapshot();
    }
}

void SessionStateClient::bindSignals()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(QString::fromLatin1(kService),
                QString::fromLatin1(kPath),
                QString::fromLatin1(kIface),
                QStringLiteral("SessionLocked"),
                this,
                SLOT(onSessionLocked()));

    bus.connect(QString::fromLatin1(kService),
                QString::fromLatin1(kPath),
                QString::fromLatin1(kIface),
                QStringLiteral("SessionUnlocked"),
                this,
                SLOT(onSessionUnlocked()));

    bus.connect(QString::fromLatin1(kService),
                QString::fromLatin1(kPath),
                QString::fromLatin1(kIface),
                QStringLiteral("LockStateChanged"),
                this,
                SLOT(onLockStateChanged(QString)));

    bus.connect(QString::fromLatin1(kService),
                QString::fromLatin1(kPath),
                QString::fromLatin1(kIface),
                QStringLiteral("Resumed"),
                this,
                SLOT(onResumed()));

    auto *watcher = new QDBusServiceWatcher(QString::fromLatin1(kService),
                                             bus,
                                             QDBusServiceWatcher::WatchForOwnerChange,
                                             this);
    connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged,
            this, &SessionStateClient::onNameOwnerChanged);
}

void SessionStateClient::refreshServiceAvailability()
{
    QDBusConnectionInterface *iface = QDBusConnection::sessionBus().interface();
    const bool available = iface
            ? iface->isServiceRegistered(QString::fromLatin1(kService)).value()
            : false;
    if (m_serviceAvailable != available) {
        m_serviceAvailable = available;
        emit serviceAvailableChanged();
    }
}

void SessionStateClient::refreshLockStateSnapshot()
{
    if (!m_iface || !m_iface->isValid()) {
        return;
    }

    const QVariant rawState = m_iface->property("lockState");
    const QString state = rawState.toString().trimmed();
    if (state.isEmpty()) {
        return;
    }

    onLockStateChanged(state);
}

} // namespace Slm::Session
