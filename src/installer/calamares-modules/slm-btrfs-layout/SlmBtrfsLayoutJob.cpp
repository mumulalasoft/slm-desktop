#include "SlmBtrfsLayoutJob.h"

#include <GlobalStorage.h>
#include <JobQueue.h>
#include <utils/Logger.h>

#include <QDir>
#include <QProcess>
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
constexpr const char *kStagingRoot = "/mnt/slm-install-root";

struct CmdResult
{
    bool started = false;
    int exitCode = -1;
    QString output;
};

// Generic subprocess wrapper. Duplicates the slm-partition helper for now;
// when a fourth user appears these graduate to shared/SlmCommand.
CmdResult runCmd(const QString &program, const QStringList &args,
                 int timeoutMs = 60000)
{
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(program, args);
    CmdResult out;
    if (!proc.waitForStarted(3000)) {
        return out;
    }
    out.started = true;
    if (!proc.waitForFinished(timeoutMs)) {
        proc.kill();
        proc.waitForFinished(2000);
        out.exitCode = -2;
        out.output = QString::fromUtf8(proc.readAll());
        return out;
    }
    out.exitCode = proc.exitCode();
    out.output = QString::fromUtf8(proc.readAll());
    return out;
}

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

    // Defense in depth — see slm-partition for the rationale.
    if (!gs->value(QStringLiteral("slm.target.confirmed")).toBool()) {
        return Calamares::JobResult::error(
            tr("Installation has not been confirmed."),
            QStringLiteral("BTRFS_006: slm.target.confirmed not set"));
    }

    // Real execution: create the §2.2 subvolume set on the top-level
    // volume, then mount them at their §2.3 staging paths so slm-deploy
    // and downstream modules can write into them.
    const QString espDevice = gs->value(QStringLiteral("slm.target.esp_device")).toString();
    if (espDevice.isEmpty()) {
        return Calamares::JobResult::error(
            tr("EFI partition device not known."),
            QStringLiteral("BTRFS_007: slm.target.esp_device not set"));
    }

    auto fail = [](const QString &code, const QString &userMsg,
                   const CmdResult &res) {
        return Calamares::JobResult::error(
            userMsg,
            QStringLiteral("%1: exit=%2 started=%3 output=%4")
                .arg(code)
                .arg(res.exitCode)
                .arg(res.started ? QStringLiteral("yes") : QStringLiteral("no"))
                .arg(res.output.trimmed()));
    };

    // 1. Scratch-mount the top-level Btrfs volume so we can create
    // subvolumes at the right level. Default mount options are fine here;
    // we'll remount per-subvolume with the proper flags below.
    const QString scratch = QString::fromLatin1(kScratchMount);
    if (!QDir().mkpath(scratch)) {
        return Calamares::JobResult::error(
            tr("Unable to prepare Btrfs scratch directory."),
            QStringLiteral("BTRFS_010: mkpath failed for %1").arg(scratch));
    }
    auto scratchMount = runCmd(QStringLiteral("mount"), { rootDevice, scratch });
    if (!scratchMount.started || scratchMount.exitCode != 0) {
        return fail(QStringLiteral("BTRFS_010"),
                    tr("Unable to mount Btrfs scratch volume."), scratchMount);
    }

    // 2. Create each subvolume in spec order. If any fails we still need
    // to unmount the scratch volume — track failure as a scalar (per the
    // JobResult-is-move-only quirk in [[feedback-calamares-api]]) and
    // build the error after cleanup.
    QString createFailCode;
    QString createFailMsg;
    CmdResult createFailRes;
    for (const QString &subvol : kSubvolumes) {
        const QString path = scratch + QLatin1Char('/') + subvol;
        cDebug() << "slm-btrfs-layout: btrfs subvolume create" << path;
        auto sv = runCmd(QStringLiteral("btrfs"), {
            QStringLiteral("subvolume"),
            QStringLiteral("create"),
            path,
        });
        if (!sv.started || sv.exitCode != 0) {
            createFailCode = QStringLiteral("BTRFS_011");
            createFailMsg = tr("Unable to create Btrfs subvolume %1.").arg(subvol);
            createFailRes = sv;
            break;
        }
    }

    // 3. Always unmount the scratch — leaving it mounted would block
    // step 4 (the staging mount uses the same device with subvol= options
    // and the kernel would complain about double-mount).
    auto scratchUmount = runCmd(QStringLiteral("umount"), { scratch });
    if (!scratchUmount.started || scratchUmount.exitCode != 0) {
        cWarning() << "slm-btrfs-layout: scratch umount failed"
                   << "exit=" << scratchUmount.exitCode
                   << "out=" << scratchUmount.output.trimmed();
        // Don't promote unmount failure if subvolume creation succeeded —
        // the kernel will release the mount when the staging mount lands.
    }

    if (!createFailCode.isEmpty()) {
        return fail(createFailCode, createFailMsg, createFailRes);
    }

    // 4. Mount @ at the staging root, then each other subvolume at its
    // §2.3 target path. mkpath the target dirs first; they don't exist
    // until @ is mounted.
    if (!QDir().mkpath(QString::fromLatin1(kStagingRoot))) {
        return Calamares::JobResult::error(
            tr("Unable to prepare staging directory."),
            QStringLiteral("BTRFS_013: mkpath failed for %1").arg(kStagingRoot));
    }
    const QString stagingRoot = QString::fromLatin1(kStagingRoot);

    for (const MountPlan &entry : kMountPlan) {
        const QString subvol = QLatin1String(entry.subvol);
        const QString target = stagingRoot + QLatin1String(entry.target);
        const QString options = QStringLiteral("subvol=%1,%2")
                                    .arg(subvol, QLatin1String(entry.options));

        // For everything past @, the mount-point directory lives inside @
        // and was created by mkfs.btrfs's empty subvolume. We need to
        // mkdir it before mounting.
        if (subvol != QLatin1String("@")) {
            if (!QDir().mkpath(target)) {
                return Calamares::JobResult::error(
                    tr("Unable to prepare staging mount point."),
                    QStringLiteral("BTRFS_013: mkpath failed for %1").arg(target));
            }
        }

        cDebug() << "slm-btrfs-layout: mount" << subvol << "→" << target;
        auto m = runCmd(QStringLiteral("mount"), {
            QStringLiteral("-o"), options,
            rootDevice, target,
        });
        if (!m.started || m.exitCode != 0) {
            return fail(QStringLiteral("BTRFS_013"),
                        tr("Unable to mount Btrfs subvolume %1.").arg(subvol), m);
        }
    }

    // 5. Mount the ESP at $staging/boot/efi so slm-bootloader can write
    // to /boot/efi inside the chroot.
    const QString espTarget = stagingRoot + QStringLiteral("/boot/efi");
    if (!QDir().mkpath(espTarget)) {
        return Calamares::JobResult::error(
            tr("Unable to prepare ESP mount point."),
            QStringLiteral("BTRFS_014: mkpath failed for %1").arg(espTarget));
    }
    auto espMount = runCmd(QStringLiteral("mount"), { espDevice, espTarget });
    if (!espMount.started || espMount.exitCode != 0) {
        return fail(QStringLiteral("BTRFS_014"),
                    tr("Unable to mount the EFI partition."), espMount);
    }

    // 6. Calamares convention: tell downstream jobs where the staging
    // root lives via the standard rootMountPoint key. slm-fstab,
    // slm-bootloader, slm-recovery, slm-firstboot all read this.
    gs->insert(QStringLiteral("rootMountPoint"), stagingRoot);
    gs->insert(QStringLiteral("slm.btrfs.executed"), true);
    cDebug() << "slm-btrfs-layout: real execution complete; staging at" << stagingRoot;
    return Calamares::JobResult::ok();
}

void SlmBtrfsLayoutJob::setConfigurationMap(const QVariantMap &configurationMap)
{
    if (configurationMap.contains(QStringLiteral("dry-run"))) {
        m_dryRun = configurationMap.value(QStringLiteral("dry-run")).toBool();
    }
}
