#pragma once

#include <CppJob.h>
#include <utils/PluginFactory.h>

#include <QObject>
#include <QVariantMap>

class DLLEXPORT SlmFirstbootJob : public Calamares::CppJob
{
    Q_OBJECT
public:
    explicit SlmFirstbootJob(QObject *parent = nullptr);
    ~SlmFirstbootJob() override;

    QString prettyName() const override;
    Calamares::JobResult exec() override;
    void setConfigurationMap(const QVariantMap &configurationMap) override;
};

CALAMARES_PLUGIN_FACTORY_DECLARATION(SlmFirstbootJobFactory)
