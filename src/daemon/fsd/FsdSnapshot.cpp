#include "FsdSnapshot.h"

#include <QDateTime>
#include <QDebug>
#include <QUuid>

// Phase 2 will add:
//   #include <sys/statfs.h>
//   #include <linux/magic.h>    // BTRFS_SUPER_MAGIC
//   #include <QProcess>         // to call "btrfs subvolume snapshot -r"

FsdSnapshot::FsdSnapshot(QObject *parent)
    : QObject(parent)
{
}

QString FsdSnapshot::createSnapshot(const QString &sourcePath, const QString &op)
{
    // Phase 0 stub — no snapshot taken yet.
    // TODO(Phase2): check isBtrfs(sourcePath), find subvolume root,
    //   run: btrfs subvolume snapshot -r <subvol> <snapshotRoot()>/<name>
    //   emit snapshotCreated() on success or snapshotFailed() on error.

    Q_UNUSED(sourcePath)
    Q_UNUSED(op)

    qInfo().noquote() << "[slm-fsd][snapshot] STUB — no snapshot taken for"
                      << sourcePath << "op=" << op;
    return {};
}

bool FsdSnapshot::isBtrfs(const QString &path)
{
    // Phase 2: use statfs(path.toLocal8Bit(), &buf) and check buf.f_type == 0x9123683eUL
    Q_UNUSED(path)
    return false;
}

QString FsdSnapshot::snapshotRoot()
{
    return QStringLiteral("/var/lib/slm/snapshots/fsd");
}
