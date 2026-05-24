#include "settingsstore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

// Legacy JSON env-var — checked once for migration.
static constexpr auto kLegacyJsonEnvVar = "SLM_SETTINGSD_STORE_PATH";

SettingsStore::SettingsStore() = default;

// ── Start ─────────────────────────────────────────────────────────────────────

bool SettingsStore::start(QString *error)
{
    // Step 1 — Load system defaults (non-fatal if absent)
    m_systemDefaults.loadDirectory(QStringLiteral("/usr/share/slm/defaults"));

    // Step 2 — Load admin policy (non-fatal if absent)
    m_adminPolicy.loadFile(QStringLiteral("/etc/slm/policy.toml"));

    // Step 3 — Open the SQLite user DB
    if (!m_sqlStore.open(error))
        return false;

    // Step 4 — Migrate from legacy JSON if DB was just created (revision == 0)
    if (m_sqlStore.revision() == 0) {
        QString migErr;
        if (!migrateFromJson(&migErr)) {
            // Migration failure is non-fatal: log and continue with defaults.
            qWarning("settingsd: JSON migration failed: %s", qPrintable(migErr));
        }
    }

    return true;
}

// ── Layered Resolver ──────────────────────────────────────────────────────────

// Priority: runtime > user DB > admin policy > system defaults > built-in default.
QVariant SettingsStore::resolve(const QString &key) const
{
    if (m_runtimeOverrides.contains(key))
        return m_runtimeOverrides.value(key);

    const QVariant dbVal = m_sqlStore.get(key);
    if (dbVal.isValid())
        return dbVal;

    if (m_adminPolicy.has(key))
        return m_adminPolicy.get(key);

    if (m_systemDefaults.has(key))
        return m_systemDefaults.get(key);

    return Slm::SettingsSchema::instance().builtinDefault(key);
}

QVariantMap SettingsStore::resolveAll() const
{
    // Start from all keys registered in the schema.
    QVariantMap result;
    const QList<Slm::KeySpec> specs = Slm::SettingsSchema::instance().allSpecs();
    for (const Slm::KeySpec &s : specs)
        result.insert(s.key, resolve(s.key));

    // Overlay user-DB values that may belong to dynamic namespaces
    // (e.g. perAppOverride.*, firewall.rules.*) not in the schema registry.
    const QVariantMap dbAll = m_sqlStore.loadAll();
    for (auto it = dbAll.constBegin(); it != dbAll.constEnd(); ++it) {
        if (!result.contains(it.key()))
            result.insert(it.key(), it.value());
        else
            result.insert(it.key(), resolve(it.key())); // re-resolve with full priority
    }

    return result;
}

QVariantMap SettingsStore::resolvedNested() const
{
    return nestedFromFlat(resolveAll());
}

// ── Source tracking ───────────────────────────────────────────────────────────

static QString sourceName(const QString &key,
                          const QVariantMap &runtimeOverrides,
                          const Slm::SettingsSqlStore &sqlStore,
                          const Slm::SettingsTomlLoader &adminPolicy,
                          const Slm::SettingsTomlLoader &systemDefaults)
{
    if (runtimeOverrides.contains(key))    return QStringLiteral("runtime");
    if (sqlStore.get(key).isValid())       return QStringLiteral("user");
    if (adminPolicy.has(key))              return QStringLiteral("policy");
    if (systemDefaults.has(key))           return QStringLiteral("system");
    return QStringLiteral("default");
}

// ── Public API ────────────────────────────────────────────────────────────────

QVariantMap SettingsStore::settings() const
{
    QVariantMap result = resolvedNested();
    // Preserve the schemaVersion key that clients expect at the root level.
    result.insert(QStringLiteral("schemaVersion"), 1);

    // Ensure dynamic sub-maps always exist as empty maps if not populated.
    // These live under dynamic-namespace prefixes and are not in the schema.
    auto ensureSubMap = [&result](const QString &section, const QString &key) {
        QVariantMap sec = result.value(section).toMap();
        if (!sec.contains(key))
            sec.insert(key, QVariantMap{});
        result.insert(section, sec);
    };
    ensureSubMap(QStringLiteral("firewall"), QStringLiteral("networkProfiles"));
    ensureSubMap(QStringLiteral("firewall"), QStringLiteral("rules"));
    if (!result.contains(QStringLiteral("perAppOverride")))
        result.insert(QStringLiteral("perAppOverride"), QVariantMap{});

    return result;
}

QVariantMap SettingsStore::effectiveSetting(const QString &key) const
{
    const Slm::SettingsSchema &schema = Slm::SettingsSchema::instance();
    const QVariant value = resolve(key);
    const QString src = sourceName(key, m_runtimeOverrides, m_sqlStore,
                                   m_adminPolicy, m_systemDefaults);
    const bool isDefault = (src == QLatin1String("default"));

    QVariantMap result;
    result.insert(QStringLiteral("key"),        key);
    result.insert(QStringLiteral("value"),      value);
    result.insert(QStringLiteral("source"),     src);
    result.insert(QStringLiteral("is_default"), isDefault);
    result.insert(QStringLiteral("known"),      schema.hasKey(key));

    const Slm::KeySpec *s = schema.spec(key);
    if (s) {
        const QString typeName = [&] {
            switch (s->type) {
            case Slm::SettingsType::Bool:   return QStringLiteral("bool");
            case Slm::SettingsType::Int:    return QStringLiteral("int");
            case Slm::SettingsType::Real:   return QStringLiteral("real");
            case Slm::SettingsType::String: return QStringLiteral("string");
            }
            return QStringLiteral("string");
        }();
        result.insert(QStringLiteral("type"), typeName);
        if (!s->enumValues.isEmpty())
            result.insert(QStringLiteral("enum_values"), s->enumValues);
        if (s->hasRange) {
            result.insert(QStringLiteral("range_min"), s->rangeMin);
            result.insert(QStringLiteral("range_max"), s->rangeMax);
        }
        result.insert(QStringLiteral("user_writable"),      s->userWritable);
        result.insert(QStringLiteral("lockable_by_policy"), s->lockableByPolicy);
        result.insert(QStringLiteral("requires_restart"),   s->requiresRestart);
    }
    return result;
}

QVariantMap SettingsStore::schemaInfo(const QString &key) const
{
    const Slm::KeySpec *s = Slm::SettingsSchema::instance().spec(key);
    if (!s) return {};

    const QString typeName = [&] {
        switch (s->type) {
        case Slm::SettingsType::Bool:   return QStringLiteral("bool");
        case Slm::SettingsType::Int:    return QStringLiteral("int");
        case Slm::SettingsType::Real:   return QStringLiteral("real");
        case Slm::SettingsType::String: return QStringLiteral("string");
        }
        return QStringLiteral("string");
    }();

    QVariantMap info;
    info.insert(QStringLiteral("key"),               s->key);
    info.insert(QStringLiteral("type"),              typeName);
    info.insert(QStringLiteral("default_value"),     s->defaultValue);
    info.insert(QStringLiteral("user_writable"),     s->userWritable);
    info.insert(QStringLiteral("lockable_by_policy"),s->lockableByPolicy);
    info.insert(QStringLiteral("requires_restart"),  s->requiresRestart);
    if (!s->enumValues.isEmpty())
        info.insert(QStringLiteral("enum_values"), s->enumValues);
    if (s->hasRange) {
        info.insert(QStringLiteral("range_min"), s->rangeMin);
        info.insert(QStringLiteral("range_max"), s->rangeMax);
    }
    return info;
}

bool SettingsStore::setRuntimeOverride(const QString &key, const QVariant &value,
                                       QString *error)
{
    if (!Slm::SettingsSchema::instance().validate(key, value, error))
        return false;
    m_runtimeOverrides.insert(key, value);
    return true;
}

void SettingsStore::clearRuntimeOverride(const QString &key)
{
    m_runtimeOverrides.remove(key);
}

bool SettingsStore::setSetting(const QString &path,
                               const QVariant &value,
                               QStringList *changedPaths,
                               QString *error)
{
    if (path.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("path must not be empty");
        return false;
    }

    if (!Slm::SettingsSchema::instance().validate(path, value, error))
        return false;

    const QVariant current = resolve(path);
    if (current == value) {
        if (changedPaths) changedPaths->clear();
        return true;
    }

    if (!m_sqlStore.set(path, value, error))
        return false;

    if (changedPaths) {
        changedPaths->clear();
        changedPaths->append(path);
    }
    return true;
}

bool SettingsStore::setSettingsPatch(const QVariantMap &patch,
                                     QStringList *changedPaths,
                                     QString *error)
{
    if (changedPaths) changedPaths->clear();
    if (patch.isEmpty()) return true;

    // Validate all keys before touching the DB.
    for (auto it = patch.constBegin(); it != patch.constEnd(); ++it) {
        const QString key = it.key().trimmed();
        if (key.isEmpty()) continue;
        if (!Slm::SettingsSchema::instance().validate(key, it.value(), error))
            return false;
    }

    // Build the subset that actually changes.
    QVariantMap toWrite;
    for (auto it = patch.constBegin(); it != patch.constEnd(); ++it) {
        const QString key = it.key().trimmed();
        if (key.isEmpty()) continue;
        if (resolve(key) != it.value())
            toWrite.insert(key, it.value());
    }

    if (toWrite.isEmpty()) return true;

    QStringList changed;
    if (!m_sqlStore.setMany(toWrite, &changed, error))
        return false;

    if (changedPaths) *changedPaths = changed;
    return true;
}

// ── Migration from legacy JSON ────────────────────────────────────────────────

bool SettingsStore::migrateFromJson(QString *error)
{
    // Honour legacy env-var override first; fall back to default path.
    QString jsonPath = qEnvironmentVariable(kLegacyJsonEnvVar).trimmed();
    if (jsonPath.isEmpty()) {
        const QString base = QDir::homePath()
                             + QStringLiteral("/.config/slm-desktop/settings");
        jsonPath = base + QStringLiteral("/settings.json");
    }

    QFile f(jsonPath);
    if (!f.exists())
        return true; // Nothing to migrate.
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("cannot open legacy settings: %1").arg(f.errorString());
        return false;
    }

    bool ok = false;
    const QVariantMap raw = parseJsonObject(f.readAll(), &ok);
    if (!ok) {
        if (error) *error = QStringLiteral("legacy settings JSON is malformed");
        return false;
    }

    // Flatten nested map → dotted keys, skip the internal schemaVersion key.
    QVariantMap flat = flattenNested(raw);
    flat.remove(QStringLiteral("schemaVersion"));

    if (!m_sqlStore.importFlat(flat, error))
        return false;

    qInfo("settingsd: migrated %lld settings from legacy JSON",
          static_cast<long long>(flat.size()));
    return true;
}

// ── Utilities ─────────────────────────────────────────────────────────────────

QVariant SettingsStore::valueByPath(const QVariantMap &root,
                                    const QString &path, bool *ok)
{
    if (ok) *ok = false;
    const QStringList segs = path.split(u'.', Qt::SkipEmptyParts);
    if (segs.isEmpty()) return {};

    QVariant cur = root;
    for (int i = 0; i < segs.size(); ++i) {
        const QVariantMap map = cur.toMap();
        if (!map.contains(segs.at(i))) return {};
        cur = map.value(segs.at(i));
        if (i < segs.size() - 1 && !cur.canConvert<QVariantMap>()) return {};
    }
    if (ok) *ok = true;
    return cur;
}

QVariantMap SettingsStore::flattenNested(const QVariantMap &nested,
                                          const QString &prefix)
{
    QVariantMap result;
    for (auto it = nested.constBegin(); it != nested.constEnd(); ++it) {
        const QString key = prefix.isEmpty() ? it.key()
                                             : prefix + u'.' + it.key();
        if (it.value().canConvert<QVariantMap>()) {
            const QVariantMap sub = it.value().toMap();
            if (sub.isEmpty()) {
                // Keep empty maps as-is rather than losing them.
                result.insert(key, it.value());
            } else {
                result.insert(flattenNested(sub, key));
            }
        } else {
            result.insert(key, it.value());
        }
    }
    return result;
}

static void setNestedValue(QVariantMap &node,
                           const QStringList &segs, int idx,
                           const QVariant &value)
{
    const QString &seg = segs.at(idx);
    if (idx == segs.size() - 1) {
        node.insert(seg, value);
        return;
    }
    QVariantMap child = node.value(seg).toMap();
    setNestedValue(child, segs, idx + 1, value);
    node.insert(seg, child);
}

QVariantMap SettingsStore::nestedFromFlat(const QVariantMap &flat)
{
    QVariantMap root;
    for (auto it = flat.constBegin(); it != flat.constEnd(); ++it) {
        const QStringList segs = it.key().split(u'.', Qt::SkipEmptyParts);
        if (segs.isEmpty()) continue;
        setNestedValue(root, segs, 0, it.value());
    }
    return root;
}

QVariantMap SettingsStore::parseJsonObject(const QByteArray &json, bool *ok)
{
    if (ok) *ok = false;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return {};
    if (ok) *ok = true;
    return doc.object().toVariantMap();
}
