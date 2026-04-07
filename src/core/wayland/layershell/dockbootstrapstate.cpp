#include "dockbootstrapstate.h"

#include <QDebug>

DockBootstrapState::DockBootstrapState(QObject *parent)
    : QObject(parent)
{
}

bool DockBootstrapState::integrationEnabled() const
{
    return m_integrationEnabled;
}

bool DockBootstrapState::layerRoleBound() const
{
    return m_layerRoleBound;
}

bool DockBootstrapState::firstConfigureReceived() const
{
    return m_firstConfigureReceived;
}

bool DockBootstrapState::configureAcked() const
{
    return m_configureAcked;
}

bool DockBootstrapState::readyToRender() const
{
    return m_readyToRender;
}

bool DockBootstrapState::visibleToUser() const
{
    return m_visibleToUser;
}

void DockBootstrapState::reset()
{
    const bool prevIntegration = m_integrationEnabled;
    const bool prevLayerRole = m_layerRoleBound;
    const bool prevFirstConfigure = m_firstConfigureReceived;
    const bool prevAcked = m_configureAcked;
    const bool prevVisible = m_visibleToUser;

    m_integrationEnabled = false;
    m_layerRoleBound = false;
    m_firstConfigureReceived = false;
    m_configureAcked = false;
    m_visibleToUser = false;

    if (prevIntegration) emit integrationEnabledChanged();
    if (prevLayerRole) emit layerRoleBoundChanged();
    if (prevFirstConfigure) emit firstConfigureReceivedChanged();
    if (prevAcked) emit configureAckedChanged();
    if (prevVisible) emit visibleToUserChanged();
    refreshReadyToRender();
}

void DockBootstrapState::setIntegrationEnabled(bool enabled)
{
    if (m_integrationEnabled == enabled) {
        return;
    }
    m_integrationEnabled = enabled;
    emit integrationEnabledChanged();
    if (enabled) {
        qInfo().noquote() << QStringLiteral("DOCK_BOOTSTRAP integration enabled");
    } else {
        qWarning().noquote() << QStringLiteral("DOCK_BOOTSTRAP integration unavailable");
    }
    refreshReadyToRender();
}

void DockBootstrapState::setLayerRoleBound(bool bound)
{
    if (m_layerRoleBound == bound) {
        return;
    }
    m_layerRoleBound = bound;
    emit layerRoleBoundChanged();
    if (bound) {
        qInfo().noquote() << QStringLiteral("DOCK_BOOTSTRAP layer role bound");
    }
    refreshReadyToRender();
}

void DockBootstrapState::markFirstConfigureReceived(quint32 serial)
{
    if (m_firstConfigureReceived) {
        return;
    }
    m_firstConfigureReceived = true;
    emit firstConfigureReceivedChanged();
    qInfo().noquote() << QStringLiteral("DOCK_BOOTSTRAP first configure received serial=%1")
                             .arg(serial);
    refreshReadyToRender();
}

void DockBootstrapState::markConfigureAcked(quint32 serial)
{
    if (m_configureAcked) {
        return;
    }
    m_configureAcked = true;
    emit configureAckedChanged();
    qInfo().noquote() << QStringLiteral("DOCK_BOOTSTRAP configure acked serial=%1")
                             .arg(serial);
    refreshReadyToRender();
}

void DockBootstrapState::setVisibleToUser(bool visible)
{
    if (m_visibleToUser == visible) {
        return;
    }
    m_visibleToUser = visible;
    emit visibleToUserChanged();
    qInfo().noquote() << QStringLiteral("DOCK_BOOTSTRAP visible=%1")
                             .arg(visible ? QStringLiteral("true")
                                          : QStringLiteral("false"));
}

void DockBootstrapState::refreshReadyToRender()
{
    const bool nextReady = m_integrationEnabled
                           && m_layerRoleBound
                           && m_firstConfigureReceived
                           && m_configureAcked;
    if (m_readyToRender == nextReady) {
        return;
    }
    m_readyToRender = nextReady;
    emit readyToRenderChanged();
    qInfo().noquote() << QStringLiteral("DOCK_BOOTSTRAP readyToRender=%1")
                             .arg(m_readyToRender ? QStringLiteral("true")
                                                  : QStringLiteral("false"));
}
