#include "SlmBtrfsLayoutJob.h"

#include <utils/Logger.h>

CALAMARES_PLUGIN_FACTORY_DEFINITION(SlmBtrfsLayoutJobFactory, registerPlugin<SlmBtrfsLayoutJob>();)

SlmBtrfsLayoutJob::SlmBtrfsLayoutJob(QObject *parent)
    : Calamares::CppJob(parent)
{
}

SlmBtrfsLayoutJob::~SlmBtrfsLayoutJob() = default;

QString SlmBtrfsLayoutJob::prettyName() const
{
    return tr("Create Btrfs subvolumes");
}

Calamares::JobResult SlmBtrfsLayoutJob::exec()
{
    // TODO: implement per docs/SLM_INSTALLER_BACKEND.md §2.
    // Create subvolumes @, @home, @var, @log, @cache, @snapshots, @resources,
    // @recovery-staging. Mount with compress=zstd:3,noatime,ssd,space_cache=v2.
    return Calamares::JobResult::ok();
}

void SlmBtrfsLayoutJob::setConfigurationMap(const QVariantMap &configurationMap)
{
    Q_UNUSED(configurationMap)
}
