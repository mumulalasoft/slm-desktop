#include "developermode.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

static QString configPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
           + QStringLiteral("/slm/developer.conf");
}

DeveloperModeController::DeveloperModeController(QObject *parent)
    : QObject(parent)
{
    load();
}

bool DeveloperModeController::developerMode() const
{
    return m_developerMode;
}

bool DeveloperModeController::experimentalFeatures() const
{
    return m_experimentalFeatures;
}

void DeveloperModeController::setDeveloperMode(bool enabled)
{
    if (m_developerMode == enabled)
        return;
    m_developerMode = enabled;
    // Turning off developer mode also disables experimental features.
    if (!enabled && m_experimentalFeatures) {
        m_experimentalFeatures = false;
        emit experimentalFeaturesChanged();
    }
    save();
    emit developerModeChanged();
}

void DeveloperModeController::setExperimentalFeatures(bool enabled)
{
    if (m_experimentalFeatures == enabled)
        return;
    // Experimental features require developer mode to be on.
    if (enabled && !m_developerMode)
        return;
    m_experimentalFeatures = enabled;
    save();
    emit experimentalFeaturesChanged();
}

void DeveloperModeController::load()
{
    QSettings s(configPath(), QSettings::IniFormat);
    m_developerMode        = s.value(QStringLiteral("developerMode"), false).toBool();
    m_experimentalFeatures = s.value(QStringLiteral("experimentalFeatures"), false).toBool();
    // Safety: experimentalFeatures can only be true when developerMode is true.
    if (!m_developerMode)
        m_experimentalFeatures = false;
}

void DeveloperModeController::save()
{
    const QString path = configPath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSettings s(path, QSettings::IniFormat);
    s.setValue(QStringLiteral("developerMode"), m_developerMode);
    s.setValue(QStringLiteral("experimentalFeatures"), m_experimentalFeatures);
    s.sync();
}
