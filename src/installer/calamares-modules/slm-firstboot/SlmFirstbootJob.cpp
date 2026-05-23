#include "SlmFirstbootJob.h"

#include <utils/Logger.h>

CALAMARES_PLUGIN_FACTORY_DEFINITION(SlmFirstbootJobFactory, registerPlugin<SlmFirstbootJob>();)

SlmFirstbootJob::SlmFirstbootJob(QObject *parent)
    : Calamares::CppJob(parent)
{
}

SlmFirstbootJob::~SlmFirstbootJob() = default;

QString SlmFirstbootJob::prettyName() const
{
    return tr("Prepare first boot");
}

Calamares::JobResult SlmFirstbootJob::exec()
{
    // TODO: implement per docs/SLM_INSTALLER_BACKEND.md §7.
    // Write /var/lib/slm/firstboot_pending JSON with install_id UUID.
    // session-broker reads this marker post-reboot to launch slm-welcome.
    return Calamares::JobResult::ok();
}

void SlmFirstbootJob::setConfigurationMap(const QVariantMap &configurationMap)
{
    Q_UNUSED(configurationMap)
}
