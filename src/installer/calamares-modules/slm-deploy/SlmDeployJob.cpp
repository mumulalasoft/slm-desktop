#include "SlmDeployJob.h"

#include <utils/Logger.h>

CALAMARES_PLUGIN_FACTORY_DEFINITION(SlmDeployJobFactory, registerPlugin<SlmDeployJob>();)

SlmDeployJob::SlmDeployJob(QObject *parent)
    : Calamares::CppJob(parent)
{
}

SlmDeployJob::~SlmDeployJob() = default;

QString SlmDeployJob::prettyName() const
{
    return tr("Deploy SLM Desktop files");
}

Calamares::JobResult SlmDeployJob::exec()
{
    // TODO: implement per docs/SLM_INSTALLER_BACKEND.md §5.
    // unsquashfs the SLM rootfs into /mnt/slm-install-root, reporting
    // determinate progress through Calamares progress signals.
    return Calamares::JobResult::ok();
}

void SlmDeployJob::setConfigurationMap(const QVariantMap &configurationMap)
{
    Q_UNUSED(configurationMap)
}
