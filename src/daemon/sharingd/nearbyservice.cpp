#include "nearbyservice.h"
#include "nearbyengine.h"
#include "sharingmanager.h"

NearbyService::NearbyService(SharingManager *manager, QObject *parent)
    : QObject(parent)
    , m_manager(manager)
{
    connectEngineSignals();
}

NearbyService::~NearbyService() = default;

QVariantMap NearbyService::GetDevices() const
{
    return makeResult(true, QString(), {
        {QStringLiteral("devices"), m_manager->nearbyEngine()->devices()},
    });
}

QVariantMap NearbyService::StartDiscovery()
{
    m_manager->nearbyEngine()->startDiscovery();
    return makeResult(true, QString(), {
        {QStringLiteral("active"), m_manager->nearbyEngine()->isDiscovering()},
    });
}

QVariantMap NearbyService::StopDiscovery()
{
    m_manager->nearbyEngine()->stopDiscovery();
    return makeResult(true);
}

QVariantMap NearbyService::GetDiscoveryState() const
{
    return makeResult(true, QString(), {
        {QStringLiteral("active"), m_manager->nearbyEngine()->isDiscovering()},
    });
}

QVariantMap NearbyService::SendFileTo(const QString &deviceId, const QString &path)
{
    if (!m_manager->nearbyEngine()->deviceKnown(deviceId))
        return makeResult(false, QStringLiteral("device-not-found"));

    auto *session = m_manager->startOutgoingTransfer(deviceId, path);
    if (!session)
        return makeResult(false, QStringLiteral("transfer-start-failed"));

    return makeResult(true, QString(), {
        {QStringLiteral("transferId"), session->transferId()},
    });
}

QVariantMap NearbyService::GetDeviceInfo(const QString &deviceId) const
{
    const QVariantMap info = m_manager->nearbyEngine()->deviceInfo(deviceId);
    if (info.isEmpty())
        return makeResult(false, QStringLiteral("device-not-found"));

    // Merge in trust info
    QVariantMap enriched = info;
    const auto trustInfo = m_manager->trustDatabase()->deviceInfo(deviceId);
    if (!trustInfo.isEmpty())
        enriched.insert(QStringLiteral("trustLevel"), trustInfo.value(QStringLiteral("trustLevel")));

    return makeResult(true, QString(), {{QStringLiteral("device"), enriched}});
}

void NearbyService::connectEngineSignals()
{
    auto *engine = m_manager->nearbyEngine();
    connect(engine, &NearbyEngine::deviceFound, this, &NearbyService::DeviceFound);
    connect(engine, &NearbyEngine::deviceLost, this, &NearbyService::DeviceLost);
    connect(engine, &NearbyEngine::deviceUpdated, this, &NearbyService::DeviceUpdated);
    connect(engine, &NearbyEngine::discoveryStateChanged, this, &NearbyService::DiscoveryStateChanged);
}

QVariantMap NearbyService::makeResult(bool ok, const QString &error, const QVariantMap &extra)
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), ok);
    if (!error.isEmpty())
        result.insert(QStringLiteral("error"), error);
    for (auto it = extra.begin(); it != extra.end(); ++it)
        result.insert(it.key(), it.value());
    return result;
}
