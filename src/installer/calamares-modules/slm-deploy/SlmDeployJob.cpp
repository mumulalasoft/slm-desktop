#include "SlmDeployJob.h"

#include <GlobalStorage.h>
#include <JobQueue.h>
#include <utils/Logger.h>

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QRegularExpressionMatchIterator>
#include <QString>
#include <QStringList>
#include <QThread>
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

    // Defense in depth — see slm-partition for the rationale.
    if (!gs->value(QStringLiteral("slm.target.confirmed")).toBool()) {
        return Calamares::JobResult::error(
            tr("Installation has not been confirmed."),
            QStringLiteral("DEPLOY_006: slm.target.confirmed not set"));
    }

    if (!sourceExists) {
        return Calamares::JobResult::error(
            tr("SLM Desktop image not found on the install medium."),
            QStringLiteral("DEPLOY_001: squashfs not found at %1").arg(source));
    }

    // Real execution: spawn unsquashfs, parse its stdout for percentage
    // updates as they come in, emit them through the Calamares Job
    // progress signal so the §6 progress screen shows determinate
    // movement during the multi-minute unpack.
    //
    // We use QProcess directly here rather than SlmCommand because we
    // need incremental stdout reads — SlmCommand buffers everything until
    // waitForFinished, which is fine for fast ops (sgdisk, blkid) but
    // would mean the progress bar sits at 0% for the entire 5–15 min run.

    const int parallelism = qMax(1, QThread::idealThreadCount());

    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);

    qreal lastEmitted = 0.0;
    static const QRegularExpression kPctRe(QStringLiteral("(\\d{1,3})\\s*%"));

    QObject::connect(&proc, &QProcess::readyReadStandardOutput,
                     this, [&proc, &lastEmitted, this]() {
        const QByteArray chunk = proc.readAllStandardOutput();
        if (chunk.isEmpty()) {
            return;
        }
        // unsquashfs writes a carriage-return-terminated progress line.
        // We scan the whole chunk and take the LAST percentage we see,
        // so backwards-jumps from any noise don't show up.
        const QString text = QString::fromUtf8(chunk);
        QRegularExpressionMatchIterator it = kPctRe.globalMatch(text);
        int lastPct = -1;
        while (it.hasNext()) {
            lastPct = it.next().captured(1).toInt();
        }
        if (lastPct < 0) {
            return;
        }
        const qreal pct = qBound(0.0, lastPct / 100.0, 1.0);
        // Emit only when the percentage moves by ≥1% — keeps the
        // viewmanager from being flooded by every 0.1% increment.
        if (pct > lastEmitted + 0.01) {
            lastEmitted = pct;
            emit progress(pct);
        }
    });

    cDebug() << "slm-deploy: unsquashfs -p" << parallelism
             << "-f -d" << rootMount << source;
    proc.start(QStringLiteral("unsquashfs"), {
        QStringLiteral("-p"), QString::number(parallelism),
        QStringLiteral("-f"),
        QStringLiteral("-d"), rootMount,
        source,
    });
    if (!proc.waitForStarted(5000)) {
        return Calamares::JobResult::error(
            tr("Unable to start filesystem deployment."),
            QStringLiteral("DEPLOY_010: unsquashfs failed to start"));
    }

    // No hard timeout — unsquashfs on a slow USB / large image can take
    // 10+ minutes legitimately. -1 means "wait forever". The installer's
    // own UI provides the cancel affordance.
    if (!proc.waitForFinished(-1)) {
        proc.kill();
        proc.waitForFinished(5000);
        return Calamares::JobResult::error(
            tr("Filesystem deployment was interrupted."),
            QStringLiteral("DEPLOY_011: unsquashfs killed"));
    }

    const QString trailing = QString::fromUtf8(proc.readAll()).trimmed();
    if (proc.exitCode() != 0) {
        return Calamares::JobResult::error(
            tr("Filesystem deployment failed."),
            QStringLiteral("DEPLOY_012: unsquashfs exit=%1 output=%2")
                .arg(proc.exitCode()).arg(trailing));
    }

    // Pin the progress bar at 100% in case unsquashfs's final percentage
    // line didn't quite reach 100% before exit.
    emit progress(1.0);

    gs->insert(QStringLiteral("slm.deploy.executed"), true);
    cDebug() << "slm-deploy: real execution complete";
    return Calamares::JobResult::ok();
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
