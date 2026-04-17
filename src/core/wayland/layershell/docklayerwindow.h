#pragma once

#include <QQuickWindow>
#include <QTimer>

class WlrLayerShell;

// DockLayerWindow — persistent QQuickWindow that registers as a wlr-layer-shell surface.
//
// Properties:
//  - layer: TOP (default) keeps dock above regular windows and Launchpad
//  - anchor: BOTTOM (default) pins dock to screen bottom
//  - exclusiveZone: dock height in px (compositor reserves this zone)
//
// Created once at shell start, never destroyed.
// All QML Dock rendering happens inside this window.
class DockLayerWindow : public QQuickWindow
{
    Q_OBJECT
    Q_PROPERTY(bool layerConfigured READ isLayerConfigured NOTIFY layerConfiguredChanged)
    Q_PROPERTY(int exclusiveZone READ exclusiveZone WRITE setExclusiveZone
                   NOTIFY exclusiveZoneChanged)
    Q_PROPERTY(bool layerShellSupported READ isLayerShellSupported CONSTANT)

public:
    explicit DockLayerWindow(WlrLayerShell *layerShell, QWindow *parent = nullptr);
    ~DockLayerWindow() override;

    bool isLayerConfigured() const;
    bool isLayerShellSupported() const;
    int exclusiveZone() const;
    void setExclusiveZone(int zone);

signals:
    void layerConfiguredChanged();
    void exclusiveZoneChanged();

protected:
    void exposeEvent(QExposeEvent *event) override;

private slots:
    void tryConfigureLayerSurface();

private:
    WlrLayerShell *m_layerShell;
    bool           m_layerConfigured = false;
    bool           m_configureAttempted = false;
    int            m_exclusiveZone = 0;
    QTimer         m_retryTimer;
};
