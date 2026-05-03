#include "wlrlayershell.h"
#include "appdeckbootstrapstate.h"

#include <QDebug>
#include <QGuiApplication>
#include <QWindow>
#include <qpa/qplatformnativeinterface.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

// ── WlrLayerShell ─────────────────────────────────────────────────────────────

WlrLayerShell::WlrLayerShell()
    : QWaylandClientExtensionTemplate<WlrLayerShell>(4)
{
}

bool WlrLayerShell::isSupported() const
{
    return isActive();
}

void WlrLayerShell::setAppDeckBootstrapState(AppDeckBootstrapState *state)
{
    m_dockBootstrapState = state;
}

bool WlrLayerShell::configureAsLayerSurface(QWindow *window,
                                             int layer,
                                             int anchors,
                                             int exclusiveZone,
                                             const QString &nameSpace)
{
    if (!window) {
        qWarning() << "[WlrLayerShell] null window";
        return false;
    }
    if (!isActive()) {
        qWarning() << "[WlrLayerShell] protocol not available";
        return false;
    }

    // Ensure the window's native platform surface is created.
    window->create();

    // Get the wl_surface from the Qt window via the platform native interface.
    QPlatformNativeInterface *native = QGuiApplication::platformNativeInterface();
    if (!native) {
        qWarning() << "[WlrLayerShell] no platform native interface";
        return false;
    }

    struct ::wl_surface *surface = static_cast<struct ::wl_surface *>(
        native->nativeResourceForWindow(QByteArrayLiteral("surface"), window));
    if (!surface) {
        qWarning() << "[WlrLayerShell] failed to get wl_surface from window";
        return false;
    }

    // Use nullptr output to let the compositor pick the output.
    struct ::zwlr_layer_surface_v1 *layerSurface =
        get_layer_surface(surface,
                          nullptr,
                          static_cast<uint32_t>(layer),
                          nameSpace);
    if (!layerSurface) {
        qWarning() << "[WlrLayerShell] failed to create layer surface";
        return false;
    }

    // Configure anchors, exclusive zone, and zero margin.
    zwlr_layer_surface_v1_set_anchor(layerSurface, static_cast<uint32_t>(anchors));
    zwlr_layer_surface_v1_set_exclusive_zone(layerSurface, exclusiveZone);
    zwlr_layer_surface_v1_set_margin(layerSurface, 0, 0, 0, 0);
    // Use explicit window size so the layer surface does not expand to a
    // fullscreen transparent input region on some compositors.
    const uint32_t width = static_cast<uint32_t>(qMax(1, window->width()));
    const uint32_t height = static_cast<uint32_t>(qMax(1, window->height()));
    zwlr_layer_surface_v1_set_size(layerSurface, width, height);

    // Initial empty commit is required by layer-shell before compositor sends
    // the first configure. Do this explicitly so bootstrap does not depend on
    // QWindow visibility or first buffer attach timing.
    wl_surface_commit(surface);

    // The WlrLayerSurfaceV1 object manages the ack_configure lifecycle.
    auto *surfaceObj = new WlrLayerSurfaceV1(layerSurface, surface, window);
    if (m_dockBootstrapState) {
        m_dockBootstrapState->setLayerRoleBound(true);
        QObject::connect(surfaceObj, &WlrLayerSurfaceV1::firstConfigureReceived,
                         m_dockBootstrapState, &AppDeckBootstrapState::markFirstConfigureReceived);
        QObject::connect(surfaceObj, &WlrLayerSurfaceV1::configureAcked,
                         m_dockBootstrapState, &AppDeckBootstrapState::markConfigureAcked);
    }
    QObject::connect(surfaceObj, &WlrLayerSurfaceV1::closed, window, [window, surfaceObj]() {
        surfaceObj->deleteLater();
        window->close();
    });

    // Commit to apply layer-shell role.
    if (auto *iface = QGuiApplication::platformNativeInterface()) {
        struct ::wl_display *display = static_cast<struct ::wl_display *>(
            iface->nativeResourceForIntegration(QByteArrayLiteral("wl_display")));
        if (display) {
            wl_display_flush(display);
        }
    }

    qInfo() << "[WlrLayerShell] configured layer surface for" << window->title()
            << "layer=" << layer << "anchors=" << anchors << "exclusiveZone=" << exclusiveZone;
    return true;
}

bool WlrLayerShell::setExclusiveZone(QWindow *window, int exclusiveZone)
{
    if (!window) return false;
    auto *surf = window->findChild<WlrLayerSurfaceV1 *>();
    if (!surf || !surf->isConfigured()) return false;
    surf->setExclusiveZone(exclusiveZone);
    if (auto *iface = QGuiApplication::platformNativeInterface()) {
        struct ::wl_display *display = static_cast<struct ::wl_display *>(
            iface->nativeResourceForIntegration(QByteArrayLiteral("wl_display")));
        if (display) wl_display_flush(display);
    }
    return true;
}

bool WlrLayerShell::setLayer(QWindow *window, int layer)
{
    if (!window) return false;
    auto *surf = window->findChild<WlrLayerSurfaceV1 *>();
    if (!surf || !surf->isConfigured()) return false;
    surf->setLayer(layer);
    if (auto *iface = QGuiApplication::platformNativeInterface()) {
        struct ::wl_display *display = static_cast<struct ::wl_display *>(
            iface->nativeResourceForIntegration(QByteArrayLiteral("wl_display")));
        if (display) wl_display_flush(display);
    }
    return true;
}

bool WlrLayerShell::setKeyboardInteractivity(QWindow *window, int interactivity)
{
    if (!window) return false;
    auto *surf = window->findChild<WlrLayerSurfaceV1 *>();
    if (!surf || !surf->isConfigured()) return false;
    surf->setKeyboardInteractivity(interactivity);
    if (auto *iface = QGuiApplication::platformNativeInterface()) {
        struct ::wl_display *display = static_cast<struct ::wl_display *>(
            iface->nativeResourceForIntegration(QByteArrayLiteral("wl_display")));
        if (display) wl_display_flush(display);
    }
    return true;
}

bool WlrLayerShell::setLayerSurfaceSize(QWindow *window, int width, int height)
{
    if (!window) return false;
    auto *surf = window->findChild<WlrLayerSurfaceV1 *>();
    if (!surf || !surf->isConfigured()) return false;
    surf->setSurfaceSize(width, height);
    if (auto *iface = QGuiApplication::platformNativeInterface()) {
        struct ::wl_display *display = static_cast<struct ::wl_display *>(
            iface->nativeResourceForIntegration(QByteArrayLiteral("wl_display")));
        if (display) wl_display_flush(display);
    }
    return true;
}

bool WlrLayerShell::setLayerSurfaceInputRegion(QWindow *window,
                                               int x,
                                               int y,
                                               int width,
                                               int height)
{
    if (!window) return false;
    auto *surf = window->findChild<WlrLayerSurfaceV1 *>();
    if (!surf || !surf->isConfigured()) return false;

    QPlatformNativeInterface *native = QGuiApplication::platformNativeInterface();
    if (!native) return false;
    struct ::wl_compositor *compositor = static_cast<struct ::wl_compositor *>(
        native->nativeResourceForIntegration(QByteArrayLiteral("compositor")));
    if (!compositor) return false;

    surf->setInputRegionRect(compositor, x, y, width, height);
    if (auto *iface = QGuiApplication::platformNativeInterface()) {
        struct ::wl_display *display = static_cast<struct ::wl_display *>(
            iface->nativeResourceForIntegration(QByteArrayLiteral("wl_display")));
        if (display) wl_display_flush(display);
    }
    return true;
}

// ── WlrLayerSurfaceV1 ─────────────────────────────────────────────────────────

WlrLayerSurfaceV1::WlrLayerSurfaceV1(struct ::zwlr_layer_surface_v1 *surface,
                                     struct ::wl_surface *wlSurface,
                                     QObject *parent)
    : QObject(parent)
    , QtWayland::zwlr_layer_surface_v1(surface)
    , m_surface(wlSurface)
{
}

WlrLayerSurfaceV1::~WlrLayerSurfaceV1()
{
    if (isInitialized()) {
        destroy();
    }
}

bool WlrLayerSurfaceV1::isConfigured() const
{
    return m_configured;
}

void WlrLayerSurfaceV1::setExclusiveZone(int zone)
{
    set_exclusive_zone(zone);
    if (m_surface) {
        wl_surface_commit(m_surface);
    }
}

void WlrLayerSurfaceV1::setLayer(int layer)
{
    set_layer(static_cast<uint32_t>(qBound(0, layer, 3)));
    if (m_surface) {
        wl_surface_commit(m_surface);
    }
}

void WlrLayerSurfaceV1::setKeyboardInteractivity(int interactivity)
{
    set_keyboard_interactivity(static_cast<uint32_t>(qBound(0, interactivity, 2)));
    if (m_surface) {
        wl_surface_commit(m_surface);
    }
}

void WlrLayerSurfaceV1::setSurfaceSize(int width, int height)
{
    const uint32_t w = static_cast<uint32_t>(qMax(1, width));
    const uint32_t h = static_cast<uint32_t>(qMax(1, height));
    set_size(w, h);
    if (m_surface) {
        wl_surface_commit(m_surface);
    }
}

void WlrLayerSurfaceV1::setInputRegionRect(struct ::wl_compositor *compositor,
                                           int x,
                                           int y,
                                           int width,
                                           int height)
{
    if (!m_surface || !compositor) {
        return;
    }

    const int safeX = qMax(0, x);
    const int safeY = qMax(0, y);
    const int safeW = qMax(1, width);
    const int safeH = qMax(1, height);

    struct ::wl_region *region = wl_compositor_create_region(compositor);
    if (!region) {
        return;
    }
    wl_region_add(region, safeX, safeY, safeW, safeH);
    wl_surface_set_input_region(m_surface, region);
    wl_surface_commit(m_surface);
    wl_region_destroy(region);
}

void WlrLayerSurfaceV1::zwlr_layer_surface_v1_configure(uint32_t serial,
                                                         uint32_t width,
                                                         uint32_t height)
{
    Q_UNUSED(width)
    Q_UNUSED(height)
    if (!m_firstConfigureReceived) {
        m_firstConfigureReceived = true;
        emit firstConfigureReceived(serial);
    }
    ack_configure(serial);
    emit configureAcked(serial);
    if (!m_configured) {
        m_configured = true;
        emit configured();
    }
}

void WlrLayerSurfaceV1::zwlr_layer_surface_v1_closed()
{
    emit closed();
}
