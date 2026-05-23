#include "SlmHardwareProbeJob.h"

#include <utils/Logger.h>

CALAMARES_PLUGIN_FACTORY_DEFINITION(SlmHardwareProbeJobFactory, registerPlugin<SlmHardwareProbeJob>();)

SlmHardwareProbeJob::SlmHardwareProbeJob(QObject *parent)
    : Calamares::CppJob(parent)
{
}

SlmHardwareProbeJob::~SlmHardwareProbeJob() = default;

QString SlmHardwareProbeJob::prettyName() const
{
    return tr("Probe hardware compatibility");
}

Calamares::JobResult SlmHardwareProbeJob::exec()
{
    // TODO: implement per docs/SLM_INSTALLER_BACKEND.md §8.
    // Enumerate GPU vendor via lspci, detect Broadcom, evaluate Wayland
    // capability, read SecureBoot variable. Populate GlobalStorage with
    // slm.hw.gpu.*, slm.hw.broadcom.*, slm.hw.ram.mb, slm.hw.disk.smart_ok.
    return Calamares::JobResult::ok();
}

void SlmHardwareProbeJob::setConfigurationMap(const QVariantMap &configurationMap)
{
    Q_UNUSED(configurationMap)
}
