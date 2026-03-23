#include "SessionStateClient.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
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
                setLocked(false);
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

void SessionStateClient::onSessionLocked()
{
    setLocked(true);
}

void SessionStateClient::onSessionUnlocked()
{
    setLocked(false);
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

    QDBusConnectionInterface *iface = bus.interface();
    if (!iface) {
        return;
    }
    connect(iface, SIGNAL(serviceOwnerChanged(QString,QString,QString)),
            this, SLOT(onNameOwnerChanged(QString,QString,QString)));
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

} // namespace Slm::Session
