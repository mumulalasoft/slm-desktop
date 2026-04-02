#include "PortalSecretBridge.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>

namespace Slm::PortalAdapter {

PortalSecretBridge::PortalSecretBridge(QObject *parent)
    : QObject(parent)
{
}

void PortalSecretBridge::setServiceName(const QString &name)
{
    const QString normalized = name.trimmed();
    if (!normalized.isEmpty()) {
        m_serviceName = normalized;
    }
}

void PortalSecretBridge::setServicePath(const QString &path)
{
    const QString normalized = path.trimmed();
    if (!normalized.isEmpty()) {
        m_servicePath = normalized;
    }
}

void PortalSecretBridge::setServiceInterface(const QString &iface)
{
    const QString normalized = iface.trimmed();
    if (!normalized.isEmpty()) {
        m_serviceInterface = normalized;
    }
}

QVariantMap PortalSecretBridge::StoreSecret(const QString &appId,
                                            const QVariantMap &options,
                                            const QByteArray &secret) const
{
    return call("StoreSecret", {appId, options, secret});
}

QVariantMap PortalSecretBridge::GetSecret(const QString &appId, const QVariantMap &query) const
{
    return call("GetSecret", {appId, query});
}

QVariantMap PortalSecretBridge::DeleteSecret(const QString &appId, const QVariantMap &query) const
{
    return call("DeleteSecret", {appId, query});
}

QVariantMap PortalSecretBridge::ClearAppSecrets(const QString &appId) const
{
    return call("ClearAppSecrets", {appId});
}

QVariantMap PortalSecretBridge::DescribeSecret(const QString &appId, const QVariantMap &query) const
{
    return call("DescribeSecret", {appId, query});
}

QVariantMap PortalSecretBridge::ListOwnSecretMetadata(const QString &appId,
                                                      const QVariantMap &options) const
{
    return callList("ListOwnSecretMetadata", {appId, options}, QStringLiteral("items"));
}

QVariantMap PortalSecretBridge::ListSecretAppIds(const QVariantMap &options) const
{
    return callList("ListAppIds", {options}, QStringLiteral("apps"));
}

QVariantMap PortalSecretBridge::call(const char *method, const QVariantList &args) const
{
    QDBusInterface iface(m_serviceName,
                         m_servicePath,
                         m_serviceInterface,
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("secret-service-unavailable")},
            {QStringLiteral("message"), QStringLiteral("Secret service is unavailable.")},
        };
    }

    QDBusReply<QVariantMap> reply = iface.callWithArgumentList(QDBus::Block, method, args);
    if (!reply.isValid()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("secret-service-call-failed")},
            {QStringLiteral("message"), reply.error().message()},
        };
    }
    return reply.value();
}

QVariantMap PortalSecretBridge::callList(const char *method,
                                         const QVariantList &args,
                                         const QString &fieldName) const
{
    QDBusInterface iface(m_serviceName,
                         m_servicePath,
                         m_serviceInterface,
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("secret-service-unavailable")},
            {QStringLiteral("message"), QStringLiteral("Secret service is unavailable.")},
        };
    }

    QDBusReply<QVariantList> reply = iface.callWithArgumentList(QDBus::Block, method, args);
    if (!reply.isValid()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("secret-service-call-failed")},
            {QStringLiteral("message"), reply.error().message()},
        };
    }
    return {
        {QStringLiteral("ok"), true},
        {fieldName, reply.value()},
    };
}

} // namespace Slm::PortalAdapter
