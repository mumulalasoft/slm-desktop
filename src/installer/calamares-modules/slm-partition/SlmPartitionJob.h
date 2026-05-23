#pragma once

#include <CppJob.h>
#include <utils/PluginFactory.h>

#include <QObject>
#include <QVariantMap>

class DLLEXPORT SlmPartitionJob : public Calamares::CppJob
{
    Q_OBJECT
public:
    explicit SlmPartitionJob(QObject *parent = nullptr);
    ~SlmPartitionJob() override;

    QString prettyName() const override;
    Calamares::JobResult exec() override;
    void setConfigurationMap(const QVariantMap &configurationMap) override;

private:
    // Default-true safety: even if settings.conf forgets the key, we never
    // actually wipe a disk. Flip to false explicitly via `dry-run: false`
    // in the module config once the disk-select UI is live.
    bool m_dryRun = true;
};

CALAMARES_PLUGIN_FACTORY_DECLARATION(SlmPartitionJobFactory)
