#pragma once

#include "../../apps/settings/modules/developer/enventry.h"

#include <QFileSystemWatcher>
#include <QList>
#include <QObject>
#include <QString>

// SystemEnvStore — read-only (from user session) system-scope store.
//
// Reads /etc/slm/environment.d/system.json which is maintained exclusively
// by slm-envd-helper (privileged, polkit-gated).  The session daemon uses
// this store to include the System layer in MergeResolver; all writes are
// forwarded to the helper via the system D-Bus.
//
// A QFileSystemWatcher picks up external updates (e.g., another admin session
// writing the file) so the resolver stays current without a restart.
//
class SystemEnvStore : public QObject
{
    Q_OBJECT

public:
    explicit SystemEnvStore(QObject *parent = nullptr);

    // Loads (or re-loads) the file.  Returns true even if the file does not
    // exist — that simply means zero entries.
    bool load();

    QList<EnvEntry> entries() const;
    QString         lastError() const;

signals:
    void entriesChanged();

private slots:
    void onFileChanged(const QString &path);

private:
    void watchFile();

    QList<EnvEntry>    m_entries;
    QFileSystemWatcher m_watcher;
    QString            m_lastError;

    static QString filePath();
};
