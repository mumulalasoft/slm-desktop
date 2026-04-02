#include "networkmanagerbinding.h"
#include <QDebug>
#include <QtDBus/QDBusReply>

namespace {
constexpr uint kNmStateAsleep = 10;
constexpr uint kNmStateDisconnected = 20;
constexpr uint kNmStateDisconnecting = 30;
constexpr uint kNmStateConnecting = 40;
constexpr uint kNmStateConnectedLocal = 50;
constexpr uint kNmStateConnectedSite = 60;
constexpr uint kNmStateConnectedGlobal = 70;
}

NetworkManagerBinding::NetworkManagerBinding(QObject *parent)
    : SettingBinding(parent)
{
    m_nmInterface = new QDBusInterface("org.freedesktop.NetworkManager",
                                       "/org/freedesktop/NetworkManager",
                                       "org.freedesktop.NetworkManager",
                                       QDBusConnection::systemBus(),
                                       this);
    if (m_nmInterface->isValid()) {
        connect(m_nmInterface, SIGNAL(StateChanged(uint)), this, SLOT(onStateChanged(uint)));
    } else {
        qWarning() << "NetworkManagerBinding: Failed to connect to NetworkManager D-Bus interface";
    }
}

NetworkManagerBinding::~NetworkManagerBinding()
{
}

QVariant NetworkManagerBinding::value() const
{
    if (!m_nmInterface->isValid()) return "Disconnected";

    QDBusReply<uint> stateReply = m_nmInterface->call("state");
    if (!stateReply.isValid()) return "Disconnected";

    switch (stateReply.value()) {
        case kNmStateConnectedGlobal: return "Connected";
        case kNmStateConnectedSite: return "Connected (Site)";
        case kNmStateConnectedLocal: return "Connected (Local)";
        case kNmStateConnecting: return "Connecting";
        case kNmStateDisconnecting: return "Disconnecting";
        case kNmStateDisconnected: return "Disconnected";
        case kNmStateAsleep: return "Asleep";
        default: return "Unknown";
    }
}

void NetworkManagerBinding::setValue(const QVariant &newValue)
{
    Q_UNUSED(newValue)
    // Simplified: No action for now as activation/deactivation is complex
    endOperation(QStringLiteral("write"), false, QStringLiteral("networkmanager-write-not-supported"));
}

void NetworkManagerBinding::onStateChanged(uint state)
{
    emit valueChanged();
}
