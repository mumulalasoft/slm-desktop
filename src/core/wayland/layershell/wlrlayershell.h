#pragma once

#include "qwayland-wlr-layer-shell-unstable-v1.h"

#include <QWaylandClientExtensionTemplate>
#include <QObject>
#include <QWindow>

class AppDeckBootstrapState;
struct wl_surface;
struct wl_compositor;

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
    // Call this whenever the appdeck height changes so the compositor keeps the
    // correct reservation at the bottom of the screen.
    Q_INVOKABLE bool setExclusiveZone(QWindow *window, int exclusiveZone);
    Q_INVOKABLE bool setLayer(QWindow *window, int layer);
    Q_INVOKABLE bool setKeyboardInteractivity(QWindow *window, int interactivity);
    // Update the configured size of an already-configured layer surface.
    Q_INVOKABLE bool setLayerSurfaceSize(QWindow *window, int width, int height);
    // Update input region so transparent areas do not intercept pointer events.
    Q_INVOKABLE bool setLayerSurfaceInputRegion(QWindow *window,
                                                int x,
                                                int y,
                                                int width,
                                                int height);
    void setAppDeckBootstrapState(AppDeckBootstrapState *state);

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

    enum KeyboardInteractivity {
        KeyboardInteractivityNone = 0,
        KeyboardInteractivityExclusive = 1,
        KeyboardInteractivityOnDemand = 2,
    };
    Q_ENUM(KeyboardInteractivity)

private:
    AppDeckBootstrapState *m_dockBootstrapState = nullptr;
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
                               struct ::wl_surface *wlSurface,
                               QObject *parent = nullptr);
    ~WlrLayerSurfaceV1() override;

    bool isConfigured() const;

    // Update the exclusive zone on this surface and flush to the compositor.
    void setExclusiveZone(int zone);
    void setLayer(int layer);
    void setKeyboardInteractivity(int interactivity);
    // Update the layer-surface size for mode transitions (collapsed/expanded/context).
    void setSurfaceSize(int width, int height);
    // Limit pointer focus to a sub-rect of the layer surface.
    void setInputRegionRect(struct ::wl_compositor *compositor,
                            int x,
                            int y,
                            int width,
                            int height);

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
    struct ::wl_surface *m_surface = nullptr;
};
