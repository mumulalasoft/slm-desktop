#include "envservice.h"

#include "../../apps/settings/modules/developer/envvalidator.h"

#include <QDateTime>

EnvService::EnvService(QObject *parent)
    : QObject(parent)
    , m_userStore(this)
    , m_sessionStore(this)
    , m_perAppStore(this)
{
    connect(&m_userStore, &EnvStore::entriesChanged, this, [this] {
        emit userVarsChanged();
    });
    connect(&m_sessionStore, &SessionEnvStore::entriesChanged, this, [this] {
        emit sessionVarsChanged();
    });
    connect(&m_perAppStore, &PerAppEnvStore::appEntriesChanged,
            this, &EnvService::appVarsChanged);
}

bool EnvService::start()
{
    return m_userStore.load();
}

// ── User-persistent ───────────────────────────────────────────────────────────

bool EnvService::addUserVar(const QString &key, const QString &value,
                             const QString &comment, const QString &mergeMode)
{
    m_lastError.clear();

    const auto kr = EnvValidator::validateKey(key);
    if (!kr.valid) {
        m_lastError = kr.message;
        return false;
    }
    const auto vr = EnvValidator::validateValue(key, value);
    if (!vr.valid) {
        m_lastError = vr.message;
        return false;
    }

    EnvEntry e;
    e.key       = key;
    e.value     = value;
    e.comment   = comment;
    e.mergeMode = mergeMode.isEmpty() ? QStringLiteral("replace") : mergeMode;
    e.scope     = QStringLiteral("user");

    if (!m_userStore.addEntry(e)) {
        m_lastError = m_userStore.lastError();
        return false;
    }
    return true;
}

bool EnvService::updateUserVar(const QString &key, const QString &value,
                                const QString &comment, const QString &mergeMode)
{
    m_lastError.clear();

    const auto vr = EnvValidator::validateValue(key, value);
    if (!vr.valid) {
        m_lastError = vr.message;
        return false;
    }

    EnvEntry updated;
    updated.key       = key;
    updated.value     = value;
    updated.comment   = comment;
    updated.mergeMode = mergeMode.isEmpty() ? QStringLiteral("replace") : mergeMode;

    if (!m_userStore.updateEntry(key, updated)) {
        m_lastError = m_userStore.lastError();
        return false;
    }
    return true;
}

bool EnvService::deleteUserVar(const QString &key)
{
    m_lastError.clear();
    if (!m_userStore.removeEntry(key)) {
        m_lastError = m_userStore.lastError();
        return false;
    }
    return true;
}

bool EnvService::setUserVarEnabled(const QString &key, bool enabled)
{
    m_lastError.clear();
    if (!m_userStore.setEnabled(key, enabled)) {
        m_lastError = m_userStore.lastError();
        return false;
    }
    return true;
}

QVariantList EnvService::userVars() const
{
    QVariantList result;
    for (const EnvEntry &e : m_userStore.entries()) {
        QVariantMap m;
        m[QStringLiteral("key")]        = e.key;
        m[QStringLiteral("value")]      = e.value;
        m[QStringLiteral("enabled")]    = e.enabled;
        m[QStringLiteral("comment")]    = e.comment;
        m[QStringLiteral("mergeMode")]  = e.mergeMode;
        m[QStringLiteral("modifiedAt")] = e.modifiedAt.toString(Qt::ISODate);
        m[QStringLiteral("severity")]   = EnvValidator::validateKey(e.key).severity;
        result.append(m);
    }
    return result;
}

// ── Session ───────────────────────────────────────────────────────────────────

bool EnvService::setSessionVar(const QString &key, const QString &value,
                                const QString &mergeMode)
{
    m_lastError.clear();
    const auto kr = EnvValidator::validateKey(key);
    if (!kr.valid) {
        m_lastError = kr.message;
        return false;
    }
    return m_sessionStore.setVar(key, value, mergeMode);
}

bool EnvService::unsetSessionVar(const QString &key)
{
    return m_sessionStore.unsetVar(key);
}

// ── Per-app ───────────────────────────────────────────────────────────────────

bool EnvService::addAppVar(const QString &appId, const QString &key,
                            const QString &value, const QString &mergeMode)
{
    m_lastError.clear();
    const auto kr = EnvValidator::validateKey(key);
    if (!kr.valid) {
        m_lastError = kr.message;
        return false;
    }
    m_perAppStore.load(appId);
    EnvEntry e;
    e.key       = key;
    e.value     = value;
    e.mergeMode = mergeMode.isEmpty() ? QStringLiteral("replace") : mergeMode;
    e.scope     = QStringLiteral("per-app");
    if (!m_perAppStore.addEntry(appId, e)) {
        m_lastError = m_perAppStore.lastError();
        return false;
    }
    return true;
}

bool EnvService::removeAppVar(const QString &appId, const QString &key)
{
    m_lastError.clear();
    if (!m_perAppStore.removeEntry(appId, key)) {
        m_lastError = m_perAppStore.lastError();
        return false;
    }
    return true;
}

// ── Resolver ─────────────────────────────────────────────────────────────────

QVariantMap EnvService::resolveEnv(const QString &appId) const
{
    QList<MergeResolver::Layer> layers;
    layers.append({EnvLayer::UserPersistent, m_userStore.entries()});
    layers.append({EnvLayer::Session,        m_sessionStore.entries()});
    if (!appId.isEmpty())
        layers.append({EnvLayer::PerApp, m_perAppStore.entries(appId)});

    const ResolvedEnv resolved = MergeResolver::resolve(layers);

    QVariantMap result;
    for (auto it = resolved.cbegin(); it != resolved.cend(); ++it) {
        QVariantMap entry;
        entry[QStringLiteral("value")]  = it.value().value;
        entry[QStringLiteral("layer")]  = static_cast<int>(it.value().sourceLayer);
        result.insert(it.key(), entry);
    }
    return result;
}

QStringList EnvService::resolveEnvList(const QString &appId) const
{
    QList<MergeResolver::Layer> layers;
    layers.append({EnvLayer::UserPersistent, m_userStore.entries()});
    layers.append({EnvLayer::Session,        m_sessionStore.entries()});
    if (!appId.isEmpty())
        layers.append({EnvLayer::PerApp, m_perAppStore.entries(appId)});

    const ResolvedEnv resolved = MergeResolver::resolve(layers);

    QStringList result;
    result.reserve(resolved.size());
    for (auto it = resolved.cbegin(); it != resolved.cend(); ++it)
        result.append(it.key() + QLatin1Char('=') + it.value().value);
    return result;
}

QString EnvService::lastError() const
{
    return m_lastError;
}
