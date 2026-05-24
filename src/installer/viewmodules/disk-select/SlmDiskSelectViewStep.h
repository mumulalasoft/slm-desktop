#pragma once

#include "Config.h"

#include <utils/PluginFactory.h>
#include <viewpages/ViewStep.h>

#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantMap>

class QQuickWidget;

class DLLEXPORT SlmDiskSelectViewStep : public Calamares::ViewStep
{
    Q_OBJECT
public:
    explicit SlmDiskSelectViewStep(QObject *parent = nullptr);
    ~SlmDiskSelectViewStep() override;

    QString prettyName() const override;
    QWidget *widget() override;

    bool isNextEnabled() const override;
    bool isBackEnabled() const override;
    bool isAtBeginning() const override;
    bool isAtEnd() const override;

    Calamares::JobList jobs() const override;

    void onActivate() override;
    void setConfigurationMap(const QVariantMap &configurationMap) override;

private:
    void emitNextStatus();

    Slm::Installer::DiskSelectConfig m_config;
    QPointer<QQuickWidget> m_widget;
};

CALAMARES_PLUGIN_FACTORY_DECLARATION(SlmDiskSelectViewStepFactory)
