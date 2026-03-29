#include "secretservice.h"

#include <algorithm>
#include <QDateTime>
#include <QSet>

namespace Slm::Secret {

namespace {
QString normalizeSimple(const QString &value)
{
    return value.trimmed().toLower();
}
}

SecretService::SecretService(QObject *parent)
    : QObject(parent)
{
}

QVariantMap SecretService::StoreSecret(const QString &appId,
                                       const QVariantMap &options,
                                       const QByteArray &secret)
{
    const QString owner = normalizeAppId(appId);
    const QString ns = normalizeNamespace(options);
    const QString key = normalizeKey(options);
    if (owner.isEmpty() || ns.isEmpty() || key.isEmpty()) {
        return error(QStringLiteral("invalid-argument"),
                     QStringLiteral("appId, namespace, and key are required."));
    }

    const QString storageKey = makeStorageKey(owner, ns, key);
    SecretEntry entry = m_entries.value(storageKey);
    const bool isNew = entry.ownerAppId.isEmpty();
    if (isNew) {
        entry.ownerAppId = owner;
        entry.namespaceName = ns;
        entry.key = key;
        entry.createdAtMs = nowMs();
    }
    entry.label = options.value(QStringLiteral("label")).toString().trimmed();
    entry.sensitivity = normalizeSimple(options.value(QStringLiteral("sensitivity")).toString());
    if (entry.sensitivity.isEmpty()) {
        entry.sensitivity = QStringLiteral("normal");
    }
    entry.value = secret;
    entry.updatedAtMs = nowMs();
    m_entries.insert(storageKey, entry);

    QVariantMap out{
        {QStringLiteral("ok"), true},
        {QStringLiteral("created"), isNew},
        {QStringLiteral("metadata"), toMetadata(entry)},
    };
    return out;
}

QVariantMap SecretService::GetSecret(const QString &appId, const QVariantMap &query) const
{
    const QString owner = normalizeAppId(appId);
    const QString ns = normalizeNamespace(query);
    const QString key = normalizeKey(query);
    if (owner.isEmpty() || ns.isEmpty() || key.isEmpty()) {
        return error(QStringLiteral("invalid-argument"),
                     QStringLiteral("appId, namespace, and key are required."));
    }
    const QString storageKey = makeStorageKey(owner, ns, key);
    const SecretEntry entry = m_entries.value(storageKey);
    if (entry.ownerAppId.isEmpty()) {
        return error(QStringLiteral("not-found"), QStringLiteral("secret not found."));
    }

    QVariantMap out{
        {QStringLiteral("ok"), true},
        {QStringLiteral("secret"), entry.value},
        {QStringLiteral("metadata"), toMetadata(entry)},
    };
    return out;
}

QVariantMap SecretService::DeleteSecret(const QString &appId, const QVariantMap &query)
{
    const QString owner = normalizeAppId(appId);
    const QString ns = normalizeNamespace(query);
    const QString key = normalizeKey(query);
    if (owner.isEmpty() || ns.isEmpty() || key.isEmpty()) {
        return error(QStringLiteral("invalid-argument"),
                     QStringLiteral("appId, namespace, and key are required."));
    }

    const QString storageKey = makeStorageKey(owner, ns, key);
    if (!m_entries.contains(storageKey)) {
        return error(QStringLiteral("not-found"), QStringLiteral("secret not found."));
    }
    m_entries.remove(storageKey);
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("deleted"), true},
    };
}

QVariantMap SecretService::DescribeSecret(const QString &appId, const QVariantMap &query) const
{
    const QString owner = normalizeAppId(appId);
    const QString ns = normalizeNamespace(query);
    const QString key = normalizeKey(query);
    if (owner.isEmpty() || ns.isEmpty() || key.isEmpty()) {
        return error(QStringLiteral("invalid-argument"),
                     QStringLiteral("appId, namespace, and key are required."));
    }

    const QString storageKey = makeStorageKey(owner, ns, key);
    const SecretEntry entry = m_entries.value(storageKey);
    if (entry.ownerAppId.isEmpty()) {
        return error(QStringLiteral("not-found"), QStringLiteral("secret not found."));
    }
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("metadata"), toMetadata(entry)},
    };
}

QVariantMap SecretService::ClearAppSecrets(const QString &appId)
{
    const QString owner = normalizeAppId(appId);
    if (owner.isEmpty()) {
        return error(QStringLiteral("invalid-argument"), QStringLiteral("appId is required."));
    }

    int removed = 0;
    for (auto it = m_entries.begin(); it != m_entries.end();) {
        if (it.value().ownerAppId == owner) {
            it = m_entries.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("removed"), removed},
    };
}

QVariantList SecretService::ListOwnSecretMetadata(const QString &appId, const QVariantMap &options) const
{
    Q_UNUSED(options)
    const QString owner = normalizeAppId(appId);
    QVariantList rows;
    if (owner.isEmpty()) {
        return rows;
    }
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        if (it.value().ownerAppId == owner) {
            rows.push_back(toMetadata(it.value()));
        }
    }
    return rows;
}

QVariantList SecretService::ListAppIds(const QVariantMap &options) const
{
    Q_UNUSED(options)
    QSet<QString> ids;
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        const QString owner = it.value().ownerAppId.trimmed();
        if (!owner.isEmpty()) {
            ids.insert(owner);
        }
    }
    QStringList sorted = ids.values();
    std::sort(sorted.begin(), sorted.end(), [](const QString &a, const QString &b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });

    QVariantList out;
    out.reserve(sorted.size());
    for (const QString &id : sorted) {
        out.push_back(id);
    }
    return out;
}

QString SecretService::normalizeAppId(const QString &appId)
{
    return normalizeSimple(appId);
}

QString SecretService::normalizeNamespace(const QVariantMap &map)
{
    return normalizeSimple(map.value(QStringLiteral("namespace")).toString());
}

QString SecretService::normalizeKey(const QVariantMap &map)
{
    return normalizeSimple(map.value(QStringLiteral("key")).toString());
}

QString SecretService::makeStorageKey(const QString &appId, const QString &ns, const QString &key)
{
    return appId + QLatin1Char('|') + ns + QLatin1Char('|') + key;
}

qint64 SecretService::nowMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

QVariantMap SecretService::toMetadata(const SecretEntry &entry)
{
    return {
        {QStringLiteral("ownerAppId"), entry.ownerAppId},
        {QStringLiteral("namespace"), entry.namespaceName},
        {QStringLiteral("key"), entry.key},
        {QStringLiteral("label"), entry.label},
        {QStringLiteral("sensitivity"), entry.sensitivity},
        {QStringLiteral("createdAtMs"), entry.createdAtMs},
        {QStringLiteral("updatedAtMs"), entry.updatedAtMs},
        {QStringLiteral("size"), static_cast<int>(entry.value.size())},
    };
}

QVariantMap SecretService::error(const QString &code, const QString &message, const QVariantMap &extra)
{
    QVariantMap out{
        {QStringLiteral("ok"), false},
        {QStringLiteral("error"), code},
        {QStringLiteral("message"), message},
    };
    for (auto it = extra.constBegin(); it != extra.constEnd(); ++it) {
        out.insert(it.key(), it.value());
    }
    return out;
}

} // namespace Slm::Secret
