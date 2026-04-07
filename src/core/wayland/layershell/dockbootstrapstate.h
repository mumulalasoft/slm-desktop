#pragma once

#include <QObject>

class DockBootstrapState : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool integrationEnabled READ integrationEnabled NOTIFY integrationEnabledChanged)
    Q_PROPERTY(bool layerRoleBound READ layerRoleBound NOTIFY layerRoleBoundChanged)
    Q_PROPERTY(bool firstConfigureReceived READ firstConfigureReceived NOTIFY firstConfigureReceivedChanged)
    Q_PROPERTY(bool configureAcked READ configureAcked NOTIFY configureAckedChanged)
    Q_PROPERTY(bool readyToRender READ readyToRender NOTIFY readyToRenderChanged)
    Q_PROPERTY(bool visibleToUser READ visibleToUser WRITE setVisibleToUser NOTIFY visibleToUserChanged)

public:
    explicit DockBootstrapState(QObject *parent = nullptr);

    bool integrationEnabled() const;
    bool layerRoleBound() const;
    bool firstConfigureReceived() const;
    bool configureAcked() const;
    bool readyToRender() const;
    bool visibleToUser() const;

    Q_INVOKABLE void reset();
    Q_INVOKABLE void setIntegrationEnabled(bool enabled);
    Q_INVOKABLE void setLayerRoleBound(bool bound);
    Q_INVOKABLE void markFirstConfigureReceived(quint32 serial);
    Q_INVOKABLE void markConfigureAcked(quint32 serial);
    Q_INVOKABLE void setVisibleToUser(bool visible);

signals:
    void integrationEnabledChanged();
    void layerRoleBoundChanged();
    void firstConfigureReceivedChanged();
    void configureAckedChanged();
    void readyToRenderChanged();
    void visibleToUserChanged();

private:
    void refreshReadyToRender();

    bool m_integrationEnabled = false;
    bool m_layerRoleBound = false;
    bool m_firstConfigureReceived = false;
    bool m_configureAcked = false;
    bool m_readyToRender = false;
    bool m_visibleToUser = false;
};
