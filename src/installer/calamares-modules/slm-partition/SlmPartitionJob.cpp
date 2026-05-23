#include "SlmPartitionJob.h"

#include <utils/Logger.h>

CALAMARES_PLUGIN_FACTORY_DEFINITION(SlmPartitionJobFactory, registerPlugin<SlmPartitionJob>();)

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
    // TODO: implement per docs/SLM_INSTALLER_BACKEND.md §2.
    // sgdisk --zap-all, create GPT with ESP (FAT32, 1GB) + Recovery (ext4, 8GB)
    // + Root (Btrfs, remainder). Respect slm.esp.reuse from GlobalStorage.
    return Calamares::JobResult::ok();
}

void SlmPartitionJob::setConfigurationMap(const QVariantMap &configurationMap)
{
    Q_UNUSED(configurationMap)
}
