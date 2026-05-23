#include "SlmSnapshotJob.h"

#include "SlmCommand.h"

#include <GlobalStorage.h>
#include <JobQueue.h>
#include <utils/Logger.h>

#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QVariantMap>

CALAMARES_PLUGIN_FACTORY_DEFINITION(SlmSnapshotJobFactory, registerPlugin<SlmSnapshotJob>();)

namespace {

// §7.1 — read-only snapshot of the deployed @ at the Btrfs top level. The
// scratch mount uses subvolid=5 to reach the top-level subvolume regardless
// of which subvol is bound at /.
constexpr const char *kSnapshotName = "@factory-initial";
constexpr const char *kTopLevelScratch = "/mnt/slm-btrfs-root";

QString resolveRootMountPoint(Calamares::GlobalStorage *gs)
{
    const QString fromGs = gs->value(QStringLiteral("rootMountPoint")).toString();
    return fromGs.isEmpty() ? QStringLiteral("/mnt/slm-install-root") : fromGs;
}

} // namespace

SlmSnapshotJob::SlmSnapshotJob(QObject *parent)
    : Calamares::CppJob(parent)
{
}

SlmSnapshotJob::~SlmSnapshotJob() = default;

QString SlmSnapshotJob::prettyName() const
{
    return tr("Create factory snapshot");
}

Calamares::JobResult SlmSnapshotJob::exec()
{
    auto *gs = Calamares::JobQueue::instanceGlobalStorage();
    if (!gs) {
        return Calamares::JobResult::error(
            tr("Installer state unavailable."),
            QStringLiteral("SNAP_001: GlobalStorage missing"));
    }

    const QString rootDevice = gs->value(QStringLiteral("slm.target.root_device")).toString();
    if (rootDevice.isEmpty()) {
        return Calamares::JobResult::error(
            tr("Root device not yet known."),
            QStringLiteral("SNAP_001: slm.target.root_device not set"));
    }

    const QString rootMount = resolveRootMountPoint(gs);
    const QString snapshotPath = QString::fromLatin1(kTopLevelScratch)
                                 + QLatin1Char('/') + QLatin1String(kSnapshotName);

    gs->insert(QStringLiteral("slm.snapshot.name"), QLatin1String(kSnapshotName));
    gs->insert(QStringLiteral("slm.snapshot.path"), snapshotPath);

    cDebug() << "slm-snapshot:"
             << "root_device=" << rootDevice
             << "root_mount=" << rootMount
             << "snapshot=" << snapshotPath
             << "dry_run=" << m_dryRun;

    if (m_dryRun) {
        cDebug() << "slm-snapshot: DRY RUN — would run:";
        cDebug() << "  mkdir -p" << kTopLevelScratch;
        cDebug() << "  mount -o subvolid=5" << rootDevice << kTopLevelScratch;
        cDebug() << "  btrfs subvolume snapshot -r" << rootMount << snapshotPath;
        cDebug() << "  umount" << kTopLevelScratch;
        gs->insert(QStringLiteral("slm.snapshot.dry_run"), true);
        return Calamares::JobResult::ok();
    }

    if (!QDir().mkpath(QString::fromLatin1(kTopLevelScratch))) {
        return Calamares::JobResult::error(
            tr("Unable to prepare snapshot scratch directory."),
            QStringLiteral("SNAP_002: mkpath failed for %1").arg(kTopLevelScratch));
    }

    const Slm::Installer::CommandResult mount = Slm::Installer::SlmCommand::run(QStringLiteral("mount"),
        { QStringLiteral("-o"), QStringLiteral("subvolid=5"),
          rootDevice, QString::fromLatin1(kTopLevelScratch) });
    if (!mount.started || mount.exitCode != 0) {
        return Calamares::JobResult::error(
            tr("Unable to mount Btrfs top-level subvolume."),
            QStringLiteral("SNAP_002: mount subvolid=5 %1 exit=%2: %3")
                .arg(rootDevice).arg(mount.exitCode).arg(mount.output.trimmed()));
    }

    const Slm::Installer::CommandResult snap = Slm::Installer::SlmCommand::run(QStringLiteral("btrfs"),
        { QStringLiteral("subvolume"), QStringLiteral("snapshot"),
          QStringLiteral("-r"), rootMount, snapshotPath });
    const bool snapFailed = !snap.started || snap.exitCode != 0;
    const QString snapOutput = snap.output.trimmed();
    const int snapExit = snap.exitCode;

    // Always attempt to unmount, even on snapshot failure. A leaked scratch
    // mount would block later modules and any installer re-run.
    const Slm::Installer::CommandResult umount = Slm::Installer::SlmCommand::run(QStringLiteral("umount"),
        { QString::fromLatin1(kTopLevelScratch) });
    if (!umount.started || umount.exitCode != 0) {
        cWarning() << "slm-snapshot: failed to unmount" << kTopLevelScratch
                   << "exit=" << umount.exitCode << "out=" << umount.output.trimmed();
        // Not promoted to a JobResult error: the snapshot success is what
        // matters for recovery; the leftover mount can be cleaned up later.
    }

    if (snapFailed) {
        return Calamares::JobResult::error(
            tr("Unable to create factory snapshot."),
            QStringLiteral("SNAP_003: btrfs snapshot exit=%1: %2")
                .arg(snapExit).arg(snapOutput));
    }
    return Calamares::JobResult::ok();
}

void SlmSnapshotJob::setConfigurationMap(const QVariantMap &configurationMap)
{
    if (configurationMap.contains(QStringLiteral("dry-run"))) {
        m_dryRun = configurationMap.value(QStringLiteral("dry-run")).toBool();
    }
}
