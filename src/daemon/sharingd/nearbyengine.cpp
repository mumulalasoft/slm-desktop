#include "nearbyengine.h"
#include "adapters/avahiadapter.h"

#include <QCryptographicHash>

NearbyEngine::NearbyEngine(AvahiAdapter *avahi, QObject *parent)
    : QObject(parent)
    , m_avahi(avahi)
{
    connect(m_avahi, &AvahiAdapter::capabilityEvent, this, &NearbyEngine::onAdapterEvent);
}

void NearbyEngine::startDiscovery()
{
    if (m_discovering)
        return;
    m_discovering = true;
    m_avahi->startBrowse(QStringLiteral("_slm-sharing._tcp"));
    emit discoveryStateChanged(true);
}

void NearbyEngine::stopDiscovery()
{
    if (!m_discovering)
        return;
    m_discovering = false;
    m_avahi->stopBrowse();
    emit discoveryStateChanged(false);
}

QVariantList NearbyEngine::devices() const
{
    return QVariantList(m_devices.values().begin(), m_devices.values().end());
}

QVariantMap NearbyEngine::deviceInfo(const QString &deviceId) const
{
    return m_devices.value(deviceId);
}

bool NearbyEngine::deviceKnown(const QString &deviceId) const
{
    return m_devices.contains(deviceId);
}

void NearbyEngine::onAdapterEvent(const QString &event, const QVariantMap &payload)
{
    const QString mdnsName = payload.value(QStringLiteral("name")).toString();
    const QString deviceId = deviceIdFromName(mdnsName);

    if (event == QLatin1String("device-resolved")) {
        const QVariantMap info = buildDeviceInfo(mdnsName, payload);
        const bool isNew = !m_devices.contains(deviceId);
        m_devices.insert(deviceId, info);
        if (isNew)
            emit deviceFound(deviceId, info);
        else
            emit deviceUpdated(deviceId, info);
    } else if (event == QLatin1String("device-lost")) {
        if (m_devices.remove(deviceId))
            emit deviceLost(deviceId);
    }
}

QString NearbyEngine::deviceIdFromName(const QString &mdnsName) const
{
    // Derive a stable UUID-like ID from the mDNS service name
    const QByteArray hash = QCryptographicHash::hash(
        mdnsName.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.left(16).toHex());
}

QVariantMap NearbyEngine::buildDeviceInfo(const QString &mdnsName,
                                           const QVariantMap &payload) const
{
    // Extract SLM-specific TXT record fields if present
    // In the real protocol these carry: type, capabilities, publicKeyFingerprint
    const QString deviceId = deviceIdFromName(mdnsName);
    return {
        {QStringLiteral("deviceId"), deviceId},
        {QStringLiteral("name"), mdnsName},
        {QStringLiteral("type"), payload.value(QStringLiteral("type"), QStringLiteral("unknown"))},
        {QStringLiteral("address"), payload.value(QStringLiteral("address"))},
        {QStringLiteral("port"), payload.value(QStringLiteral("port"))},
        {QStringLiteral("capabilities"), payload.value(QStringLiteral("capabilities"), QStringList{QStringLiteral("file")})},
        {QStringLiteral("trustLevel"), QStringLiteral("untrusted")},
        {QStringLiteral("online"), true},
        {QStringLiteral("battery"), -1},
        {QStringLiteral("transport"), QStringList{QStringLiteral("mdns")}},
    };
}
