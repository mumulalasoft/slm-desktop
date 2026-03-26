#pragma once

#include <QObject>
#include <QString>

// DeveloperModeController — persists Developer Mode and Experimental Features
// flags to ~/.config/slm/developer.conf.
//
// Exposed to QML as the "DeveloperMode" context property.
//
class DeveloperModeController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool developerMode       READ developerMode       WRITE setDeveloperMode
               NOTIFY developerModeChanged)
    Q_PROPERTY(bool experimentalFeatures READ experimentalFeatures WRITE setExperimentalFeatures
               NOTIFY experimentalFeaturesChanged)

public:
    explicit DeveloperModeController(QObject *parent = nullptr);

    bool developerMode() const;
    bool experimentalFeatures() const;

public slots:
    void setDeveloperMode(bool enabled);
    void setExperimentalFeatures(bool enabled);

signals:
    void developerModeChanged();
    void experimentalFeaturesChanged();

private:
    void load();
    void save();

    bool m_developerMode       = false;
    bool m_experimentalFeatures = false;
};
