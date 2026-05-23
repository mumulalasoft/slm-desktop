#include "SlmUefiCheckJob.h"

#include <utils/Logger.h>

CALAMARES_PLUGIN_FACTORY_DEFINITION(SlmUefiCheckJobFactory, registerPlugin<SlmUefiCheckJob>();)

SlmUefiCheckJob::SlmUefiCheckJob(QObject *parent)
    : Calamares::CppJob(parent)
{
}

SlmUefiCheckJob::~SlmUefiCheckJob() = default;

QString SlmUefiCheckJob::prettyName() const
{
    return tr("Validate UEFI firmware");
}

Calamares::JobResult SlmUefiCheckJob::exec()
{
    // TODO: implement per docs/SLM_INSTALLER_BACKEND.md §3.
    // Check /sys/firmware/efi, efivars writable, ESP capacity, Secure Boot
    // state, RAM minimum, and write results to GlobalStorage under
    // keys slm.hw.uefi.*, slm.hw.blocks, slm.hw.warnings.
    return Calamares::JobResult::ok();
}

void SlmUefiCheckJob::setConfigurationMap(const QVariantMap &configurationMap)
{
    Q_UNUSED(configurationMap)
}
