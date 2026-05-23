#include "SlmBootloaderJob.h"

#include <GlobalStorage.h>
#include <JobQueue.h>
#include <utils/Logger.h>

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

    // Real execution touches the ESP and writes EFI variables — both have
    // host-wide blast radius if the wrong disk is targeted. Even past the
    // confirmation guard, the chroot bootctl + efibootmgr sequence has not
    // been implemented in this module yet.
    return Calamares::JobResult::error(
        tr("Bootloader installation is not yet implemented."),
        QStringLiteral("BOOT_005: real-execution path not yet implemented"));
}

void SlmBootloaderJob::setConfigurationMap(const QVariantMap &configurationMap)
{
    if (configurationMap.contains(QStringLiteral("dry-run"))) {
        m_dryRun = configurationMap.value(QStringLiteral("dry-run")).toBool();
    }
}
