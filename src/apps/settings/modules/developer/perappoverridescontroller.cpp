#include "perappoverridescontroller.h"
#include "envserviceclient.h"

PerAppOverridesController::PerAppOverridesController(EnvServiceClient *client,
                                                       QObject          *parent)
    : QObject(parent)
    , m_client(client)
{
    connect(m_client, &EnvServiceClient::serviceAvailableChanged,
            this, &PerAppOverridesController::refresh);
    connect(m_client, &EnvServiceClient::appVarsChanged,
            this, [this](const QString &appId) {
                if (appId == m_selectedAppId)
                    reloadAppVars();
                reloadAppList();
            });
}

QStringList PerAppOverridesController::appList() const
{
    return m_appList;
}

QString PerAppOverridesController::selectedAppId() const
{
    return m_selectedAppId;
}

QVariantList PerAppOverridesController::appVars() const
{
    return m_appVars;
}

QString PerAppOverridesController::lastError() const
{
    return m_lastError;
}

void PerAppOverridesController::setSelectedAppId(const QString &appId)
{
    if (m_selectedAppId == appId)
        return;
    m_selectedAppId = appId;
    emit selectedAppIdChanged();
    reloadAppVars();
}

bool PerAppOverridesController::addVar(const QString &key, const QString &value,
                                        const QString &mergeMode)
{
    m_lastError.clear();
    if (m_selectedAppId.isEmpty()) {
        m_lastError = tr("No application selected.");
        emit lastErrorChanged();
        return false;
    }
    const bool ok = m_client->addAppVar(m_selectedAppId, key, value, mergeMode);
    if (!ok) {
        m_lastError = m_client->lastError();
        emit lastErrorChanged();
    }
    return ok;
}

bool PerAppOverridesController::removeVar(const QString &key)
{
    m_lastError.clear();
    if (m_selectedAppId.isEmpty()) {
        m_lastError = tr("No application selected.");
        emit lastErrorChanged();
        return false;
    }
    const bool ok = m_client->removeAppVar(m_selectedAppId, key);
    if (!ok) {
        m_lastError = m_client->lastError();
        emit lastErrorChanged();
    }
    return ok;
}

void PerAppOverridesController::refresh()
{
    reloadAppList();
    reloadAppVars();
}

void PerAppOverridesController::reloadAppList()
{
    m_appList = m_client->serviceAvailable() ? m_client->listAppsWithOverrides() : QStringList{};
    emit appListChanged();
}

void PerAppOverridesController::reloadAppVars()
{
    if (m_selectedAppId.isEmpty() || !m_client->serviceAvailable()) {
        m_appVars.clear();
        emit appVarsChanged();
        return;
    }
    m_appVars = m_client->getAppVars(m_selectedAppId);
    emit appVarsChanged();
}
