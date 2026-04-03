#pragma once

#include "../../apps/settings/modules/developer/enventry.h"

#include <QList>
#include <QObject>
#include <QString>

// HelperService — pure business logic for slm-envd-helper.
//
// Reads and writes /etc/slm/environment.d/system.json.
// Does NOT know about D-Bus or polkit; those concerns belong to
// HelperDbusInterface.
//
class HelperService : public QObject
{
    Q_OBJECT

public:
    explicit HelperService(QObject *parent = nullptr);

    // Write (add or update) a system-scope entry.
    bool writeEntry(const QString &key, const QString &value,
                    const QString &comment, const QString &mergeMode,
                    bool enabled);

    // Delete a system-scope entry by key.  Returns false if the key is not
    // found (not an error for the polkit path — just a no-op).
    bool deleteEntry(const QString &key);

    // Read all current system-scope entries.
    QList<EnvEntry> entries() const;

    QString lastError() const;

private:
    bool load();
    bool save();

    QList<EnvEntry> m_entries;
    QString         m_lastError;

    static QString filePath();
};
