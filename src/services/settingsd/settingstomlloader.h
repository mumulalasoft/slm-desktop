#pragma once

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

namespace Slm {

// Minimal TOML loader — handles sections + scalar key-value pairs only.
// Sufficient for system defaults files.  No third-party library required.
class SettingsTomlLoader
{
public:
    SettingsTomlLoader() = default;

    // Load a single TOML file.  Returns false only on hard I/O errors;
    // parse errors emit a warning and skip the offending line.
    bool loadFile(const QString &path, QString *error = nullptr);

    // Load all *.toml files from a directory (non-recursive, sorted).
    void loadDirectory(const QString &dirPath);

    bool     has(const QString &key) const;
    QVariant get(const QString &key) const;

    // Returns flat dotted-key map of all loaded values.
    const QVariantMap &all() const;

private:
    static QVariantMap parseToml(const QString &text, const QString &sourcePath);

    QVariantMap m_values;
};

} // namespace Slm
