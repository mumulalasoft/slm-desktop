#include "processservicescontroller.h"

ProcessServicesController::ProcessServicesController(SvcManagerClient *client, QObject *parent)
    : QObject(parent)
    , m_client(client)
{
    connect(m_client, &SvcManagerClient::serviceAvailableChanged,
            this, &ProcessServicesController::onServiceAvailableChanged);
    connect(m_client, &SvcManagerClient::componentStateChanged,
            this, &ProcessServicesController::onComponentStateChanged);

    if (m_client->serviceAvailable())
        refresh();
}

bool ProcessServicesController::serviceAvailable() const
{
    return m_client->serviceAvailable();
}

void ProcessServicesController::refresh()
{
    m_lastError.clear();
    if (!m_client->serviceAvailable()) {
        m_components.clear();
        emit componentsChanged();
        return;
    }
    m_components = m_client->listComponents();
    emit componentsChanged();
}

bool ProcessServicesController::restart(const QString &name)
{
    m_lastError.clear();
    const bool ok = m_client->restartComponent(name);
    if (!ok) {
        m_lastError = m_client->lastError();
        emit lastErrorChanged();
    }
    return ok;
}

bool ProcessServicesController::stop(const QString &name)
{
    m_lastError.clear();
    const bool ok = m_client->stopComponent(name);
    if (!ok) {
        m_lastError = m_client->lastError();
        emit lastErrorChanged();
    }
    return ok;
}

void ProcessServicesController::onServiceAvailableChanged()
{
    emit serviceAvailableChanged();
    if (m_client->serviceAvailable())
        refresh();
    else {
        m_components.clear();
        emit componentsChanged();
    }
}

void ProcessServicesController::onComponentStateChanged(const QString &name,
                                                         const QString &newStatus)
{
    updateComponentStatus(name, newStatus);
}

void ProcessServicesController::updateComponentStatus(const QString &name,
                                                       const QString &status)
{
    for (int i = 0; i < m_components.size(); ++i) {
        QVariantMap m = m_components[i].toMap();
        if (m.value(QStringLiteral("name")).toString() == name) {
            m[QStringLiteral("status")] = status;
            m_components[i] = m;
            emit componentsChanged();
            return;
        }
    }
    // Component not in list yet — do a full refresh.
    refresh();
}
