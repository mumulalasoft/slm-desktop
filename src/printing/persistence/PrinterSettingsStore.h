#pragma once

#include <QObject>
#include <QVariantMap>

namespace Slm::Print {

// Persists per-printer settings using QSettings.
// Keys are stored under group "print/<sanitized-printerId>/".
// All methods are synchronous and safe to call on the main thread.
class PrinterSettingsStore : public QObject
{
    Q_OBJECT

public:
    explicit PrinterSettingsStore(QObject *parent = nullptr);

    // Loads previously saved settings for printerId.
    // Returns an empty map if no settings have been saved yet.
    QVariantMap load(const QString &printerId) const;

    // Saves settings for printerId. Overwrites any existing entry.
    // settings must be a map in the format produced by PrintSettingsModel::serialize().
    void save(const QString &printerId, const QVariantMap &settings);

    // Removes all stored settings for printerId.
    void clear(const QString &printerId);

    // Returns true if settings exist for printerId.
    bool has(const QString &printerId) const;

private:
    // QSettings group key for a printer ID. Replaces characters not safe
    // for use as QSettings group names.
    static QString groupKey(const QString &printerId);
};

} // namespace Slm::Print
