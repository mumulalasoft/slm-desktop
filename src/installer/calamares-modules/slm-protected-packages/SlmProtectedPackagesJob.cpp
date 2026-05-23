#include "SlmProtectedPackagesJob.h"

#include <utils/Logger.h>

CALAMARES_PLUGIN_FACTORY_DEFINITION(SlmProtectedPackagesJobFactory, registerPlugin<SlmProtectedPackagesJob>();)

SlmProtectedPackagesJob::SlmProtectedPackagesJob(QObject *parent)
    : Calamares::CppJob(parent)
{
}

SlmProtectedPackagesJob::~SlmProtectedPackagesJob() = default;

QString SlmProtectedPackagesJob::prettyName() const
{
    return tr("Record protected packages");
}

Calamares::JobResult SlmProtectedPackagesJob::exec()
{
    // TODO: implement per docs/SLM_INSTALLER_BACKEND.md §6.
    // Combine curated category list with dpkg-query output from staging root.
    // Emit /usr/share/slm/protected-packages.json and copy to
    // /recovery/metadata/protected-packages.json.
    return Calamares::JobResult::ok();
}

void SlmProtectedPackagesJob::setConfigurationMap(const QVariantMap &configurationMap)
{
    Q_UNUSED(configurationMap)
}
