#include "SlmBootloaderJob.h"

#include <utils/Logger.h>

CALAMARES_PLUGIN_FACTORY_DEFINITION(SlmBootloaderJobFactory, registerPlugin<SlmBootloaderJob>();)

SlmBootloaderJob::SlmBootloaderJob(QObject *parent)
    : Calamares::CppJob(parent)
{
}

SlmBootloaderJob::~SlmBootloaderJob() = default;

QString SlmBootloaderJob::prettyName() const
{
    return tr("Install systemd-boot");
}

Calamares::JobResult SlmBootloaderJob::exec()
{
    // TODO: implement per docs/SLM_INSTALLER_BACKEND.md §3.
    // bootctl install --no-variables inside chroot, then write efibootmgr
    // entries from live env. Generate loader.conf and the three entries:
    // SLM Desktop, SLM Recovery, SLM Safe Mode. Persist cmdline at
    // /etc/kernel/cmdline for future UKI/TPM2 support.
    return Calamares::JobResult::ok();
}

void SlmBootloaderJob::setConfigurationMap(const QVariantMap &configurationMap)
{
    Q_UNUSED(configurationMap)
}
