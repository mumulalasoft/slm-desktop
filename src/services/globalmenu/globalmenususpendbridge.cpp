#include "globalmenususpendbridge.h"
#include "../../../globalmenumanager.h"

#include <QDBusConnection>
#include <QDBusMessage>

namespace {
constexpr auto kLogin1Service   = "org.freedesktop.login1";
constexpr auto kLogin1Path      = "/org/freedesktop/login1";
constexpr auto kLogin1Interface = "org.freedesktop.login1.Manager";
constexpr auto kSleepSignal     = "PrepareForSleep";
}

GlobalMenuSuspendBridge::GlobalMenuSuspendBridge(QObject *parent)
    : QObject(parent)
    , m_retryTimer(new QTimer(this))
{
    m_retryTimer->setInterval(kRetryIntervalMs);
    m_retryTimer->setSingleShot(false);
    connect(m_retryTimer, &QTimer::timeout, this, [this]() {
        if (!m_manager) {
            stopRetryLoop();
            return;
        }
        if (m_retryCount == 0) {
            m_manager->resetAfterResume();
        } else {
            m_manager->refresh();
        }
        ++m_retryCount;
        if (m_retryCount >= kMaxRetries) {
            stopRetryLoop();
        }
    });

    QDBusConnection::sessionBus().connect(
        QString::fromLatin1(kLogin1Service),
        QString::fromLatin1(kLogin1Path),
        QString::fromLatin1(kLogin1Interface),
        QString::fromLatin1(kSleepSignal),
        this,
        SLOT(onPrepareForSleep(bool)));
}

void GlobalMenuSuspendBridge::setMenuManager(GlobalMenuManager *manager)
{
    m_manager = manager;
}

bool GlobalMenuSuspendBridge::resumePending() const
{
    return m_resumePending;
}

void GlobalMenuSuspendBridge::onPrepareForSleep(bool sleeping)
{
    if (sleeping) {
        // System is going to sleep — stop any pending retry, clear state.
        stopRetryLoop();
    } else {
        // System woke up — start retry loop to rebind menus.
        startRetryLoop();
    }
}

void GlobalMenuSuspendBridge::startRetryLoop()
{
    m_retryCount = 0;
    m_resumePending = true;
    emit resumePendingChanged();
    m_retryTimer->start();
}

void GlobalMenuSuspendBridge::stopRetryLoop()
{
    m_retryTimer->stop();
    if (m_resumePending) {
        m_resumePending = false;
        emit resumePendingChanged();
    }
}
