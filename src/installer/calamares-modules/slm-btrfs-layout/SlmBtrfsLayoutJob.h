#pragma once

#include <CppJob.h>
#include <utils/PluginFactory.h>

#include <QObject>
#include <QVariantMap>

class DLLEXPORT SlmBtrfsLayoutJob : public Calamares::CppJob
{
    Q_OBJECT
public:
    explicit SlmBtrfsLayoutJob(QObject *parent = nullptr);
    ~SlmBtrfsLayoutJob() override;

    QString prettyName() const override;
    Calamares::JobResult exec() override;
    void setConfigurationMap(const QVariantMap &configurationMap) override;

private:
    // Default-true safety: matches slm-partition. Flip via `dry-run: false`
    // in the module config once the upstream pipeline (slm-partition real
    // execution + scratch-mount setup) is wired up.
    bool m_dryRun = true;
};

CALAMARES_PLUGIN_FACTORY_DECLARATION(SlmBtrfsLayoutJobFactory)
