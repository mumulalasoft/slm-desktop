#pragma once

#include "enventry.h"

#include <QFileSystemWatcher>
#include <QList>
#include <QObject>
#include <QString>

// LocalEnvStore — reads and writes user-persistent environment variable entries
// from ~/.config/slm/environment.d/user.json.
//
// File changes from external tools are detected via QFileSystemWatcher and
// re-trigger a load, keeping the in-memory state consistent.
class LocalEnvStore : public QObject
{
    Q_OBJECT
public:
    explicit LocalEnvStore(QObject *parent = nullptr);

    // Load from disk. Safe to call multiple times; clears and reloads.
    // Returns true on success; false if the file exists but cannot be parsed.
    bool load();

    // Persist current entries to disk. Returns true on success.
    bool save();

    QList<EnvEntry> entries() const;

    // Returns false if a key with the same name already exists.
    bool addEntry(const EnvEntry &entry);

    // Updates value, comment, and mergeMode for the entry matching `key`.
    bool updateEntry(const QString &key, const EnvEntry &updated);

    // Removes the entry with the given key. Returns false if not found.
    bool removeEntry(const QString &key);

    // Toggles the enabled flag for the given key.
    bool setEnabled(const QString &key, bool enabled);

signals:
    void entriesChanged();

private slots:
    void onFileChanged(const QString &path);

private:
    QString    filePath() const;
    void       ensureDir() const;
    void       watchFile();

    QList<EnvEntry>     m_entries;
    QFileSystemWatcher  m_watcher;
    bool                m_saving = false;
};
