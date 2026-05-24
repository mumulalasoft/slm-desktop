#include "trustservice.h"
#include "nearbyengine.h"
#include "sharingmanager.h"
#include "trustdatabase.h"

#include <QUuid>

TrustService::TrustService(SharingManager *manager, QObject *parent)
    : QObject(parent)
    , m_manager(manager)
{}

TrustService::~TrustService() = default;

QVariantMap TrustService::GetTrustedDevices() const
{
    return makeResult(true, QString(), {
        {QStringLiteral("devices"), m_manager->trustDatabase()->allDevices()},
    });
}

QVariantMap TrustService::GetDeviceTrust(const QString &deviceId) const
{
    const QVariantMap info = m_manager->trustDatabase()->deviceInfo(deviceId);
    if (info.isEmpty())
        return makeResult(false, QStringLiteral("device-not-found"));
    return makeResult(true, QString(), {{QStringLiteral("device"), info}});
}

QVariantMap TrustService::SetDeviceTrust(const QString &deviceId, const QString &level)
{
    const auto trustLevel = TrustDatabase::trustLevelFromString(level);
    const bool ok = m_manager->trustDatabase()->setTrustLevel(deviceId, trustLevel);
    if (!ok)
        return makeResult(false, QStringLiteral("trust-update-failed"));

    emit DeviceTrustChanged(deviceId, level);
    return makeResult(true);
}

QVariantMap TrustService::PairDevice(const QString &deviceId)
{
    // Start a pairing handshake — the result is delivered asynchronously via PairingCompleted
    const QString pairingId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QVariantMap deviceInfo = m_manager->nearbyEngine()->deviceInfo(deviceId);

    if (deviceInfo.isEmpty())
        return makeResult(false, QStringLiteral("device-not-found"));

    // Record as untrusted until the handshake completes
    m_manager->trustDatabase()->upsertDevice(deviceId, deviceInfo);

    emit PairingRequested(pairingId, deviceInfo);
    return makeResult(true, QString(), {{QStringLiteral("pairingId"), pairingId}});
}

QVariantMap TrustService::UnpairDevice(const QString &deviceId)
{
    const bool ok = m_manager->trustDatabase()->removeDevice(deviceId);
    if (!ok)
        return makeResult(false, QStringLiteral("device-not-found"));

    emit DeviceTrustChanged(deviceId, QStringLiteral("untrusted"));
    return makeResult(true);
}

QVariantMap TrustService::GetDevicePermissions(const QString &deviceId) const
{
    const QVariantMap perms = m_manager->trustDatabase()->permissions(deviceId);
    return makeResult(true, QString(), {{QStringLiteral("permissions"), perms}});
}

QVariantMap TrustService::SetDevicePermission(const QString &deviceId,
                                               const QString &permission,
                                               bool allowed)
{
    const bool ok = m_manager->trustDatabase()->setPermission(deviceId, permission, allowed);
    if (!ok)
        return makeResult(false, QStringLiteral("permission-update-failed"));

    emit DevicePermissionChanged(deviceId, permission, allowed);
    return makeResult(true);
}

QVariantMap TrustService::AcceptPairingRequest(const QString &pairingId)
{
    // In the full protocol the pairingId maps to a pending handshake object;
    // accepting finalises the key exchange and promotes to 'trusted'.
    emit PairingCompleted(pairingId, true);
    return makeResult(true, QString(), {{QStringLiteral("pairingId"), pairingId}});
}

QVariantMap TrustService::RejectPairingRequest(const QString &pairingId)
{
    emit PairingCompleted(pairingId, false);
    return makeResult(true);
}

QVariantMap TrustService::BlockDevice(const QString &deviceId)
{
    const bool ok = m_manager->trustDatabase()->setTrustLevel(
        deviceId, TrustDatabase::TrustLevel::Blocked);
    if (!ok)
        return makeResult(false, QStringLiteral("block-failed"));
    emit DeviceTrustChanged(deviceId, QStringLiteral("blocked"));
    return makeResult(true);
}

QVariantMap TrustService::UnblockDevice(const QString &deviceId)
{
    const bool ok = m_manager->trustDatabase()->setTrustLevel(
        deviceId, TrustDatabase::TrustLevel::Untrusted);
    if (!ok)
        return makeResult(false, QStringLiteral("unblock-failed"));
    emit DeviceTrustChanged(deviceId, QStringLiteral("untrusted"));
    return makeResult(true);
}

QVariantMap TrustService::makeResult(bool ok, const QString &error, const QVariantMap &extra)
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), ok);
    if (!error.isEmpty())
        result.insert(QStringLiteral("error"), error);
    for (auto it = extra.begin(); it != extra.end(); ++it)
        result.insert(it.key(), it.value());
    return result;
}
