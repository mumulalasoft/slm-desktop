#include "SlmRecoveryJob.h"

#include <GlobalStorage.h>
#include <JobQueue.h>
#include <utils/Logger.h>

#include <QByteArray>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

CALAMARES_PLUGIN_FACTORY_DEFINITION(SlmRecoveryJobFactory, registerPlugin<SlmRecoveryJob>();)

namespace {

// §4.1 — directory layout at the mounted recovery partition root.
constexpr const char *kRecoveryStaging = "/mnt/slm-recovery-staging";

const QStringList kRecoveryDirs = {
    QStringLiteral("boot"),
    QStringLiteral("rootfs"),
    QStringLiteral("factory"),
    QStringLiteral("logs"),
    QStringLiteral("metadata"),
};

constexpr const char *kManifestVersion = "1.0";
constexpr const char *kInstallerVersion = "1.0.0";

QString resolveRootMountPoint(Calamares::GlobalStorage *gs)
{
    const QString fromGs = gs->value(QStringLiteral("rootMountPoint")).toString();
    return fromGs.isEmpty() ? QStringLiteral("/mnt/slm-install-root") : fromGs;
}

QJsonObject buildInstallManifest(Calamares::GlobalStorage *gs)
{
    QJsonObject manifest;
    manifest.insert(QStringLiteral("manifest_version"), QLatin1String(kManifestVersion));
    manifest.insert(QStringLiteral("installer_version"), QLatin1String(kInstallerVersion));
    manifest.insert(QStringLiteral("installed_at"),
                    QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    manifest.insert(QStringLiteral("install_id"),
                    gs->value(QStringLiteral("slm.firstboot.install_id")).toString());
    manifest.insert(QStringLiteral("target_disk"),
                    gs->value(QStringLiteral("slm.target.disk")).toString());
    manifest.insert(QStringLiteral("esp_device"),
                    gs->value(QStringLiteral("slm.target.esp_device")).toString());
    manifest.insert(QStringLiteral("recovery_device"),
                    gs->value(QStringLiteral("slm.target.recovery_device")).toString());
    manifest.insert(QStringLiteral("root_device"),
                    gs->value(QStringLiteral("slm.target.root_device")).toString());

    const QStringList subvolumes = gs->value(QStringLiteral("slm.btrfs.subvolumes")).toStringList();
    QJsonArray subvolArr;
    for (const QString &s : subvolumes) {
        subvolArr.append(s);
    }
    manifest.insert(QStringLiteral("subvolumes"), subvolArr);
    manifest.insert(QStringLiteral("factory_snapshot"),
                    gs->value(QStringLiteral("slm.snapshot.name")).toString());

    return manifest;
}

} // namespace

SlmRecoveryJob::SlmRecoveryJob(QObject *parent)
    : Calamares::CppJob(parent)
{
}

SlmRecoveryJob::~SlmRecoveryJob() = default;

QString SlmRecoveryJob::prettyName() const
{
    return tr("Set up recovery partition");
}

Calamares::JobResult SlmRecoveryJob::exec()
{
    auto *gs = Calamares::JobQueue::instanceGlobalStorage();
    if (!gs) {
        return Calamares::JobResult::error(
            tr("Installer state unavailable."),
            QStringLiteral("RCVR_001: GlobalStorage missing"));
    }

    const QString recoveryDevice = gs->value(QStringLiteral("slm.target.recovery_device"))
                                       .toString();
    if (recoveryDevice.isEmpty()) {
        return Calamares::JobResult::error(
            tr("Recovery partition not yet known."),
            QStringLiteral("RCVR_001: slm.target.recovery_device not set"));
    }

    const QString rootMount = resolveRootMountPoint(gs);
    const QString staging = QString::fromLatin1(kRecoveryStaging);

    // §4.2 independence: kernel and initrd come from the staging root's /boot
    // and must be COPIED, not symlinked, into the recovery partition's /boot.
    const QString srcKernel = rootMount + QStringLiteral("/boot/vmlinuz");
    const QString srcInitrd = rootMount + QStringLiteral("/boot/initrd.img");
    const QString dstKernel = staging + QStringLiteral("/boot/vmlinuz");
    const QString dstInitrd = staging + QStringLiteral("/boot/initrd.img");
    const QString manifestPath = staging + QStringLiteral("/metadata/install-manifest.json");
    const QString protectedSrc = gs->value(QStringLiteral("slm.protected.path")).toString();
    const QString protectedDst = staging + QStringLiteral("/metadata/protected-packages.json");

    const QJsonObject manifest = buildInstallManifest(gs);
    const QByteArray manifestBytes = QJsonDocument(manifest).toJson(QJsonDocument::Indented);

    gs->insert(QStringLiteral("slm.recovery.staging"), staging);
    gs->insert(QStringLiteral("slm.recovery.kernel_dst"), dstKernel);
    gs->insert(QStringLiteral("slm.recovery.initrd_dst"), dstInitrd);
    gs->insert(QStringLiteral("slm.recovery.manifest_path"), manifestPath);
    gs->insert(QStringLiteral("slm.recovery.manifest_preview"),
               QString::fromUtf8(manifestBytes));
    gs->insert(QStringLiteral("slm.recovery.dirs"), QVariant(kRecoveryDirs));

    cDebug() << "slm-recovery:"
             << "recovery_device=" << recoveryDevice
             << "staging=" << staging
             << "kernel_src=" << srcKernel
             << "manifest_path=" << manifestPath
             << "dry_run=" << m_dryRun;

    if (m_dryRun) {
        cDebug() << "slm-recovery: DRY RUN — would run:";
        cDebug() << "  mkdir -p" << staging;
        cDebug() << "  mount" << recoveryDevice << staging;
        for (const QString &dir : kRecoveryDirs) {
            cDebug() << "  mkdir -p" << (staging + QLatin1Char('/') + dir);
        }
        cDebug() << "  cp" << srcKernel << dstKernel;
        cDebug() << "  cp" << srcInitrd << dstInitrd;
        cDebug() << "  write" << manifestPath << "(" << manifestBytes.size() << "bytes)";
        if (!protectedSrc.isEmpty()) {
            cDebug() << "  cp" << protectedSrc << protectedDst;
        }
        cDebug() << "  # recovery.squashfs build deferred (needs dracut + custom rootfs)";
        cDebug() << "  # factory image generation deferred (needs btrfs send | zstd of @factory-initial)";
        cDebug() << "  umount" << staging;
        gs->insert(QStringLiteral("slm.recovery.dry_run"), true);
        return Calamares::JobResult::ok();
    }

    // Real path stays gated: mounting and writing to the recovery partition
    // requires it to already be formatted by slm-partition's real path; until
    // that is wired up there is no recovery filesystem to populate. Recovery
    // squashfs build and factory-image generation are separate follow-ups
    // (they depend on dracut configuration and disk-space policy from §4.4).
    return Calamares::JobResult::error(
        tr("Recovery partition setup is not yet implemented."),
        QStringLiteral("RCVR_005: real-execution path pending slm-partition follow-up"));
}

void SlmRecoveryJob::setConfigurationMap(const QVariantMap &configurationMap)
{
    if (configurationMap.contains(QStringLiteral("dry-run"))) {
        m_dryRun = configurationMap.value(QStringLiteral("dry-run")).toBool();
    }
}
