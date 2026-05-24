#pragma once

#include <CppJob.h>
#include <utils/PluginFactory.h>

#include <QObject>
#include <QVariantMap>

class DLLEXPORT SlmBootloaderJob : public Calamares::CppJob
{
    Q_OBJECT
public:
    explicit SlmBootloaderJob(QObject *parent = nullptr);
    ~SlmBootloaderJob() override;

    QString prettyName() const override;
    Calamares::JobResult exec() override;
    void setConfigurationMap(const QVariantMap &configurationMap) override;

private:
    bool m_dryRun = true;
};

CALAMARES_PLUGIN_FACTORY_DECLARATION(SlmBootloaderJobFactory)
