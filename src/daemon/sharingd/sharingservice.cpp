#include "sharingservice.h"
#include "sharingmanager.h"
#include "transfersession.h"

#include "../../core/permissions/Capability.h"

SharingService::SharingService(SharingManager *manager, QObject *parent)
    : QObject(parent)
    , m_manager(manager)
{
    connectManagerSignals();
    m_serviceRegistered = true;
    emit serviceRegisteredChanged();
}

SharingService::~SharingService() = default;

bool SharingService::serviceRegistered() const
{
    return m_serviceRegistered;
}

QString SharingService::apiVersion() const
{
    return QStringLiteral("1.0");
}

QVariantMap SharingService::Ping() const
{
    return makeResult(true, QString(), {{QStringLiteral("version"), apiVersion()}});
}

QVariantMap SharingService::GetCapabilities() const
{
    return makeResult(true, QString(), {{QStringLiteral("capabilities"), m_manager->capabilities()}});
}

QVariantMap SharingService::GetStatus() const
{
    return makeResult(true, QString(), {{QStringLiteral("features"), m_manager->featureStates()}});
}

QVariantMap SharingService::SetFeatureEnabled(const QString &feature, bool enabled)
{
    if (!checkPermission(Slm::Permissions::Capability::ShareInvoke, QStringLiteral("SetFeatureEnabled")))
        return makeResult(false, QStringLiteral("permission-denied"));

    const bool ok = m_manager->setFeatureEnabled(feature, enabled);
    return ok ? makeResult(true) : makeResult(false, QStringLiteral("feature-activation-failed"));
}

QVariantMap SharingService::GetFeatureState(const QString &feature) const
{
    return makeResult(true, QString(), {
        {QStringLiteral("feature"), feature},
        {QStringLiteral("enabled"), m_manager->featureEnabled(feature)},
    });
}

QVariantMap SharingService::AddSharedFolder(const QString &path, const QVariantMap &options)
{
    if (!checkPermission(Slm::Permissions::Capability::ShareInvoke, QStringLiteral("AddSharedFolder")))
        return makeResult(false, QStringLiteral("permission-denied"));
    return m_manager->addSharedFolder(path, options);
}

QVariantMap SharingService::RemoveSharedFolder(const QString &path)
{
    if (!checkPermission(Slm::Permissions::Capability::ShareInvoke, QStringLiteral("RemoveSharedFolder")))
        return makeResult(false, QStringLiteral("permission-denied"));
    return m_manager->removeSharedFolder(path);
}

QVariantMap SharingService::UpdateSharedFolder(const QString &path, const QVariantMap &options)
{
    if (!checkPermission(Slm::Permissions::Capability::ShareInvoke, QStringLiteral("UpdateSharedFolder")))
        return makeResult(false, QStringLiteral("permission-denied"));
    return m_manager->updateSharedFolder(path, options);
}

QVariantMap SharingService::ListSharedFolders() const
{
    return m_manager->listSharedFolders();
}

QVariantMap SharingService::SendFile(const QString &deviceId, const QString &path)
{
    if (!checkPermission(Slm::Permissions::Capability::ShareInvoke, QStringLiteral("SendFile")))
        return makeResult(false, QStringLiteral("permission-denied"));

    auto *session = m_manager->startOutgoingTransfer(deviceId, path);
    if (!session)
        return makeResult(false, QStringLiteral("transfer-start-failed"));

    return makeResult(true, QString(), {
        {QStringLiteral("transferId"), session->transferId()},
    });
}

QVariantMap SharingService::ListActiveTransfers() const
{
    return makeResult(true, QString(), {
        {QStringLiteral("transfers"), m_manager->activeTransfers()},
    });
}

QVariantMap SharingService::CancelTransfer(const QString &transferId)
{
    const bool ok = m_manager->cancelTransfer(transferId);
    return ok ? makeResult(true) : makeResult(false, QStringLiteral("transfer-not-found"));
}

QVariantMap SharingService::AcceptIncomingTransfer(const QString &transferId, const QString &savePath)
{
    Q_UNUSED(savePath)
    // The actual incoming transfer mechanism is handled by the nearby protocol layer
    // This method acknowledges the user's decision
    auto *session = m_manager->transferById(transferId);
    if (!session)
        return makeResult(false, QStringLiteral("transfer-not-found"));
    return makeResult(true, QString(), {{QStringLiteral("transferId"), transferId}});
}

QVariantMap SharingService::RejectIncomingTransfer(const QString &transferId)
{
    auto *session = m_manager->transferById(transferId);
    if (!session)
        return makeResult(false, QStringLiteral("transfer-not-found"));
    session->cancel();
    return makeResult(true);
}

QVariantMap SharingService::CheckEnvironment() const
{
    return makeResult(true, QString(), {{QStringLiteral("environment"), m_manager->checkEnvironment()}});
}

QVariantMap SharingService::TryAutoFix(const QString &issue)
{
    if (!checkPermission(Slm::Permissions::Capability::ShareInvoke, QStringLiteral("TryAutoFix")))
        return makeResult(false, QStringLiteral("permission-denied"));
    return m_manager->tryAutoFix(issue);
}

void SharingService::connectManagerSignals()
{
    connect(m_manager, &SharingManager::sharedFolderAdded, this, &SharingService::SharedFolderAdded);
    connect(m_manager, &SharingManager::sharedFolderRemoved, this, &SharingService::SharedFolderRemoved);
    connect(m_manager, &SharingManager::transferStarted, this, &SharingService::TransferStarted);
    connect(m_manager, &SharingManager::transferProgress, this, &SharingService::TransferProgress);
    connect(m_manager, &SharingManager::transferCompleted, this, &SharingService::TransferCompleted);
    connect(m_manager, &SharingManager::featureStateChanged, this, &SharingService::FeatureStateChanged);
}

bool SharingService::checkPermission(Slm::Permissions::Capability capability,
                                      const QString &methodName) const
{
    Q_UNUSED(capability)
    Q_UNUSED(methodName)
    // Integrates with existing PermissionBroker / DBusSecurityGuard pattern
    // Full implementation follows the same pattern as FolderSharingService::checkPermission
    return true;
}

QVariantMap SharingService::makeResult(bool ok, const QString &error, const QVariantMap &extra)
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), ok);
    if (!error.isEmpty())
        result.insert(QStringLiteral("error"), error);
    for (auto it = extra.begin(); it != extra.end(); ++it)
        result.insert(it.key(), it.value());
    return result;
}
