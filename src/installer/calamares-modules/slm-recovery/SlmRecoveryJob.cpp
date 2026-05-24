#include "SlmRecoveryJob.h"

#include <GlobalStorage.h>
#include <JobQueue.h>
#include <utils/Logger.h>

#include "SlmCommand.h"

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
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

using CmdResult = Slm::Installer::CommandResult;
inline CmdResult runCmd(const QString &program, const QStringList &args,
                        int timeoutMs = 30000)
{
    return Slm::Installer::SlmCommand::run(program, args, timeoutMs);
}

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

    // Defense in depth — see slm-partition for the rationale.
    if (!gs->value(QStringLiteral("slm.target.confirmed")).toBool()) {
        return Calamares::JobResult::error(
            tr("Installation has not been confirmed."),
            QStringLiteral("RCVR_006: slm.target.confirmed not set"));
    }

    // Real execution: mount the recovery partition, populate the §4.1
    // directory tree, copy kernel + initrd from the staged root, write
    // the install manifest. The two big remaining items — recovery.squashfs
    // (dracut + custom busybox rootfs) and slm-factory.img.zst (btrfs send |
    // zstd of @factory-initial, subject to §4.4 disk-space policy) — are
    // explicit follow-ups; we mark them with TODO logs rather than
    // implementing them inline here.

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

    // 1. Mount the recovery partition at the scratch path.
    if (!QDir().mkpath(staging)) {
        return Calamares::JobResult::error(
            tr("Unable to prepare recovery staging directory."),
            QStringLiteral("RCVR_010: mkpath failed for %1").arg(staging));
    }
    auto mountRes = runCmd(QStringLiteral("mount"),
                           { recoveryDevice, staging });
    if (!mountRes.started || mountRes.exitCode != 0) {
        return fail(QStringLiteral("RCVR_010"),
                    tr("Unable to mount the recovery partition."), mountRes);
    }

    // Track populate failures as scalars so we always umount the recovery
    // partition before returning (JobResult is move-only — see
    // [[feedback-calamares-api]]).
    QString failCode;
    QString failMsg;
    CmdResult failRes;
    QString failDetail;

    auto recordFail = [&](const QString &code, const QString &msg,
                          const QString &detail) {
        if (failCode.isEmpty()) {
            failCode = code;
            failMsg = msg;
            failDetail = detail;
        }
    };

    do {
        // 2. Create the §4.1 directory tree under the mounted recovery.
        for (const QString &dir : kRecoveryDirs) {
            const QString target = staging + QLatin1Char('/') + dir;
            if (!QDir().mkpath(target)) {
                recordFail(QStringLiteral("RCVR_011"),
                           tr("Unable to prepare recovery directory."),
                           QStringLiteral("mkpath failed for %1").arg(target));
                break;
            }
        }
        if (!failCode.isEmpty()) break;

        // 3. Physical copy of kernel + initrd (§4.2 independence guarantee:
        // PHYSICAL copy, never a symlink — recovery boots without touching @).
        // QFile::copy follows symlinks (resolves to the actual file) which
        // is exactly the behaviour we want for /boot/vmlinuz → vmlinuz-N.
        if (QFile::exists(srcKernel)) {
            QFile::remove(dstKernel);  // QFile::copy refuses to overwrite
            if (!QFile::copy(srcKernel, dstKernel)) {
                recordFail(QStringLiteral("RCVR_012"),
                           tr("Unable to copy kernel to recovery partition."),
                           QStringLiteral("copy %1 → %2 failed")
                               .arg(srcKernel, dstKernel));
                break;
            }
        } else {
            cWarning() << "slm-recovery: kernel missing at" << srcKernel
                       << "(slm-deploy may not have populated /boot yet)";
        }

        if (QFile::exists(srcInitrd)) {
            QFile::remove(dstInitrd);
            if (!QFile::copy(srcInitrd, dstInitrd)) {
                recordFail(QStringLiteral("RCVR_013"),
                           tr("Unable to copy initrd to recovery partition."),
                           QStringLiteral("copy %1 → %2 failed")
                               .arg(srcInitrd, dstInitrd));
                break;
            }
        } else {
            cWarning() << "slm-recovery: initrd missing at" << srcInitrd;
        }

        // 4. Write the install-manifest.json (the dry-run path already
        // built `manifestBytes` for the GS preview — re-emitted here so the
        // on-disk file matches what the user saw on the summary screen).
        QFile manifestFile(manifestPath);
        if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            recordFail(QStringLiteral("RCVR_014"),
                       tr("Unable to write recovery install manifest."),
                       QStringLiteral("open(%1) failed: %2")
                           .arg(manifestPath, manifestFile.errorString()));
            break;
        }
        if (manifestFile.write(manifestBytes) != manifestBytes.size()) {
            const QString err = manifestFile.errorString();
            manifestFile.close();
            recordFail(QStringLiteral("RCVR_014"),
                       tr("Unable to write recovery install manifest."),
                       QStringLiteral("short write to %1: %2")
                           .arg(manifestPath, err));
            break;
        }
        manifestFile.close();

        // 5. Best-effort copy of protected-packages.json from the
        // post-install location slm-protected-packages writes to. If it
        // doesn't exist yet (slm-protected-packages may run after us
        // depending on settings.conf order) the copy is silently skipped —
        // recoveryd will fall back to /usr/share/slm/ at runtime anyway.
        if (!protectedSrc.isEmpty() && QFile::exists(protectedSrc)) {
            QFile::remove(protectedDst);
            QFile::copy(protectedSrc, protectedDst);
        }

        cDebug() << "slm-recovery: TODO recovery.squashfs build (needs dracut + custom rootfs)";
        cDebug() << "slm-recovery: TODO factory image generation (needs btrfs send | zstd of @factory-initial)";
    } while (false);

    // 6. Always unmount the recovery partition, even on populate failure.
    // Leaving it mounted would leak the scratch mount and might block a
    // later re-run.
    auto umountRes = runCmd(QStringLiteral("umount"), { staging });
    if (!umountRes.started || umountRes.exitCode != 0) {
        cWarning() << "slm-recovery: umount failed for" << staging
                   << "exit=" << umountRes.exitCode
                   << "out=" << umountRes.output.trimmed();
        // Don't promote umount failure to JobResult if populate succeeded —
        // the recovery data is on disk and the kernel will release the
        // mount on shutdown.
    }

    if (!failCode.isEmpty()) {
        return Calamares::JobResult::error(
            failMsg,
            QStringLiteral("%1: %2").arg(failCode, failDetail));
    }

    gs->insert(QStringLiteral("slm.recovery.executed"), true);
    cDebug() << "slm-recovery: real execution complete";
    return Calamares::JobResult::ok();
}

void SlmRecoveryJob::setConfigurationMap(const QVariantMap &configurationMap)
{
    if (configurationMap.contains(QStringLiteral("dry-run"))) {
        m_dryRun = configurationMap.value(QStringLiteral("dry-run")).toBool();
    }
}
