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
};

CALAMARES_PLUGIN_FACTORY_DECLARATION(SlmPartitionJobFactory)
