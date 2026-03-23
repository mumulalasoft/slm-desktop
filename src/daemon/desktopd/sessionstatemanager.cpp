#include "sessionstatemanager.h"

#include "../../core/workspace/windowingbackendmanager.h"

#include <QDBusInterface>
#include <QDBusReply>
#include <QMetaObject>
#include <QProcessEnvironment>

SessionStateManager::SessionStateManager(WindowingBackendManager *windowingBackend,
                                         QObject *compositorStateModel,
                                         QObject *parent)
    : QObject(parent)
    , m_windowingBackend(windowingBackend)
    , m_compositorStateModel(compositorStateModel)
{
    if (m_windowingBackend) {
        connect(m_windowingBackend, &WindowingBackendManager::eventReceived,
                this, &SessionStateManager::updateFromEvent);
    }

    QDBusConnection::sessionBus().connect(QStringLiteral("org.freedesktop.ScreenSaver"),
                                          QStringLiteral("/org/freedesktop/ScreenSaver"),
                                          QStringLiteral("org.freedesktop.ScreenSaver"),
                                          QStringLiteral("ActiveChanged"),
                                          this,
                                          SLOT(onScreenSaverActiveChanged(bool)));
}

quint32 SessionStateManager::Inhibit(const QString &reason, uint flags)
{
    Q_UNUSED(flags);
    const QString why = reason.trimmed().isEmpty()
            ? QStringLiteral("SLM Desktop operation in progress")
            : reason.trimmed();

    QDBusInterface iface(QStringLiteral("org.freedesktop.ScreenSaver"),
                         QStringLiteral("/org/freedesktop/ScreenSaver"),
                         QStringLiteral("org.freedesktop.ScreenSaver"),
                         QDBusConnection::sessionBus());
    if (iface.isValid()) {
        const QString app = QStringLiteral("SLM Desktop");
        QDBusReply<uint> reply = iface.call(QStringLiteral("Inhibit"), app, why);
        if (reply.isValid()) {
            return reply.value();
        }
    }

    m_inhibitSeq += 1;
    return m_inhibitSeq;
}

void SessionStateManager::Logout()
{
    bool ok = false;
    const QString sessionId = QProcessEnvironment::systemEnvironment()
                                      .value(QStringLiteral("XDG_SESSION_ID"))
                                      .trimmed();
    if (!sessionId.isEmpty()) {
        ok = callLogin1(QStringLiteral("TerminateSession"), {sessionId});
    }
    if (!ok) {
        QDBusInterface iface(QStringLiteral("org.kde.Shutdown"),
                             QStringLiteral("/Shutdown"),
                             QStringLiteral("org.kde.Shutdown"),
                             QDBusConnection::sessionBus());
        if (iface.isValid()) {
            iface.call(QStringLiteral("logout"));
        }
    }
}

void SessionStateManager::Shutdown()
{
    if (callLogin1(QStringLiteral("PowerOff"), {false})) {
        return;
    }
    QDBusInterface iface(QStringLiteral("org.kde.Shutdown"),
                         QStringLiteral("/Shutdown"),
                         QStringLiteral("org.kde.Shutdown"),
                         QDBusConnection::sessionBus());
    if (iface.isValid()) {
        iface.call(QStringLiteral("logoutAndShutdown"));
    }
}

void SessionStateManager::Reboot()
{
    if (callLogin1(QStringLiteral("Reboot"), {false})) {
        return;
    }
    QDBusInterface iface(QStringLiteral("org.kde.Shutdown"),
                         QStringLiteral("/Shutdown"),
                         QStringLiteral("org.kde.Shutdown"),
                         QDBusConnection::sessionBus());
    if (iface.isValid()) {
        iface.call(QStringLiteral("logoutAndReboot"));
    }
}

void SessionStateManager::Lock()
{
    if (callScreenSaver(QStringLiteral("Lock"))) {
        emit SessionLocked();
        emitIdle(true);
        return;
    }
    if (callLogin1(QStringLiteral("LockSessions"))) {
        emit SessionLocked();
        emitIdle(true);
    }
}

void SessionStateManager::Unlock()
{
    bool ok = callScreenSaver(QStringLiteral("SetActive"), {false});
    if (!ok) {
        const QString sessionId = QProcessEnvironment::systemEnvironment()
                                          .value(QStringLiteral("XDG_SESSION_ID"))
                                          .trimmed();
        if (!sessionId.isEmpty()) {
            ok = callLogin1(QStringLiteral("UnlockSession"), {sessionId});
        }
    }
    Q_UNUSED(ok)
    emit SessionUnlocked();
    emitIdle(false);
}

void SessionStateManager::updateFromEvent(const QString &event, const QVariantMap &payload)
{
    const QString key = event.trimmed().toLower();
    const QString payloadKey = payload.value(QStringLiteral("event")).toString().trimmed().toLower();
    const QString normalized = payloadKey.isEmpty() ? key : payloadKey;

    if (normalized == QStringLiteral("lock")
            || normalized == QStringLiteral("session-locked")
            || normalized == QStringLiteral("screen-locked")) {
        emit SessionLocked();
        emitIdle(true);
        return;
    }
    if (normalized == QStringLiteral("unlock")
            || normalized == QStringLiteral("session-unlocked")
            || normalized == QStringLiteral("screen-unlocked")) {
        emit SessionUnlocked();
        emitIdle(false);
        return;
    }

    if (normalized == QStringLiteral("window-focused")
            || normalized == QStringLiteral("window-activated")
            || normalized == QStringLiteral("focus-changed")) {
        QString appId = payload.value(QStringLiteral("appId")).toString().trimmed();
        if (appId.isEmpty()) {
            appId = payload.value(QStringLiteral("appid")).toString().trimmed();
        }
        if (appId.isEmpty()) {
            appId = payload.value(QStringLiteral("app_id")).toString().trimmed();
        }
        emitActiveApp(appId);
    }
}

bool SessionStateManager::callLogin1(const QString &method, const QVariantList &args)
{
    QDBusInterface iface(QStringLiteral("org.freedesktop.login1"),
                         QStringLiteral("/org/freedesktop/login1"),
                         QStringLiteral("org.freedesktop.login1.Manager"),
                         QDBusConnection::systemBus());
    if (!iface.isValid()) {
        return false;
    }
    QDBusMessage reply = iface.callWithArgumentList(QDBus::Block, method, args);
    return reply.type() != QDBusMessage::ErrorMessage;
}

bool SessionStateManager::callScreenSaver(const QString &method, const QVariantList &args)
{
    QDBusInterface iface(QStringLiteral("org.freedesktop.ScreenSaver"),
                         QStringLiteral("/org/freedesktop/ScreenSaver"),
                         QStringLiteral("org.freedesktop.ScreenSaver"),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return false;
    }
    QDBusMessage reply = iface.callWithArgumentList(QDBus::Block, method, args);
    return reply.type() != QDBusMessage::ErrorMessage;
}

void SessionStateManager::emitIdle(bool idle)
{
    if (m_idle == idle) {
        return;
    }
    m_idle = idle;
    emit IdleChanged(m_idle);
    if (m_idle) {
        emit SessionLocked();
    } else {
        emit SessionUnlocked();
    }
}

void SessionStateManager::emitActiveApp(const QString &appId)
{
    const QString next = appId.trimmed();
    if (next.isEmpty() || next == m_activeAppId) {
        return;
    }
    m_activeAppId = next;
    emit ActiveAppChanged(m_activeAppId);
}

void SessionStateManager::onScreenSaverActiveChanged(bool active)
{
    emitIdle(active);
}
