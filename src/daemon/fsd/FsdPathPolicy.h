#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantMap>

// FsdPathPolicy handles:
//   1. Protected-path detection (is this path under a protected prefix?)
//   2. Path validation (canonicalization, UTF-8, size limits, ".." rejection)
//   3. Config loading from /etc/slm/fsd/protected-paths.d/
//
// All methods are synchronous and safe to call on the D-Bus dispatch thread.
class FsdPathPolicy : public QObject
{
    Q_OBJECT

public:
    explicit FsdPathPolicy(QObject *parent = nullptr);

    // Load protected prefixes from /etc/slm/fsd/protected-paths.d/*.conf.
    // Falls back to built-in defaults if no config dir exists.
    void loadConfig();

    // Returns true if path falls under a known protected prefix.
    // Sets *scope to the scope name (e.g. "system", "etc", "boot").
    bool isProtected(const QString &path, QString *scope = nullptr) const;

    // Validate and canonicalize a raw path from a D-Bus caller.
    // Returns the canonical path, or an empty string on failure.
    // Sets *errorCode to a machine-readable reason on failure.
    QString validateAndCanonicalize(const QString &rawPath, QString *errorCode = nullptr) const;

    // Returns all currently loaded protected prefixes (for debugging/logging).
    QStringList protectedPrefixes() const;

private:
    struct ProtectedEntry {
        QString prefix; // canonicalized absolute path
        QString scope;  // human label
    };

    void loadBuiltinDefaults();
    void loadFromDir(const QString &dirPath);
    QString scopeForPrefix(const QString &prefix) const;

    QList<ProtectedEntry> m_entries;
};
