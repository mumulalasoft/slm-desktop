#include "envservice.h"

#include "../../apps/settings/modules/developer/envvalidator.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>

static constexpr char kHelperService[]   = "org.slm.EnvironmentHelper1";
static constexpr char kHelperPath[]      = "/org/slm/EnvironmentHelper";
static constexpr char kHelperInterface[] = "org.slm.EnvironmentHelper1";
static constexpr int  kHelperTimeoutMs   = 35000; // must cover pkcheck dialog wait

EnvService::EnvService(QObject *parent)
    : QObject(parent)
    , m_userStore(this)
    , m_sessionStore(this)
    , m_perAppStore(this)
    , m_systemStore(this)
{
    connect(&m_userStore, &EnvStore::entriesChanged, this, [this] {
        emit userVarsChanged();
    });
    connect(&m_sessionStore, &SessionEnvStore::entriesChanged, this, [this] {
        emit sessionVarsChanged();
    });
    connect(&m_perAppStore, &PerAppEnvStore::appEntriesChanged,
            this, &EnvService::appVarsChanged);
    connect(&m_systemStore, &SystemEnvStore::entriesChanged, this, [this] {
        emit systemVarsChanged();
    });
}

bool EnvService::start()
{
    m_systemStore.load(); // best-effort: missing file is not fatal
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

// ── Per-app discovery ─────────────────────────────────────────────────────────

QVariantList EnvService::appVars(const QString &appId) const
{
    QVariantList result;
    for (const EnvEntry &e : m_perAppStore.entries(appId)) {
        QVariantMap m;
        m[QStringLiteral("key")]        = e.key;
        m[QStringLiteral("value")]      = e.value;
        m[QStringLiteral("enabled")]    = e.enabled;
        m[QStringLiteral("mergeMode")]  = e.mergeMode;
        m[QStringLiteral("modifiedAt")] = e.modifiedAt.toString(Qt::ISODate);
        result.append(m);
    }
    return result;
}

QStringList EnvService::appsWithOverrides() const
{
    // PerAppEnvStore tracks which app IDs have been loaded/modified.
    // Scan the on-disk directory for app JSON files.
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                        + QStringLiteral("/slm/environment.d/apps");
    QStringList apps;
    const QDir d(dir);
    if (d.exists()) {
        const QStringList files = d.entryList(QStringList{QStringLiteral("*.json")},
                                              QDir::Files | QDir::Readable);
        for (const QString &f : files) {
            QString id = f;
            if (id.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive))
                id.chop(5);
            if (!id.isEmpty())
                apps.append(id);
        }
    }
    return apps;
}

// ── System scope ─────────────────────────────────────────────────────────────

bool EnvService::writeSystemVar(const QString &key, const QString &value,
                                 const QString &comment, const QString &mergeMode,
                                 bool enabled)
{
    m_lastError.clear();

    const auto kr = EnvValidator::validateKey(key);
    if (!kr.valid) {
        m_lastError = kr.message;
        return false;
    }

    QDBusInterface helper(QLatin1String(kHelperService),
                          QLatin1String(kHelperPath),
                          QLatin1String(kHelperInterface),
                          QDBusConnection::systemBus());
    if (!helper.isValid()) {
        m_lastError = QStringLiteral("slm-envd-helper not available on system bus");
        return false;
    }

    QDBusReply<QVariantMap> reply = helper.callWithArgumentList(
        QDBus::Block,
        QStringLiteral("WriteSystemEntry"),
        { key, value, comment,
          mergeMode.isEmpty() ? QStringLiteral("replace") : mergeMode,
          enabled });

    if (!reply.isValid()) {
        m_lastError = reply.error().message();
        return false;
    }
    const QVariantMap r = reply.value();
    if (!r.value(QStringLiteral("ok")).toBool()) {
        m_lastError = r.value(QStringLiteral("error")).toString();
        return false;
    }

    m_systemStore.load();
    return true;
}

bool EnvService::deleteSystemVar(const QString &key)
{
    m_lastError.clear();

    QDBusInterface helper(QLatin1String(kHelperService),
                          QLatin1String(kHelperPath),
                          QLatin1String(kHelperInterface),
                          QDBusConnection::systemBus());
    if (!helper.isValid()) {
        m_lastError = QStringLiteral("slm-envd-helper not available on system bus");
        return false;
    }

    QDBusReply<QVariantMap> reply = helper.call(
        QStringLiteral("DeleteSystemEntry"), key);

    if (!reply.isValid()) {
        m_lastError = reply.error().message();
        return false;
    }
    const QVariantMap r = reply.value();
    if (!r.value(QStringLiteral("ok")).toBool()) {
        m_lastError = r.value(QStringLiteral("error")).toString();
        return false;
    }

    m_systemStore.load();
    return true;
}

QVariantList EnvService::systemVars() const
{
    QVariantList result;
    for (const EnvEntry &e : m_systemStore.entries()) {
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

// ── Resolver ─────────────────────────────────────────────────────────────────

QVariantMap EnvService::resolveEnv(const QString &appId) const
{
    QList<MergeResolver::Layer> layers;
    layers.append({EnvLayer::System,         m_systemStore.entries()});
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
    layers.append({EnvLayer::System,         m_systemStore.entries()});
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
