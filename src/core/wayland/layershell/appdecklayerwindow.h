#pragma once

#include <QQuickWindow>
#include <QTimer>

class WlrLayerShell;

// AppDeckLayerWindow — persistent QQuickWindow that registers as a wlr-layer-shell surface.
//
// Properties:
//  - layer: TOP (default) keeps appdeck above regular windows and AppHub
//  - anchor: BOTTOM (default) pins appdeck to screen bottom
//  - exclusiveZone: appdeck height in px (compositor reserves this zone)
//
// Created once at shell start, never destroyed.
// All QML AppDeck rendering happens inside this window.
class AppDeckLayerWindow : public QQuickWindow
{
    Q_OBJECT
    Q_PROPERTY(bool layerConfigured READ isLayerConfigured NOTIFY layerConfiguredChanged)
    Q_PROPERTY(int exclusiveZone READ exclusiveZone WRITE setExclusiveZone
                   NOTIFY exclusiveZoneChanged)
    Q_PROPERTY(bool layerShellSupported READ isLayerShellSupported CONSTANT)

public:
    explicit AppDeckLayerWindow(WlrLayerShell *layerShell, QWindow *parent = nullptr);
    ~AppDeckLayerWindow() override;

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
