#include "SlmBootloaderJob.h"

#include "SlmCommand.h"

#include <GlobalStorage.h>
#include <JobQueue.h>
#include <utils/Logger.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QVariantMap>

CALAMARES_PLUGIN_FACTORY_DEFINITION(SlmBootloaderJobFactory, registerPlugin<SlmBootloaderJob>();)

namespace {

// §3.3 / §3.4 / §3.5 — content templates. %1 = btrfs UUID, %2 = recovery UUID.
constexpr const char *kLoaderConf =
    "# /boot/efi/loader/loader.conf\n"
    "default  slm-desktop.conf\n"
    "timeout  3\n"
    "console-mode  auto\n"
    "editor   no\n";

constexpr const char *kEntryDesktop =
    "title   SLM Desktop\n"
    "linux   /EFI/slm/vmlinuz\n"
    "initrd  /EFI/slm/initrd.img\n"
    "options root=UUID=%1 rootflags=subvol=@ rw quiet splash"
    " loglevel=3 udev.log_level=3 systemd.show_status=auto\n";

constexpr const char *kEntryRecovery =
    "title   SLM Recovery\n"
    "linux   /EFI/slm-recovery/vmlinuz\n"
    "initrd  /EFI/slm-recovery/initrd.img\n"
    "options root=UUID=%1 rw quiet"
    " systemd.unit=slm-recovery.target slm.recovery=1\n";

constexpr const char *kEntrySafeMode =
    "title   SLM Safe Mode\n"
    "linux   /EFI/slm/vmlinuz\n"
    "initrd  /EFI/slm/initrd.img\n"
    "options root=UUID=%1 rootflags=subvol=@ rw"
    " slm.session=safe systemd.unit=graphical.target nomodeset\n";

QString resolveRootMountPoint(Calamares::GlobalStorage *gs)
{
    const QString fromGs = gs->value(QStringLiteral("rootMountPoint")).toString();
    return fromGs.isEmpty() ? QStringLiteral("/mnt/slm-install-root") : fromGs;
}

// Resolve a partition's UUID via blkid. Empty return signals failure;
// callers translate that into a JobResult::error with the appropriate
// BOOT_xxx code.
QString readUuid(const QString &device)
{
    const auto res = Slm::Installer::SlmCommand::run(
        QStringLiteral("blkid"),
        { QStringLiteral("-s"), QStringLiteral("UUID"),
          QStringLiteral("-o"), QStringLiteral("value"), device },
        5000);
    if (!res.started || res.exitCode != 0) {
        return {};
    }
    return res.output.trimmed();
}

bool writeFile(const QString &path, const QByteArray &content, QString *err)
{
    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath())) {
        *err = QStringLiteral("mkpath failed for %1").arg(info.absolutePath());
        return false;
    }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        *err = QStringLiteral("open(%1) failed: %2").arg(path, f.errorString());
        return false;
    }
    if (f.write(content) != content.size()) {
        *err = QStringLiteral("short write to %1: %2").arg(path, f.errorString());
        f.close();
        return false;
    }
    f.close();
    return true;
}

} // namespace

SlmBootloaderJob::SlmBootloaderJob(QObject *parent)
    : Calamares::CppJob(parent)
{
}

SlmBootloaderJob::~SlmBootloaderJob() = default;

QString SlmBootloaderJob::prettyName() const
{
    return tr("Install systemd-boot");
}

Calamares::JobResult SlmBootloaderJob::exec()
{
    auto *gs = Calamares::JobQueue::instanceGlobalStorage();
    if (!gs) {
        return Calamares::JobResult::error(
            tr("Installer state unavailable."),
            QStringLiteral("BOOT_001: GlobalStorage missing"));
    }

    const QString targetDisk = gs->value(QStringLiteral("slm.target.disk")).toString();
    const QString rootDevice = gs->value(QStringLiteral("slm.target.root_device")).toString();
    const QString recoveryDevice = gs->value(QStringLiteral("slm.target.recovery_device"))
                                       .toString();
    if (targetDisk.isEmpty() || rootDevice.isEmpty() || recoveryDevice.isEmpty()) {
        return Calamares::JobResult::error(
            tr("Disk layout not yet planned."),
            QStringLiteral("BOOT_001: missing slm.target.disk, "
                           "slm.target.root_device, or slm.target.recovery_device"));
    }

    const QString rootMount = resolveRootMountPoint(gs);
    const QString espRelative = QStringLiteral("/boot/efi");

    // In dry-run there's no filesystem yet — use placeholders that the summary
    // screen can render verbatim. Real path will replace via blkid (same
    // pattern as slm-fstab).
    const QString btrfsUuid = m_dryRun ? QStringLiteral("<btrfs-uuid>")
                                       : QString(); // resolved in real path (BOOT_005)
    const QString recoveryUuid = m_dryRun ? QStringLiteral("<recovery-uuid>")
                                          : QString();

    const QString loaderConf = QLatin1String(kLoaderConf);
    const QString entryDesktop = QString(QLatin1String(kEntryDesktop)).arg(btrfsUuid);
    const QString entryRecovery = QString(QLatin1String(kEntryRecovery)).arg(recoveryUuid);
    const QString entrySafeMode = QString(QLatin1String(kEntrySafeMode)).arg(btrfsUuid);

    QVariantMap entriesPreview;
    entriesPreview.insert(QStringLiteral("slm-desktop.conf"), entryDesktop);
    entriesPreview.insert(QStringLiteral("slm-recovery.conf"), entryRecovery);
    entriesPreview.insert(QStringLiteral("slm-safe-mode.conf"), entrySafeMode);

    gs->insert(QStringLiteral("slm.bootloader.efi_path"), espRelative);
    gs->insert(QStringLiteral("slm.bootloader.label"), QStringLiteral("SLM Desktop"));
    gs->insert(QStringLiteral("slm.bootloader.preview.loader_conf"), loaderConf);
    gs->insert(QStringLiteral("slm.bootloader.preview.entries"), entriesPreview);

    cDebug() << "slm-bootloader:"
             << "target_disk=" << targetDisk
             << "esp=" << espRelative
             << "root_uuid=" << btrfsUuid
             << "recovery_uuid=" << recoveryUuid
             << "dry_run=" << m_dryRun;

    if (m_dryRun) {
        cDebug() << "slm-bootloader: DRY RUN — would run:";
        cDebug() << "  chroot" << rootMount
                 << "bootctl install --path=/boot/efi --no-variables";
        cDebug() << "  efibootmgr --create"
                 << "--disk" << targetDisk << "--part 1"
                 << "--label 'SLM Desktop'"
                 << "--loader '\\EFI\\systemd\\systemd-bootx64.efi'";
        cDebug() << "  write" << (rootMount + espRelative + QStringLiteral("/loader/loader.conf"));
        for (auto it = entriesPreview.constBegin(); it != entriesPreview.constEnd(); ++it) {
            cDebug() << "  write"
                     << (rootMount + espRelative + QStringLiteral("/loader/entries/") + it.key());
        }
        gs->insert(QStringLiteral("slm.bootloader.dry_run"), true);
        return Calamares::JobResult::ok();
    }

    // Defense in depth — see slm-partition for the rationale.
    if (!gs->value(QStringLiteral("slm.target.confirmed")).toBool()) {
        return Calamares::JobResult::error(
            tr("Installation has not been confirmed."),
            QStringLiteral("BOOT_006: slm.target.confirmed not set"));
    }

    // Real execution: bootctl install inside the chroot, write the
    // loader.conf + entries with real UUIDs, then efibootmgr from the
    // live env. Sequence matches docs/SLM_INSTALLER_BACKEND.md §3.2.

    auto fail = [](const QString &code, const QString &userMsg,
                   const Slm::Installer::CommandResult &res) {
        return Calamares::JobResult::error(
            userMsg,
            QStringLiteral("%1: exit=%2 started=%3 output=%4")
                .arg(code)
                .arg(res.exitCode)
                .arg(res.started ? QStringLiteral("yes") : QStringLiteral("no"))
                .arg(res.output.trimmed()));
    };

    // 1. bootctl install --no-variables inside the chroot. This places the
    // systemd-boot binaries in $ESP/EFI/systemd/ and creates the loader/
    // directory. --no-variables defers EFI variable creation to efibootmgr
    // (step 4) so we control the boot entry label and ordering.
    cDebug() << "slm-bootloader: chroot bootctl install" << rootMount;
    const auto bootctl = Slm::Installer::SlmCommand::run(QStringLiteral("chroot"), {
        rootMount,
        QStringLiteral("bootctl"),
        QStringLiteral("install"),
        QStringLiteral("--path=/boot/efi"),
        QStringLiteral("--no-variables"),
    });
    if (!bootctl.started || bootctl.exitCode != 0) {
        return fail(QStringLiteral("BOOT_010"),
                    tr("Unable to install systemd-boot."), bootctl);
    }

    // 2. Resolve the real Btrfs and Recovery partition UUIDs. blkid runs
    // outside the chroot — slm-fstab already proved this works at this
    // point in the pipeline (the filesystems were just created by
    // slm-partition, then mounted by slm-btrfs-layout).
    const QString realBtrfsUuid = readUuid(rootDevice);
    if (realBtrfsUuid.isEmpty()) {
        return Calamares::JobResult::error(
            tr("Unable to read Btrfs root UUID."),
            QStringLiteral("BOOT_011: blkid failed for %1").arg(rootDevice));
    }
    const QString realRecoveryUuid = readUuid(recoveryDevice);
    if (realRecoveryUuid.isEmpty()) {
        return Calamares::JobResult::error(
            tr("Unable to read recovery partition UUID."),
            QStringLiteral("BOOT_011: blkid failed for %1").arg(recoveryDevice));
    }

    // 3. Render the conf files with real UUIDs and write them. bootctl
    // install created loader/ already, but the entries/ subdirectory
    // may not exist — writeFile() mkpaths as needed.
    const QString loaderDir = rootMount + espRelative + QStringLiteral("/loader");
    const QString entriesDir = loaderDir + QStringLiteral("/entries");

    const QString realEntryDesktop  = QString(QLatin1String(kEntryDesktop)).arg(realBtrfsUuid);
    const QString realEntryRecovery = QString(QLatin1String(kEntryRecovery)).arg(realRecoveryUuid);
    const QString realEntrySafeMode = QString(QLatin1String(kEntrySafeMode)).arg(realBtrfsUuid);

    struct ConfFile { QString path; QByteArray content; };
    const ConfFile files[] = {
        { loaderDir + QStringLiteral("/loader.conf"), loaderConf.toUtf8() },
        { entriesDir + QStringLiteral("/slm-desktop.conf"), realEntryDesktop.toUtf8() },
        { entriesDir + QStringLiteral("/slm-recovery.conf"), realEntryRecovery.toUtf8() },
        { entriesDir + QStringLiteral("/slm-safe-mode.conf"), realEntrySafeMode.toUtf8() },
    };
    for (const ConfFile &f : files) {
        QString err;
        if (!writeFile(f.path, f.content, &err)) {
            return Calamares::JobResult::error(
                tr("Unable to write bootloader configuration."),
                QStringLiteral("BOOT_012: %1").arg(err));
        }
    }

    // 4. efibootmgr from the live env — writes the EFI variables that
    // point firmware at our newly-installed systemd-boot binary. Runs
    // OUTSIDE the chroot (the live ISO's efibootmgr has access to
    // /sys/firmware/efi/efivars; the staged root doesn't yet).
    cDebug() << "slm-bootloader: efibootmgr --create" << targetDisk;
    const auto efimgr = Slm::Installer::SlmCommand::run(QStringLiteral("efibootmgr"), {
        QStringLiteral("--create"),
        QStringLiteral("--disk"), targetDisk,
        QStringLiteral("--part"), QStringLiteral("1"),
        QStringLiteral("--label"), QStringLiteral("SLM Desktop"),
        QStringLiteral("--loader"),
        QStringLiteral("\\EFI\\systemd\\systemd-bootx64.efi"),
    });
    if (!efimgr.started || efimgr.exitCode != 0) {
        return fail(QStringLiteral("BOOT_014"),
                    tr("Unable to register the SLM boot entry with firmware."),
                    efimgr);
    }

    gs->insert(QStringLiteral("slm.bootloader.executed"), true);
    cDebug() << "slm-bootloader: real execution complete";
    return Calamares::JobResult::ok();
}

void SlmBootloaderJob::setConfigurationMap(const QVariantMap &configurationMap)
{
    if (configurationMap.contains(QStringLiteral("dry-run"))) {
        m_dryRun = configurationMap.value(QStringLiteral("dry-run")).toBool();
    }
}
