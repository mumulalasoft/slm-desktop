#include "devicesservice.h"

#include "../../../dbuslogutils.h"
#include "../../core/permissions/Capability.h"
#include "devicesmanager.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>

namespace {
constexpr const char kService[] = "org.slm.Desktop.Devices";
constexpr const char kPath[] = "/org/slm/Desktop/Devices";
constexpr const char kApiVersion[] = "1.0";
}

DevicesService::DevicesService(DevicesManager *manager, QObject *parent)
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
        QStringLiteral("org.slm.Desktop.Devices"), QStringLiteral("Mount"), Capability::DeviceMount);
    m_guard.registerMethodCapability(
        QStringLiteral("org.slm.Desktop.Devices"), QStringLiteral("Eject"), Capability::DeviceEject);
    m_guard.registerMethodCapability(
        QStringLiteral("org.slm.Desktop.Devices"), QStringLiteral("Unlock"), Capability::DeviceUnlock);
    m_guard.registerMethodCapability(
        QStringLiteral("org.slm.Desktop.Devices"), QStringLiteral("Format"), Capability::DeviceFormat);
    registerDbusService();
}

DevicesService::~DevicesService()
{
    if (!m_serviceRegistered) {
        return;
    }
    QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(kPath));
    QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(kService));
}

bool DevicesService::serviceRegistered() const
{
    return m_serviceRegistered;
}

QString DevicesService::apiVersion() const
{
    return QString::fromLatin1(kApiVersion);
}

QVariantMap DevicesService::Ping() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("service"), QString::fromLatin1(kService)},
        {QStringLiteral("registered"), m_serviceRegistered},
        {QStringLiteral("api_version"), apiVersion()},
    };
}

QVariantMap DevicesService::GetCapabilities() const
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("service"), QString::fromLatin1(kService)},
        {QStringLiteral("api_version"), apiVersion()},
        {QStringLiteral("capabilities"), QStringList{
             QStringLiteral("mount"),
             QStringLiteral("eject"),
             QStringLiteral("unlock"),
             QStringLiteral("format"),
        }},
    };
}

QVariantMap DevicesService::Mount(const QString &target)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    if (calledFromDBus()) {
        using namespace Slm::Permissions;
        const auto decision = m_guard.checkMethod(
            message(),
            QStringLiteral("org.slm.Desktop.Devices"),
            QStringLiteral("Mount"));
        if (!decision.isAllowed()) {
            const QVariantMap denied{
                {QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("permission-denied")},
            };
            QVariantMap payload;
            payload.insert(QStringLiteral("target"), target);
            payload.insert(QStringLiteral("reason"), decision.reason);
            SlmDbusLog::logEvent(QString::fromLatin1(kService), QStringLiteral("Mount"), requestId, caller, QString(), QStringLiteral("denied"), payload);
            return denied;
        }
    }
    const QVariantMap result = m_manager ? m_manager->Mount(target)
                                         : QVariantMap{{QStringLiteral("ok"), false}};
    QVariantMap payload;
    payload.insert(QStringLiteral("target"), target);
    payload.insert(QStringLiteral("ok"), result.value(QStringLiteral("ok")).toBool());
    payload.insert(QStringLiteral("error"), result.value(QStringLiteral("error")).toString());
    SlmDbusLog::logEvent(QString::fromLatin1(kService), QStringLiteral("Mount"), requestId, caller, QString(), QStringLiteral("finish"), payload);
    return result;
}

QVariantMap DevicesService::Eject(const QString &target)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    if (calledFromDBus()) {
        using namespace Slm::Permissions;
        const auto decision = m_guard.checkMethod(
            message(),
            QStringLiteral("org.slm.Desktop.Devices"),
            QStringLiteral("Eject"));
        if (!decision.isAllowed()) {
            const QVariantMap denied{
                {QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("permission-denied")},
            };
            QVariantMap payload;
            payload.insert(QStringLiteral("target"), target);
            payload.insert(QStringLiteral("reason"), decision.reason);
            SlmDbusLog::logEvent(QString::fromLatin1(kService), QStringLiteral("Eject"), requestId, caller, QString(), QStringLiteral("denied"), payload);
            return denied;
        }
    }
    const QVariantMap result = m_manager ? m_manager->Eject(target)
                                         : QVariantMap{{QStringLiteral("ok"), false}};
    QVariantMap payload;
    payload.insert(QStringLiteral("target"), target);
    payload.insert(QStringLiteral("ok"), result.value(QStringLiteral("ok")).toBool());
    payload.insert(QStringLiteral("error"), result.value(QStringLiteral("error")).toString());
    SlmDbusLog::logEvent(QString::fromLatin1(kService), QStringLiteral("Eject"), requestId, caller, QString(), QStringLiteral("finish"), payload);
    return result;
}

QVariantMap DevicesService::Unlock(const QString &target, const QString &passphrase)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    if (calledFromDBus()) {
        using namespace Slm::Permissions;
        const auto decision = m_guard.checkMethod(
            message(),
            QStringLiteral("org.slm.Desktop.Devices"),
            QStringLiteral("Unlock"));
        if (!decision.isAllowed()) {
            const QVariantMap denied{
                {QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("permission-denied")},
                {QStringLiteral("stdout"), QString()},
                {QStringLiteral("stderr"), QString()},
            };
            QVariantMap payload;
            payload.insert(QStringLiteral("target"), target);
            payload.insert(QStringLiteral("reason"), decision.reason);
            SlmDbusLog::logEvent(QString::fromLatin1(kService), QStringLiteral("Unlock"), requestId, caller, QString(), QStringLiteral("denied"), payload);
            return denied;
        }
    }
    const QVariantMap result = m_manager ? m_manager->Unlock(target, passphrase)
                                         : QVariantMap{{QStringLiteral("ok"), false}};
    QVariantMap payload;
    payload.insert(QStringLiteral("target"), target);
    payload.insert(QStringLiteral("ok"), result.value(QStringLiteral("ok")).toBool());
    payload.insert(QStringLiteral("error"), result.value(QStringLiteral("error")).toString());
    payload.insert(QStringLiteral("passphrase_set"), !passphrase.isEmpty());
    SlmDbusLog::logEvent(QString::fromLatin1(kService), QStringLiteral("Unlock"), requestId, caller, QString(), QStringLiteral("finish"), payload);
    return result;
}

QVariantMap DevicesService::Format(const QString &target, const QString &filesystem)
{
    const QString requestId = SlmDbusLog::nextRequestId();
    const QString caller = calledFromDBus() ? message().service() : QString();
    if (calledFromDBus()) {
        using namespace Slm::Permissions;
        const auto decision = m_guard.checkMethod(
            message(),
            QStringLiteral("org.slm.Desktop.Devices"),
            QStringLiteral("Format"));
        if (!decision.isAllowed()) {
            const QVariantMap denied{
                {QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("permission-denied")},
                {QStringLiteral("stdout"), QString()},
                {QStringLiteral("stderr"), QString()},
            };
            QVariantMap payload;
            payload.insert(QStringLiteral("target"), target);
            payload.insert(QStringLiteral("filesystem"), filesystem);
            payload.insert(QStringLiteral("reason"), decision.reason);
            SlmDbusLog::logEvent(QString::fromLatin1(kService), QStringLiteral("Format"), requestId, caller, QString(), QStringLiteral("denied"), payload);
            return denied;
        }
    }
    const QVariantMap result = m_manager ? m_manager->Format(target, filesystem)
                                         : QVariantMap{{QStringLiteral("ok"), false}};
    QVariantMap payload;
    payload.insert(QStringLiteral("target"), target);
    payload.insert(QStringLiteral("ok"), result.value(QStringLiteral("ok")).toBool());
    payload.insert(QStringLiteral("error"), result.value(QStringLiteral("error")).toString());
    payload.insert(QStringLiteral("filesystem"), filesystem);
    SlmDbusLog::logEvent(QString::fromLatin1(kService), QStringLiteral("Format"), requestId, caller, QString(), QStringLiteral("finish"), payload);
    return result;
}

void DevicesService::registerDbusService()
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

QString DevicesService::callerIdentity() const
{
    if (!calledFromDBus()) {
        return QString();
    }
    return message().service();
}
