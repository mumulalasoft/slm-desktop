#pragma once

#include <CppJob.h>
#include <utils/PluginFactory.h>

#include <QObject>
#include <QVariantMap>

class DLLEXPORT SlmDeployJob : public Calamares::CppJob
{
    Q_OBJECT
public:
    explicit SlmDeployJob(QObject *parent = nullptr);
    ~SlmDeployJob() override;

    QString prettyName() const override;
    Calamares::JobResult exec() override;
    void setConfigurationMap(const QVariantMap &configurationMap) override;

private:
    bool m_dryRun = true;
    QString m_squashfsSource;
};

CALAMARES_PLUGIN_FACTORY_DECLARATION(SlmDeployJobFactory)
