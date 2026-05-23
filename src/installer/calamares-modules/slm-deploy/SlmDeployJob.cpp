#include "SlmDeployJob.h"

#include <GlobalStorage.h>
#include <JobQueue.h>
#include <utils/Logger.h>

#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QVariantMap>

CALAMARES_PLUGIN_FACTORY_DEFINITION(SlmDeployJobFactory, registerPlugin<SlmDeployJob>();)

namespace {

// §12 leaves the source path open (live-build base vs snapshot of installed
// system). The Ubuntu casper convention is the safe default; settings.conf
// can override via `squashfs-source: /path/to/filesystem.squashfs`.
constexpr const char *kDefaultSquashfsSource = "/cdrom/casper/filesystem.squashfs";

QString resolveRootMountPoint(Calamares::GlobalStorage *gs)
{
    const QString fromGs = gs->value(QStringLiteral("rootMountPoint")).toString();
    return fromGs.isEmpty() ? QStringLiteral("/mnt/slm-install-root") : fromGs;
}

} // namespace

SlmDeployJob::SlmDeployJob(QObject *parent)
    : Calamares::CppJob(parent)
    , m_squashfsSource(QString::fromLatin1(kDefaultSquashfsSource))
{
}

SlmDeployJob::~SlmDeployJob() = default;

QString SlmDeployJob::prettyName() const
{
    return tr("Deploy SLM Desktop files");
}

Calamares::JobResult SlmDeployJob::exec()
{
    auto *gs = Calamares::JobQueue::instanceGlobalStorage();
    if (!gs) {
        return Calamares::JobResult::error(
            tr("Installer state unavailable."),
            QStringLiteral("DEPLOY_001: GlobalStorage missing"));
    }

    const QString rootMount = resolveRootMountPoint(gs);
    const QString source = m_squashfsSource;

    qint64 sourceBytes = -1;
    bool sourceExists = false;
    {
        QFileInfo info(source);
        sourceExists = info.exists() && info.isFile();
        if (sourceExists) {
            sourceBytes = info.size();
        }
    }

    gs->insert(QStringLiteral("slm.deploy.squashfs_source"), source);
    gs->insert(QStringLiteral("slm.deploy.target_root"), rootMount);
    gs->insert(QStringLiteral("slm.deploy.source_exists"), sourceExists);
    if (sourceBytes >= 0) {
        gs->insert(QStringLiteral("slm.deploy.source_bytes"), QVariant::fromValue(sourceBytes));
    }

    cDebug() << "slm-deploy:"
             << "source=" << source
             << "exists=" << sourceExists
             << "size_bytes=" << sourceBytes
             << "target=" << rootMount
             << "dry_run=" << m_dryRun;

    if (m_dryRun) {
        // Dry-run does NOT require the source to exist — disk-select harness
        // tests run before any ISO mount, and rejecting them on a missing
        // squashfs would block the rest of the pipeline previews.
        cDebug() << "slm-deploy: DRY RUN — would run:";
        cDebug() << "  unsquashfs -p" << QString::number(4)
                 << "-f -d" << rootMount << source;
        cDebug() << "  # progress reported via stdout '[ N%]' → Calamares progress signal";
        gs->insert(QStringLiteral("slm.deploy.dry_run"), true);
        return Calamares::JobResult::ok();
    }

    if (!sourceExists) {
        return Calamares::JobResult::error(
            tr("SLM Desktop image not found on the install medium."),
            QStringLiteral("DEPLOY_001: squashfs not found at %1").arg(source));
    }

    // Real path stays gated: this is the heaviest module (multi-minute
    // unsquashfs, determinate progress reporting via Calamares' progress
    // signal, rollback via `rm -rf $ROOT/*` per §5.3). Wire up alongside the
    // disk-select UI follow-up so the destructive prefix lands together.
    return Calamares::JobResult::error(
        tr("Filesystem deployment is not yet implemented."),
        QStringLiteral("DEPLOY_005: real-execution path pending disk-select UI"));
}

void SlmDeployJob::setConfigurationMap(const QVariantMap &configurationMap)
{
    if (configurationMap.contains(QStringLiteral("dry-run"))) {
        m_dryRun = configurationMap.value(QStringLiteral("dry-run")).toBool();
    }
    if (configurationMap.contains(QStringLiteral("squashfs-source"))) {
        const QString candidate = configurationMap.value(QStringLiteral("squashfs-source"))
                                      .toString().trimmed();
        if (!candidate.isEmpty()) {
            m_squashfsSource = candidate;
        }
    }
}
