#include "SlmFstabJob.h"

#include <utils/Logger.h>

CALAMARES_PLUGIN_FACTORY_DEFINITION(SlmFstabJobFactory, registerPlugin<SlmFstabJob>();)

SlmFstabJob::SlmFstabJob(QObject *parent)
    : Calamares::CppJob(parent)
{
}

SlmFstabJob::~SlmFstabJob() = default;

QString SlmFstabJob::prettyName() const
{
    return tr("Generate /etc/fstab");
}

Calamares::JobResult SlmFstabJob::exec()
{
    // TODO: implement per docs/SLM_INSTALLER_BACKEND.md §2.
    // Emit /etc/fstab using UUID= references for each Btrfs subvolume and
    // the ESP. Always include the "DO NOT EDIT MANUALLY" banner.
    return Calamares::JobResult::ok();
}

void SlmFstabJob::setConfigurationMap(const QVariantMap &configurationMap)
{
    Q_UNUSED(configurationMap)
}
