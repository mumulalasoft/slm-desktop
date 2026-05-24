#pragma once

#include <CppJob.h>
#include <utils/PluginFactory.h>

#include <QObject>
#include <QVariantMap>

class DLLEXPORT SlmFstabJob : public Calamares::CppJob
{
    Q_OBJECT
public:
    explicit SlmFstabJob(QObject *parent = nullptr);
    ~SlmFstabJob() override;

    QString prettyName() const override;
    Calamares::JobResult exec() override;
    void setConfigurationMap(const QVariantMap &configurationMap) override;

private:
    // Default-true safety: matches slm-partition / slm-btrfs-layout. The
    // dry-run path emits the fstab to the debug log with placeholder UUIDs
    // and writes nothing to disk.
    bool m_dryRun = true;
};

CALAMARES_PLUGIN_FACTORY_DECLARATION(SlmFstabJobFactory)
