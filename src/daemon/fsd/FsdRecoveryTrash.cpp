#include "FsdRecoveryTrash.h"

#include <QDebug>

// Phase 1 will add:
//   #include <QDir>
//   #include <QFile>
//   #include <QJsonDocument>
//   #include <QJsonObject>
//   #include <filesystem>    // std::filesystem::remove_all for purge

FsdRecoveryTrash::FsdRecoveryTrash(QObject *parent)
    : QObject(parent)
{
}

QString FsdRecoveryTrash::moveToTrash(const QString &sourcePath,
                                       uint uid,
                                       const QString &tokenId,
                                       const QString &snapshotName)
{
    // Phase 0 stub — no actual move.
    // TODO(Phase1):
    //   1. Build entry path: trashRootForUid(uid)/<timestamp>-<uuid>/
    //   2. QDir().mkpath(entryPath + "/")
    //   3. QFile::rename(sourcePath, entryPath + "/content")
    //   4. Write metadata.json with QJsonDocument
    //   5. Return entryPath on success

    Q_UNUSED(sourcePath)
    Q_UNUSED(uid)
    Q_UNUSED(tokenId)
    Q_UNUSED(snapshotName)

    qInfo().noquote() << "[slm-fsd][trash] STUB — move to trash not implemented for"
                      << sourcePath;
    return {};
}

QString FsdRecoveryTrash::purgeEntry(const QString &trashEntryPath)
{
    // Phase 0 stub — no actual purge.
    // TODO(Phase1): std::filesystem::remove_all(trashEntryPath.toStdString())
    Q_UNUSED(trashEntryPath)
    qInfo().noquote() << "[slm-fsd][trash] STUB — purge not implemented for"
                      << trashEntryPath;
    return QStringLiteral("NotImplemented");
}

QString FsdRecoveryTrash::trashRootForUid(uint uid)
{
    return trashRoot() + QStringLiteral("/") + QString::number(uid);
}

QString FsdRecoveryTrash::trashRoot()
{
    return QStringLiteral("/var/lib/slm/recovery-trash");
}
