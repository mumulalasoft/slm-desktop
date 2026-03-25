#pragma once

#include "../../apps/settings/modules/developer/enventry.h"

#include <QFileSystemWatcher>
#include <QList>
#include <QObject>
#include <QString>

// EnvStore — service-side user-persistent store.
//
// Reads and writes the same JSON file as LocalEnvStore (Phase 1):
//   ~/.config/slm/environment.d/user.json
//
// The service owns the file at runtime; the settings app communicates through
// D-Bus rather than writing the file directly once the service is active.
// A QFileSystemWatcher is used to pick up external writes (e.g., from a text
// editor or migration scripts).
//
class EnvStore : public QObject
{
    Q_OBJECT

public:
    explicit EnvStore(QObject *parent = nullptr);

    bool load();
    bool save();

    QList<EnvEntry> entries() const;

    bool addEntry(const EnvEntry &entry);
    bool updateEntry(const QString &key, const EnvEntry &updated);
    bool removeEntry(const QString &key);
    bool setEnabled(const QString &key, bool enabled);

    QString lastError() const;

signals:
    void entriesChanged();

private slots:
    void onFileChanged(const QString &path);

private:
    void watchFile();

    QList<EnvEntry>   m_entries;
    QFileSystemWatcher m_watcher;
    bool              m_saving  = false;
    QString           m_lastError;

    static QString filePath();
};
