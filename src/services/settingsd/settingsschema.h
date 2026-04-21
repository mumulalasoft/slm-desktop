#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace Slm {

enum class SettingsType { Bool, Int, Real, String };

struct KeySpec {
    QString      key;
    SettingsType type;
    QVariant     defaultValue;
    QStringList  enumValues;
    bool         hasRange         = false;
    double       rangeMin         = 0.0;
    double       rangeMax         = 0.0;
    bool         userWritable     = true;
    bool         lockableByPolicy = false;
    bool         requiresRestart  = false;
};

class SettingsSchema
{
public:
    static const SettingsSchema &instance();

    bool           hasKey(const QString &key) const;
    const KeySpec *spec(const QString &key) const;
    bool           validate(const QString &key, const QVariant &value,
                            QString *error = nullptr) const;
    QVariant       builtinDefault(const QString &key) const;
    QList<KeySpec> allSpecs() const;

    // Returns true for keys that begin with a registered dynamic prefix.
    // Dynamic-prefix keys bypass unknown-key rejection but still pass type checks
    // if a matching spec exists.
    bool isDynamicPrefix(const QString &key) const;

private:
    SettingsSchema();
    void reg(KeySpec spec);
    void regDynamic(const QString &prefix);

    QHash<QString, KeySpec> m_specs;
    QStringList             m_dynamicPrefixes;
};

} // namespace Slm
