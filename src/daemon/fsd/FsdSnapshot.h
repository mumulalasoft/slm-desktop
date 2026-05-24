#pragma once

#include <QObject>
#include <QString>

// FsdSnapshot creates read-only BTRFS snapshots before destructive operations
// on protected paths.
//
// Snapshot naming convention:
//   slm-fsd-<op>-<YYYYMMDDTHHMMSS>-<uuid-short>
//
// Snapshots are stored under:
//   /var/lib/slm/snapshots/fsd/
//
// If the path is not on a BTRFS subvolume, snapshot() returns an empty string
// and the operation proceeds without a snapshot.
//
// Phase 0: all methods are stubs that return empty strings.
class FsdSnapshot : public QObject
{
    Q_OBJECT

public:
    explicit FsdSnapshot(QObject *parent = nullptr);

    // Attempt to create a snapshot of the subvolume containing sourcePath.
    // op: short operation name e.g. "delete", "rename"
    // Returns the snapshot name on success, or "" if not applicable / failed.
    QString createSnapshot(const QString &sourcePath, const QString &op);

    // Returns true if sourcePath is on a BTRFS filesystem.
    static bool isBtrfs(const QString &path);

    // Returns the snapshot root directory.
    static QString snapshotRoot();

signals:
    void snapshotCreated(const QString &snapshotName, const QString &sourcePath);
    void snapshotFailed(const QString &sourcePath, const QString &reason);
};
