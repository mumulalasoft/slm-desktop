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

    // Real execution is intentionally not wired up yet — the disk-select UI
    // that sets slm.target.confirmed does not exist, and shipping a destructive
    // path without it risks wiping the wrong disk in test harnesses.
    return Calamares::JobResult::error(
        tr("Disk partitioning is not yet implemented."),
        QStringLiteral("PART_004: real-execution path pending disk-select UI"));
}

void SlmPartitionJob::setConfigurationMap(const QVariantMap &configurationMap)
{
    if (configurationMap.contains(QStringLiteral("dry-run"))) {
        m_dryRun = configurationMap.value(QStringLiteral("dry-run")).toBool();
    }
}
