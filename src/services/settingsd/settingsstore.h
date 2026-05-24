#pragma once

#include "settingsschema.h"
#include "settingssqlstore.h"
#include "settingstomlloader.h"

#include <QVariant>
#include <QVariantMap>
#include <QStringList>

class SettingsStore
{
public:
    SettingsStore();

    bool start(QString *error = nullptr);
    QVariantMap settings() const;

    bool setSetting(const QString &path, const QVariant &value,
                    QStringList *changedPaths, QString *error);
    bool setSettingsPatch(const QVariantMap &patch,
                          QStringList *changedPaths, QString *error);

    // Returns {value, source, is_default, schema_type, enum_values, range_min, range_max}
    QVariantMap effectiveSetting(const QString &key) const;

    // Returns the full schema spec for one key, or empty map if unknown.
    QVariantMap schemaInfo(const QString &key) const;

    // In-memory runtime overrides — highest priority, cleared on daemon restart.
    bool setRuntimeOverride(const QString &key, const QVariant &value, QString *error = nullptr);
    void clearRuntimeOverride(const QString &key);

    static QVariant valueByPath(const QVariantMap &root, const QString &path, bool *ok);

private:
    // Layered resolver: runtime > user DB > admin policy TOML > system defaults TOML > built-in
    QVariant resolve(const QString &key) const;
    QVariantMap resolveAll() const;        // flat dotted-key map
    QVariantMap resolvedNested() const;    // nested map (for settings() return value)

    // Flat ↔ nested conversion helpers
    static QVariantMap nestedFromFlat(const QVariantMap &flat);
    static QVariantMap flattenNested(const QVariantMap &nested,
                                     const QString &prefix = {});

    // JSON migration (reads old settings.json and imports into SQLite)
    bool migrateFromJson(QString *error);
    static QVariantMap parseJsonObject(const QByteArray &json, bool *ok);

    // In-memory runtime overrides — cleared on restart
    QVariantMap m_runtimeOverrides;

    Slm::SettingsTomlLoader m_systemDefaults;
    Slm::SettingsTomlLoader m_adminPolicy;
    Slm::SettingsSqlStore   m_sqlStore;
};
