#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace Slm::Secret {

class SecretService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.slm.Secret1")

public:
    explicit SecretService(QObject *parent = nullptr);

public slots:
    QVariantMap StoreSecret(const QString &appId, const QVariantMap &options, const QByteArray &secret);
    QVariantMap GetSecret(const QString &appId, const QVariantMap &query) const;
    QVariantMap DeleteSecret(const QString &appId, const QVariantMap &query);
    QVariantMap DescribeSecret(const QString &appId, const QVariantMap &query) const;
    QVariantMap ClearAppSecrets(const QString &appId);
    QVariantList ListOwnSecretMetadata(const QString &appId, const QVariantMap &options) const;
    QVariantList ListAppIds(const QVariantMap &options) const;

private:
    struct SecretEntry {
        QString ownerAppId;
        QString namespaceName;
        QString key;
        QString label;
        QString sensitivity;
        QByteArray value;
        qint64 createdAtMs = 0;
        qint64 updatedAtMs = 0;
    };

    static QString normalizeAppId(const QString &appId);
    static QString normalizeNamespace(const QVariantMap &map);
    static QString normalizeKey(const QVariantMap &map);
    static QString makeStorageKey(const QString &appId, const QString &ns, const QString &key);
    static qint64 nowMs();
    static QVariantMap toMetadata(const SecretEntry &entry);
    static QVariantMap error(const QString &code, const QString &message, const QVariantMap &extra = {});

    QHash<QString, SecretEntry> m_entries;
};

} // namespace Slm::Secret
