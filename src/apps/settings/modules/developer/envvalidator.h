#pragma once

#include <QString>
#include <QStringList>

struct ValidationResult {
    bool    valid    = true;
    QString severity; // "none" | "medium" | "high" | "critical"
    QString message;
};

// EnvValidator — stateless validation rules for environment variable keys and values.
// All methods are static; no instances required.
class EnvValidator
{
public:
    // Validate key format (A–Z, 0–9, _ only; must not start with a digit).
    // Returns a warning-level result for known-sensitive keys even when valid=true.
    static ValidationResult validateKey(const QString &key);

    // Validate value for a given key (e.g. blocks empty PATH).
    static ValidationResult validateValue(const QString &key, const QString &value);

    // Returns true if modifying `key` requires a warning dialog.
    static bool isSensitiveKey(const QString &key);

    // Human-readable description of why a key is sensitive.
    static QString sensitiveKeyDescription(const QString &key);

private:
    static const QStringList &criticalKeys();
    static const QStringList &highKeys();
    static const QStringList &mediumKeys();
};
