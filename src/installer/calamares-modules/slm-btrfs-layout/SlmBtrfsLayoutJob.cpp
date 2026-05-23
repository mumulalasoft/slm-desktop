#include "SlmBtrfsLayoutJob.h"

#include <GlobalStorage.h>
#include <JobQueue.h>
#include <utils/Logger.h>

#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <sys/stat.h>

CALAMARES_PLUGIN_FACTORY_DEFINITION(SlmBtrfsLayoutJobFactory, registerPlugin<SlmBtrfsLayoutJob>();)

namespace {

// §2.2 — order matters: @ must be created first.
const QStringList kSubvolumes = {
    QStringLiteral("@"),
    QStringLiteral("@home"),
    QStringLiteral("@var"),
    QStringLiteral("@log"),
    QStringLiteral("@cache"),
    QStringLiteral("@snapshots"),
    QStringLiteral("@resources"),
    QStringLiteral("@recovery-staging"),
};

// §2.3 — subvolumes that appear in the staging mount sequence (and §2.4 fstab).
// @recovery-staging is intentionally not mounted in the normal flow.
struct MountPlan
{
    const char *subvol;
    const char *target;
    const char *options;
};

constexpr MountPlan kMountPlan[] = {
    { "@",          "/",                   "compress=zstd:3,noatime,ssd" },
    { "@home",      "/home",               "compress=zstd:3,noatime,ssd" },
    { "@var",       "/var",                "compress=zstd:3,noatime,ssd" },
    { "@log",       "/var/log",            "compress=zstd:3,noatime,ssd" },
    { "@cache",     "/var/cache",          "compress=zstd:3,noatime,ssd" },
    { "@snapshots", "/.snapshots",         "noatime,ssd" },
    { "@resources", "/opt/slm/resources",  "compress=zstd:3,noatime" },
};

constexpr const char *kScratchMount = "/mnt/slm-btrfs-scratch";

bool isBlockDevice(const QString &path)
{
    struct stat st {};
    if (::stat(path.toLocal8Bit().constData(), &st) != 0) {
        return false;
    }
    return S_ISBLK(st.st_mode);
}

QVariantList buildMountPlan()
{
    QVariantList plan;
    plan.reserve(int(sizeof(kMountPlan) / sizeof(kMountPlan[0])));
    for (const MountPlan &entry : kMountPlan) {
        QVariantMap row;
        row.insert(QStringLiteral("subvolume"), QLatin1String(entry.subvol));
        row.insert(QStringLiteral("target"), QLatin1String(entry.target));
        row.insert(QStringLiteral("options"), QLatin1String(entry.options));
        plan.append(row);
    }
    return plan;
}

} // namespace

SlmBtrfsLayoutJob::SlmBtrfsLayoutJob(QObject *parent)
    : Calamares::CppJob(parent)
{
}

SlmBtrfsLayoutJob::~SlmBtrfsLayoutJob() = default;

QString SlmBtrfsLayoutJob::prettyName() const
{
    return tr("Create Btrfs subvolumes");
}

Calamares::JobResult SlmBtrfsLayoutJob::exec()
{
    auto *gs = Calamares::JobQueue::instanceGlobalStorage();
    if (!gs) {
        return Calamares::JobResult::error(
            tr("Installer state unavailable."),
            QStringLiteral("BTRFS_001: GlobalStorage missing"));
    }

    const QString rootDevice = gs->value(QStringLiteral("slm.target.root_device"))
                                   .toString().trimmed();
    if (rootDevice.isEmpty()) {
        return Calamares::JobResult::error(
            tr("Root device not yet partitioned."),
            QStringLiteral("BTRFS_001: slm.target.root_device not set"));
    }
    // In dry-run we don't require the device to exist yet — slm-partition
    // also runs dry, so the path may not be materialised. The block-device
    // check only matters for the real execution path.
    if (!m_dryRun && !isBlockDevice(rootDevice)) {
        return Calamares::JobResult::error(
            tr("Root partition is not a block device."),
            QStringLiteral("BTRFS_002: %1 is not a block device").arg(rootDevice));
    }

    const QVariantList mountPlan = buildMountPlan();
    gs->insert(QStringLiteral("slm.btrfs.subvolumes"), QVariant(kSubvolumes));
    gs->insert(QStringLiteral("slm.btrfs.scratch_mount"), QLatin1String(kScratchMount));
    gs->insert(QStringLiteral("slm.btrfs.mount_plan"), mountPlan);

    cDebug() << "slm-btrfs-layout: root=" << rootDevice
             << "subvolumes=" << kSubvolumes.join(QLatin1Char(','))
             << "dry_run=" << m_dryRun;

    if (m_dryRun) {
        cDebug() << "slm-btrfs-layout: DRY RUN — would run:";
        cDebug() << "  mkdir -p" << kScratchMount;
        cDebug() << "  mount" << rootDevice << kScratchMount;
        for (const QString &subvol : kSubvolumes) {
            cDebug() << "  btrfs subvolume create"
                     << QString(QLatin1String(kScratchMount) + QLatin1Char('/') + subvol);
        }
        cDebug() << "  umount" << kScratchMount;
        gs->insert(QStringLiteral("slm.btrfs.dry_run"), true);
        return Calamares::JobResult::ok();
    }

    // Real execution waits on slm-partition's real-execution follow-up so
    // there is an actual Btrfs filesystem on rootDevice to operate on.
    return Calamares::JobResult::error(
        tr("Btrfs subvolume creation is not yet implemented."),
        QStringLiteral("BTRFS_003: real-execution path pending slm-partition follow-up"));
}

void SlmBtrfsLayoutJob::setConfigurationMap(const QVariantMap &configurationMap)
{
    if (configurationMap.contains(QStringLiteral("dry-run"))) {
        m_dryRun = configurationMap.value(QStringLiteral("dry-run")).toBool();
    }
}
