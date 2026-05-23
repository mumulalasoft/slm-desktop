#include "SlmRecoveryJob.h"

#include <utils/Logger.h>

CALAMARES_PLUGIN_FACTORY_DEFINITION(SlmRecoveryJobFactory, registerPlugin<SlmRecoveryJob>();)

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
    // TODO: implement per docs/SLM_INSTALLER_BACKEND.md §4.
    // Copy kernel + initrd to /recovery/boot/, populate recovery.squashfs,
    // write install-manifest.json. Independence: never symlink into @.
    return Calamares::JobResult::ok();
}

void SlmRecoveryJob::setConfigurationMap(const QVariantMap &configurationMap)
{
    Q_UNUSED(configurationMap)
}
