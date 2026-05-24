#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

// FsdRecoveryTrash manages the system-level recovery trash for safe deletes.
//
// Layout:
//   /var/lib/slm/recovery-trash/<uid>/<YYYYMMDDTHHMMSS>-<uuid>/
//     metadata.json  — original path, op, token_id, snapshot_name, timestamp
//     content        — the actual moved file/directory tree
//
// Permanent purge is a separate polkit-checked operation (RequestPurge).
//
// Phase 0: all methods are stubs.
class FsdRecoveryTrash : public QObject
{
    Q_OBJECT

public:
    explicit FsdRecoveryTrash(QObject *parent = nullptr);

    // Move sourcePath into the recovery trash for uid.
    // tokenId and snapshotName are stored in metadata.json.
    // Returns the trash entry path on success, or "" on failure.
    QString moveToTrash(const QString &sourcePath,
                        uint uid,
                        const QString &tokenId,
                        const QString &snapshotName);

    // Permanently delete a trash entry (recursive).
    // Returns "" on success, or an error string on failure.
    QString purgeEntry(const QString &trashEntryPath);

    // Returns the trash root for a given uid.
    static QString trashRootForUid(uint uid);

    // Returns the global trash root.
    static QString trashRoot();
};
