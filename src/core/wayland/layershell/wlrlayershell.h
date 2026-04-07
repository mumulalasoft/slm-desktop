#pragma once

#include "qwayland-wlr-layer-shell-unstable-v1.h"

#include <QWaylandClientExtensionTemplate>
#include <QObject>
#include <QWindow>

class DockBootstrapState;

// ── WlrLayerShell ─────────────────────────────────────────────────────────────
// Global singleton binding to zwlr_layer_shell_v1.
// Registered as QML singleton "WlrLayerShell".
class WlrLayerShell : public QWaylandClientExtensionTemplate<WlrLayerShell>,
                      public QtWayland::zwlr_layer_shell_v1
{
    Q_OBJECT

public:
    explicit WlrLayerShell();

    Q_INVOKABLE bool isSupported() const;

    // Create a layer surface for the given window.
    // Returns true if the surface was configured successfully.
    // Must be called after the window's native surface is created.
    Q_INVOKABLE bool configureAsLayerSurface(QWindow *window,
                                             int layer,        // zwlr_layer_shell_v1_layer enum
                                             int anchors,      // zwlr_layer_surface_v1_anchor bitmask
                                             int exclusiveZone, // px, -1 = stretch
                                             const QString &nameSpace);

    // Update the exclusive zone of an already-configured layer surface.
    // Call this whenever the dock height changes so the compositor keeps the
    // correct reservation at the bottom of the screen.
    Q_INVOKABLE bool setExclusiveZone(QWindow *window, int exclusiveZone);
    void setDockBootstrapState(DockBootstrapState *state);

    // Layer constants (mirrors zwlr_layer_shell_v1_layer).
    enum Layer {
        LayerBackground = 0,
        LayerBottom     = 1,
        LayerTop        = 2,
        LayerOverlay    = 3,
    };
    Q_ENUM(Layer)

    // Anchor bitmask constants.
    enum Anchor {
        AnchorTop    = 1,
        AnchorBottom = 2,
        AnchorLeft   = 4,
        AnchorRight  = 8,
    };
    Q_ENUM(Anchor)

private:
    DockBootstrapState *m_dockBootstrapState = nullptr;
};

// ── WlrLayerSurfaceV1 ─────────────────────────────────────────────────────────
// Per-window layer surface configuration.
// Holds the zwlr_layer_surface_v1 object for a specific QWindow.
class WlrLayerSurfaceV1 : public QObject,
                          public QtWayland::zwlr_layer_surface_v1
{
    Q_OBJECT
    Q_PROPERTY(bool configured READ isConfigured NOTIFY configured)

public:
    explicit WlrLayerSurfaceV1(struct ::zwlr_layer_surface_v1 *surface,
                               QObject *parent = nullptr);
    ~WlrLayerSurfaceV1() override;

    bool isConfigured() const;

    // Update the exclusive zone on this surface and flush to the compositor.
    void setExclusiveZone(int zone);

signals:
    void configured();
    void closed();
    void firstConfigureReceived(quint32 serial);
    void configureAcked(quint32 serial);

protected:
    void zwlr_layer_surface_v1_configure(uint32_t serial,
                                         uint32_t width,
                                         uint32_t height) override;
    void zwlr_layer_surface_v1_closed() override;

private:
    bool m_configured = false;
    bool m_firstConfigureReceived = false;
};
