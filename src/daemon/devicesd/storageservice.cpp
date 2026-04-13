#include "storageservice.h"

#include "../../../dbuslogutils.h"
#include "../../core/permissions/Capability.h"
#include "devicesmanager.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>

namespace {
constexpr const char kService[] = "org.slm.Desktop.Storage";
constexpr const char kPath[] = "/org/slm/Desktop/Storage";
constexpr const char kApiVersion[] = "1.0";
}

StorageService::StorageService(DevicesManager *manager, QObject *parent)
    : QObject(parent)
    , m_manager(manager)
{
    using namespace Slm::Permissions;
    m_store.open();
    m_engine.setStore(&m_store);
    m_broker.setStore(&m_store);
    m_broker.setPolicyEngine(&m_engine);
    m_broker.setAuditLogger(&m_audit);

    m_guard.setTrustResolver(&m_resolver);
    m_guard.setPermissionBroker(&m_broker);
    m_guard.registerMethodCapability(
        QStringLiteral("org.slm.Desktop.Storage"), QStringLiteral("GetStorageLocations"), Capability::DeviceMount);
    m_guard.registerMethodCapability(
        QStringLiteral("org.slm.Desktop.Storage"), QStringLiteral("StoragePolicyForPath"), Capability::DeviceMount);
    m_guard.registerMethodCapability(
        QStringLiteral("org.slm.Desktop.Storage"), QStringLiteral("SetStoragePolicyForPath"), Capability::DeviceMount);
    m_guard.registerMethodCapability(
        QStringLiteral("org.slm.Desktop.Storage"), QStringLiteral("Mount"), Capability::DeviceMount);
    m_guard.registerMethodCapability(
        QStringLiteral("org.slm.Desktop.Storage"), QStringLiteral("Eject"), Capability::DeviceEject);
    m_guard.registerMethodCapability(
        QStringLiteral("org.slm.Desktop.Storage"), QStringLiteral("ConnectServer"), Capability::DeviceMount);

    if (m_manager) {
        connect(m_manager, &DevicesManager::StorageChanged,
                this, &StorageService::StorageLocationsChanged);
    }
    registerDbusService();
}

StorageService::~StorageService()
{
    if (!m_serviceRegistered) {
        return;
    }
    QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(kPath));
    QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(kService));
}

bool StorageService::serviceRegistered() const
{
    return m_serviceRegistered;
}

QString StorageService::apiVersion() const
{
    return QString::fromLatin1(kApiVersion);
}

QVariantMap StorageService::Ping() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("service"), QString::fromLatin1(kService)},
        {QStringLiteral("registered"), m_serviceRegistered},
        {QStringLiteral("api_version"), apiVersion()},
    };
}

QVariantMap StorageService::GetCapabilities() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("service"), QString::fromLatin1(kService)},
        {QStringLiteral("api_version"), apiVersion()},
        {QStringLiteral("capabilities"), QStringList{
             QStringLiteral("storage-locations"),
             QStringLiteral("storage-policy-read"),
             QStringLiteral("storage-policy-write"),
             QStringLiteral("mount"),
             QStringLiteral("eject"),
             QStringLiteral("connect-server"),
        }},
    };
}

QVariantMap StorageService::GetStorageLocations()
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    if (calledFromDBus()) {
        using namespace Slm::Permissions;
        const auto decision = m_guard.checkMethod(
            message(),
            QStringLiteral("org.slm.Desktop.Storage"),
            QStringLiteral("GetStorageLocations"));
        if (!decision.isAllowed()) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("permission-denied")},
            };
        }
    }
    const QVariantMap result = m_manager ? m_manager->GetStorageLocations()
                                         : QVariantMap{
                                               {QStringLiteral("ok"), false},
                                               {QStringLiteral("error"), QStringLiteral("manager-unavailable")}
                                           };
    QVariantMap payload;
    payload.insert(QStringLiteral("ok"), result.value(QStringLiteral("ok")).toBool());
    payload.insert(QStringLiteral("error"), result.value(QStringLiteral("error")).toString());
    payload.insert(QStringLiteral("count"), result.value(QStringLiteral("rows")).toList().size());
    SlmDbusLog::logEvent(QString::fromLatin1(kService), QStringLiteral("GetStorageLocations"),
                         requestId, caller, QString(), QStringLiteral("finish"), payload);
    return result;
}

QVariantMap StorageService::Mount(const QString &target)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    if (calledFromDBus()) {
        using namespace Slm::Permissions;
        const auto decision = m_guard.checkMethod(
            message(),
            QStringLiteral("org.slm.Desktop.Storage"),
            QStringLiteral("Mount"));
        if (!decision.isAllowed()) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("permission-denied")},
            };
        }
    }
    const QVariantMap result = m_manager ? m_manager->Mount(target)
                                         : QVariantMap{{QStringLiteral("ok"), false}};
    QVariantMap payload;
    payload.insert(QStringLiteral("target"), target);
    payload.insert(QStringLiteral("ok"), result.value(QStringLiteral("ok")).toBool());
    payload.insert(QStringLiteral("error"), result.value(QStringLiteral("error")).toString());
    SlmDbusLog::logEvent(QString::fromLatin1(kService), QStringLiteral("Mount"),
                         requestId, caller, QString(), QStringLiteral("finish"), payload);
    return result;
}

QVariantMap StorageService::StoragePolicyForPath(const QString &path)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    if (calledFromDBus()) {
        using namespace Slm::Permissions;
        const auto decision = m_guard.checkMethod(
            message(),
            QStringLiteral("org.slm.Desktop.Storage"),
            QStringLiteral("StoragePolicyForPath"));
        if (!decision.isAllowed()) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("permission-denied")},
            };
        }
    }
    const QVariantMap result = m_manager ? m_manager->StoragePolicyForPath(path)
                                         : QVariantMap{{QStringLiteral("ok"), false},
                                                       {QStringLiteral("error"), QStringLiteral("manager-unavailable")}};
    QVariantMap payload;
    payload.insert(QStringLiteral("path"), path);
    payload.insert(QStringLiteral("ok"), result.value(QStringLiteral("ok")).toBool());
    payload.insert(QStringLiteral("error"), result.value(QStringLiteral("error")).toString());
    payload.insert(QStringLiteral("policyKey"), result.value(QStringLiteral("policyKey")).toString());
    payload.insert(QStringLiteral("policyScope"), result.value(QStringLiteral("policyScope")).toString());
    SlmDbusLog::logEvent(QString::fromLatin1(kService), QStringLiteral("StoragePolicyForPath"),
                         requestId, caller, QString(), QStringLiteral("finish"), payload);
    return result;
}

QVariantMap StorageService::SetStoragePolicyForPath(const QString &path,
                                                    const QVariantMap &policyPatch,
                                                    const QString &scope)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    if (calledFromDBus()) {
        using namespace Slm::Permissions;
        const auto decision = m_guard.checkMethod(
            message(),
            QStringLiteral("org.slm.Desktop.Storage"),
            QStringLiteral("SetStoragePolicyForPath"));
        if (!decision.isAllowed()) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("permission-denied")},
            };
        }
    }
    const QVariantMap result = m_manager ? m_manager->SetStoragePolicyForPath(path, policyPatch, scope)
                                         : QVariantMap{{QStringLiteral("ok"), false},
                                                       {QStringLiteral("error"), QStringLiteral("manager-unavailable")}};
    QVariantMap payload;
    payload.insert(QStringLiteral("path"), path);
    payload.insert(QStringLiteral("scope"), scope);
    payload.insert(QStringLiteral("ok"), result.value(QStringLiteral("ok")).toBool());
    payload.insert(QStringLiteral("error"), result.value(QStringLiteral("error")).toString());
    payload.insert(QStringLiteral("policyKey"), result.value(QStringLiteral("policyKey")).toString());
    payload.insert(QStringLiteral("policyScope"), result.value(QStringLiteral("policyScope")).toString());
    SlmDbusLog::logEvent(QString::fromLatin1(kService), QStringLiteral("SetStoragePolicyForPath"),
                         requestId, caller, QString(), QStringLiteral("finish"), payload);
    return result;
}

QVariantMap StorageService::Eject(const QString &target)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    if (calledFromDBus()) {
        using namespace Slm::Permissions;
        const auto decision = m_guard.checkMethod(
            message(),
            QStringLiteral("org.slm.Desktop.Storage"),
            QStringLiteral("Eject"));
        if (!decision.isAllowed()) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("permission-denied")},
            };
        }
    }
    const QVariantMap result = m_manager ? m_manager->Eject(target)
                                         : QVariantMap{{QStringLiteral("ok"), false}};
    QVariantMap payload;
    payload.insert(QStringLiteral("target"), target);
    payload.insert(QStringLiteral("ok"), result.value(QStringLiteral("ok")).toBool());
    payload.insert(QStringLiteral("error"), result.value(QStringLiteral("error")).toString());
    SlmDbusLog::logEvent(QString::fromLatin1(kService), QStringLiteral("Eject"),
                         requestId, caller, QString(), QStringLiteral("finish"), payload);
    return result;
}

QVariantMap StorageService::ConnectServer(const QString &serverUri)
{
    QString uri = serverUri.trimmed();
    if (uri.isEmpty()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("invalid-uri")},
        };
    }
    if (!uri.contains(QStringLiteral("://"))) {
        uri.prepend(QStringLiteral("smb://"));
    }
    return Mount(uri);
}

void StorageService::registerDbusService()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }
    QDBusConnectionInterface *iface = bus.interface();
    if (!iface) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }
    if (iface->isServiceRegistered(QString::fromLatin1(kService)).value()) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }
    if (!bus.registerService(QString::fromLatin1(kService))) {
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }
    const bool ok = bus.registerObject(QString::fromLatin1(kPath),
                                       this,
                                       QDBusConnection::ExportAllSlots |
                                           QDBusConnection::ExportAllSignals |
                                           QDBusConnection::ExportAllProperties);
    if (!ok) {
        bus.unregisterService(QString::fromLatin1(kService));
        m_serviceRegistered = false;
        emit serviceRegisteredChanged();
        return;
    }
    m_serviceRegistered = true;
    emit serviceRegisteredChanged();
}
