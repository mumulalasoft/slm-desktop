#pragma once

#include <QObject>
#include <QVariantMap>

namespace Slm::PortalAdapter {

class PortalSecretBridge : public QObject
{
    Q_OBJECT

public:
    explicit PortalSecretBridge(QObject *parent = nullptr);

    void setServiceName(const QString &name);
    void setServicePath(const QString &path);
    void setServiceInterface(const QString &iface);

    QVariantMap StoreSecret(const QString &appId, const QVariantMap &options, const QByteArray &secret) const;
    QVariantMap GetSecret(const QString &appId, const QVariantMap &query) const;
    QVariantMap DeleteSecret(const QString &appId, const QVariantMap &query) const;
    QVariantMap ClearAppSecrets(const QString &appId) const;
    QVariantMap DescribeSecret(const QString &appId, const QVariantMap &query) const;
    QVariantMap ListOwnSecretMetadata(const QString &appId, const QVariantMap &options) const;
    QVariantMap ListSecretAppIds(const QVariantMap &options) const;

private:
    QVariantMap call(const char *method, const QVariantList &args) const;
    QVariantMap callList(const char *method, const QVariantList &args, const QString &fieldName) const;

    QString m_serviceName = QStringLiteral("org.slm.Secret1");
    QString m_servicePath = QStringLiteral("/org/slm/Secret1");
    QString m_serviceInterface = QStringLiteral("org.slm.Secret1");
};

} // namespace Slm::PortalAdapter
