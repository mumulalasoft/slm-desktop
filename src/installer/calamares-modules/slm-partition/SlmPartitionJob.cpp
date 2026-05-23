#include "SlmPartitionJob.h"

#include <GlobalStorage.h>
#include <JobQueue.h>
#include <utils/Logger.h>

#include <QChar>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>
#include <QVariantMap>

#include <optional>

#include <sys/stat.h>
#include <sys/sysmacros.h>

CALAMARES_PLUGIN_FACTORY_DEFINITION(SlmPartitionJobFactory, registerPlugin<SlmPartitionJob>();)

namespace {

// §2.1 sizing: ESP 1G, Recovery 8G, Root takes the remainder.
constexpr qint64 kEspSizeMb = 1024;
constexpr qint64 kRecoverySizeMb = 8 * 1024;
// Minimum total disk: ESP + Recovery + at least ~24G for root. The §12 open
// question on recovery sizing for <50G disks will eventually refine this.
constexpr qint64 kMinDiskMb = 32 * 1024;

// /sys/block/<name>/size is in 512-byte sectors.
constexpr qint64 kSysBlockSectorBytes = 512;

QString diskBasename(const QString &path)
{
    return QFileInfo(path).fileName();
}

// /dev/sda → /dev/sda1; /dev/nvme0n1 → /dev/nvme0n1p1; /dev/mmcblk0 → /dev/mmcblk0p1.
QString partitionPath(const QString &disk, int index)
{
    if (disk.isEmpty()) {
        return {};
    }
    const QChar tail = disk.back();
    const QString sep = tail.isDigit() ? QStringLiteral("p") : QString();
    return disk + sep + QString::number(index);
}

bool isBlockDevice(const QString &path)
{
    struct stat st {};
    if (::stat(path.toLocal8Bit().constData(), &st) != 0) {
        return false;
    }
    return S_ISBLK(st.st_mode);
}

std::optional<qint64> diskSizeMb(const QString &disk)
{
    const QString sysPath = QStringLiteral("/sys/block/") + diskBasename(disk)
                            + QStringLiteral("/size");
    QFile f(sysPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::nullopt;
    }
    bool ok = false;
    const qulonglong sectors = QString::fromLatin1(f.readAll()).trimmed().toULongLong(&ok);
    if (!ok || sectors == 0) {
        return std::nullopt;
    }
    return static_cast<qint64>(sectors) * kSysBlockSectorBytes / (1024 * 1024);
}

struct CmdResult
{
    bool started = false;
    int exitCode = -1;
    QString output;
};

// Generic subprocess wrapper used by the real-execution path. Merges
// stdout+stderr so a failure can quote the full noise back to the user
// in JobResult details. Defaults to a 60s timeout (mkfs.btrfs on a slow
// HDD can take 30+ seconds; sgdisk is fast).
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

enum class SmartOutcome { Ok, Failed, Unknown };

SmartOutcome runSmartHealthCheck(const QString &disk)
{
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(QStringLiteral("smartctl"), { QStringLiteral("-H"), disk });
    if (!proc.waitForStarted(2000)) {
        cWarning() << "slm-partition: smartctl unavailable";
        return SmartOutcome::Unknown;
    }
    if (!proc.waitForFinished(10000)) {
        proc.kill();
        return SmartOutcome::Unknown;
    }
    const QString out = QString::fromUtf8(proc.readAll());
    // smartctl exit codes form a bitmask: bit 3 (8) = device reports FAILED.
    const int exitCode = proc.exitCode();
    if (exitCode & 8) {
        return SmartOutcome::Failed;
    }
    if (out.contains(QStringLiteral("PASSED"), Qt::CaseInsensitive)
        || out.contains(QStringLiteral("OK"), Qt::CaseSensitive)) {
        return SmartOutcome::Ok;
    }
    return SmartOutcome::Unknown;
}

} // namespace

SlmPartitionJob::SlmPartitionJob(QObject *parent)
    : Calamares::CppJob(parent)
{
}

SlmPartitionJob::~SlmPartitionJob() = default;

QString SlmPartitionJob::prettyName() const
{
    return tr("Partition the target disk");
}

Calamares::JobResult SlmPartitionJob::exec()
{
    auto *gs = Calamares::JobQueue::instanceGlobalStorage();
    if (!gs) {
        return Calamares::JobResult::error(
            tr("Installer state unavailable."),
            QStringLiteral("PART_001: GlobalStorage missing"));
    }

    const QString targetDisk = gs->value(QStringLiteral("slm.target.disk")).toString().trimmed();
    if (targetDisk.isEmpty()) {
        return Calamares::JobResult::error(
            tr("No target disk has been selected."),
            QStringLiteral("PART_001: slm.target.disk not set"));
    }
    if (!isBlockDevice(targetDisk)) {
        return Calamares::JobResult::error(
            tr("Selected target is not a block device."),
            QStringLiteral("PART_002: %1 is not a block device").arg(targetDisk));
    }

    const std::optional<qint64> sizeMb = diskSizeMb(targetDisk);
    if (!sizeMb.has_value() || sizeMb.value() < kMinDiskMb) {
        return Calamares::JobResult::error(
            tr("Selected disk is too small to install SLM Desktop."),
            QStringLiteral("PART_003: %1 has %2 MB; %3 MB required")
                .arg(targetDisk)
                .arg(sizeMb.has_value() ? sizeMb.value() : -1)
                .arg(kMinDiskMb));
    }

    const SmartOutcome smart = runSmartHealthCheck(targetDisk);
    const bool smartOk = (smart == SmartOutcome::Ok);
    gs->insert(QStringLiteral("slm.hw.disk.smart_ok"), smartOk);
    if (smart == SmartOutcome::Failed) {
        QStringList warnings = gs->value(QStringLiteral("slm.hw.warnings")).toStringList();
        warnings << QStringLiteral("HW_W003");
        gs->insert(QStringLiteral("slm.hw.warnings"), warnings);
    }

    const qint64 rootSizeMb = sizeMb.value() - kEspSizeMb - kRecoverySizeMb;

    const QString espDevice = partitionPath(targetDisk, 1);
    const QString recoveryDevice = partitionPath(targetDisk, 2);
    const QString rootDevice = partitionPath(targetDisk, 3);

    gs->insert(QStringLiteral("slm.target.esp_device"), espDevice);
    gs->insert(QStringLiteral("slm.target.recovery_device"), recoveryDevice);
    gs->insert(QStringLiteral("slm.target.root_device"), rootDevice);
    gs->insert(QStringLiteral("slm.target.esp_size_mb"), kEspSizeMb);
    gs->insert(QStringLiteral("slm.target.recovery_size_mb"), kRecoverySizeMb);
    gs->insert(QStringLiteral("slm.target.root_size_mb"), rootSizeMb);

    cDebug() << "slm-partition: target=" << targetDisk
             << "size_mb=" << sizeMb.value()
             << "smart=" << (smart == SmartOutcome::Ok ? "ok"
                             : smart == SmartOutcome::Failed ? "failed" : "unknown")
             << "esp=" << espDevice
             << "recovery=" << recoveryDevice
             << "root=" << rootDevice
             << "dry_run=" << m_dryRun;

    if (m_dryRun) {
        cDebug() << "slm-partition: DRY RUN — would run:";
        cDebug() << "  sgdisk --zap-all" << targetDisk;
        cDebug() << "  sgdisk --new=1:0:+" << kEspSizeMb
                 << "M --typecode=1:EF00 --change-name=1:'EFI System Partition'";
        cDebug() << "  sgdisk --new=2:0:+" << kRecoverySizeMb
                 << "M --typecode=2:8300 --change-name=2:'SLM Recovery'";
        cDebug() << "  sgdisk --new=3:0:0 --typecode=3:8300 --change-name=3:'SLM Root'"
                 << targetDisk;
        cDebug() << "  mkfs.fat -F32 -n SLM-EFI" << espDevice;
        cDebug() << "  mkfs.ext4 -L SLM-RECOVERY" << recoveryDevice;
        cDebug() << "  mkfs.btrfs -L SLM-ROOT -f" << rootDevice;
        gs->insert(QStringLiteral("slm.partition.dry_run"), true);
        return Calamares::JobResult::ok();
    }

    // Defense in depth: no destructive path runs without the §2.5 summary
    // screen having explicitly confirmed. This guard is independent of
    // m_dryRun so a misconfigured `dry-run: false` still can't reach
    // destruction without user opt-in.
    if (!gs->value(QStringLiteral("slm.target.confirmed")).toBool()) {
        return Calamares::JobResult::error(
            tr("Installation has not been confirmed."),
            QStringLiteral("PART_006: slm.target.confirmed not set"));
    }

    // Real execution: wipe, partition, format. The §2.1 sequence — kept in
    // sync with the bash recipe documented in docs/SLM_INSTALLER_BACKEND.md.
    //
    // We do NOT mount anything here; staging mounts are slm-btrfs-layout's
    // responsibility. We do NOT publish UUIDs here either; slm-fstab reads
    // them via blkid at its own runtime to avoid stale cached values.

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

    // 1. Wipe existing GPT/MBR signatures.
    cDebug() << "slm-partition:" << "running sgdisk --zap-all" << targetDisk;
    auto wipe = runCmd(QStringLiteral("sgdisk"),
                       { QStringLiteral("--zap-all"), targetDisk });
    if (!wipe.started || wipe.exitCode != 0) {
        return fail(QStringLiteral("PART_010"),
                    tr("Unable to wipe the disk's partition table."), wipe);
    }

    // 2. Create the three partitions in a single sgdisk invocation
    // (§2.1 recipe — fewer round-trips through the kernel partition table
    // update than three separate calls).
    cDebug() << "slm-partition: creating GPT layout on" << targetDisk;
    const QString espSize = QStringLiteral("0:+%1M").arg(kEspSizeMb);
    const QString recoverySize = QStringLiteral("0:+%1M").arg(kRecoverySizeMb);
    auto part = runCmd(QStringLiteral("sgdisk"), {
        QStringLiteral("--new=1:") + espSize,
        QStringLiteral("--typecode=1:EF00"),
        QStringLiteral("--change-name=1:EFI System Partition"),
        QStringLiteral("--new=2:") + recoverySize,
        QStringLiteral("--typecode=2:8300"),
        QStringLiteral("--change-name=2:SLM Recovery"),
        QStringLiteral("--new=3:0:0"),
        QStringLiteral("--typecode=3:8300"),
        QStringLiteral("--change-name=3:SLM Root"),
        targetDisk,
    });
    if (!part.started || part.exitCode != 0) {
        return fail(QStringLiteral("PART_011"),
                    tr("Unable to create the partition table."), part);
    }

    // 3. Force the kernel to re-read the partition table, then wait for
    // udev to materialise the partition device nodes. Without this the
    // mkfs.* calls below can hit a race where /dev/<disk>N doesn't exist
    // yet. partprobe is the primary mechanism; udevadm settle is the
    // safety net.
    runCmd(QStringLiteral("partprobe"), { targetDisk }, 10000);
    runCmd(QStringLiteral("udevadm"),
           { QStringLiteral("settle"), QStringLiteral("--timeout=10") },
           15000);

    // Sanity-check that the partition nodes appeared. If they didn't,
    // we don't want to mkfs against /dev/sda1 that's still a stale node
    // from a previous layout.
    for (const QString &dev : { espDevice, recoveryDevice, rootDevice }) {
        if (!isBlockDevice(dev)) {
            return Calamares::JobResult::error(
                tr("Partition device did not appear after partitioning."),
                QStringLiteral("PART_012: %1 is not a block device").arg(dev));
        }
    }

    // 4. Format each partition with its target filesystem. -F / -f is
    // important: the disk could have had a previous filesystem signature
    // that the format tools would otherwise refuse to overwrite.
    cDebug() << "slm-partition: mkfs.fat" << espDevice;
    auto efi = runCmd(QStringLiteral("mkfs.fat"), {
        QStringLiteral("-F32"),
        QStringLiteral("-n"), QStringLiteral("SLM-EFI"),
        espDevice,
    });
    if (!efi.started || efi.exitCode != 0) {
        return fail(QStringLiteral("PART_013"),
                    tr("Unable to format the EFI partition."), efi);
    }

    cDebug() << "slm-partition: mkfs.ext4" << recoveryDevice;
    auto rec = runCmd(QStringLiteral("mkfs.ext4"), {
        QStringLiteral("-F"),
        QStringLiteral("-L"), QStringLiteral("SLM-RECOVERY"),
        recoveryDevice,
    });
    if (!rec.started || rec.exitCode != 0) {
        return fail(QStringLiteral("PART_014"),
                    tr("Unable to format the recovery partition."), rec);
    }

    cDebug() << "slm-partition: mkfs.btrfs" << rootDevice;
    auto root = runCmd(QStringLiteral("mkfs.btrfs"), {
        QStringLiteral("-f"),
        QStringLiteral("-L"), QStringLiteral("SLM-ROOT"),
        rootDevice,
    });
    if (!root.started || root.exitCode != 0) {
        return fail(QStringLiteral("PART_015"),
                    tr("Unable to format the root partition."), root);
    }

    gs->insert(QStringLiteral("slm.partition.executed"), true);
    cDebug() << "slm-partition: real execution complete";
    return Calamares::JobResult::ok();
}

void SlmPartitionJob::setConfigurationMap(const QVariantMap &configurationMap)
{
    if (configurationMap.contains(QStringLiteral("dry-run"))) {
        m_dryRun = configurationMap.value(QStringLiteral("dry-run")).toBool();
    }
}
