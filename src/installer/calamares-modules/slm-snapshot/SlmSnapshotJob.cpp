#include "SlmSnapshotJob.h"

#include <utils/Logger.h>

CALAMARES_PLUGIN_FACTORY_DEFINITION(SlmSnapshotJobFactory, registerPlugin<SlmSnapshotJob>();)

SlmSnapshotJob::SlmSnapshotJob(QObject *parent)
    : Calamares::CppJob(parent)
{
}

SlmSnapshotJob::~SlmSnapshotJob() = default;

QString SlmSnapshotJob::prettyName() const
{
    return tr("Create factory snapshot");
}

Calamares::JobResult SlmSnapshotJob::exec()
{
    // TODO: implement per docs/SLM_INSTALLER_BACKEND.md §7.
    // btrfs subvolume snapshot -r ... @factory-initial. Optionally emit
    // /recovery/factory/slm-factory.img.zst via btrfs send | zstd.
    return Calamares::JobResult::ok();
}

void SlmSnapshotJob::setConfigurationMap(const QVariantMap &configurationMap)
{
    Q_UNUSED(configurationMap)
}
