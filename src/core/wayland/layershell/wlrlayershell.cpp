#include "wlrlayershell.h"
#include "dockbootstrapstate.h"

#include <QDebug>
#include <QGuiApplication>
#include <QWindow>
#include <qpa/qplatformnativeinterface.h>
#include <wayland-client-core.h>

// ── WlrLayerShell ─────────────────────────────────────────────────────────────

WlrLayerShell::WlrLayerShell()
    : QWaylandClientExtensionTemplate<WlrLayerShell>(4)
{
}

bool WlrLayerShell::isSupported() const
{
    return isActive();
}

void WlrLayerShell::setDockBootstrapState(DockBootstrapState *state)
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
    window->setFlag(Qt::FramelessWindowHint, true);
    window->setFlag(Qt::WindowDoesNotAcceptFocus, true);

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
    // Size 0,0 lets the compositor determine the dimensions from anchors.
    zwlr_layer_surface_v1_set_size(layerSurface, 0, 0);

    // Initial empty commit is required by layer-shell before compositor sends
    // the first configure. Do this explicitly so bootstrap does not depend on
    // QWindow visibility or first buffer attach timing.
    wl_surface_commit(surface);

    // The WlrLayerSurfaceV1 object manages the ack_configure lifecycle.
    auto *surfaceObj = new WlrLayerSurfaceV1(layerSurface, window);
    if (m_dockBootstrapState) {
        m_dockBootstrapState->setLayerRoleBound(true);
        QObject::connect(surfaceObj, &WlrLayerSurfaceV1::firstConfigureReceived,
                         m_dockBootstrapState, &DockBootstrapState::markFirstConfigureReceived);
        QObject::connect(surfaceObj, &WlrLayerSurfaceV1::configureAcked,
                         m_dockBootstrapState, &DockBootstrapState::markConfigureAcked);
    }
    QObject::connect(surfaceObj, &WlrLayerSurfaceV1::configured, window, [window]() {
        // Commit the surface after configuration so the compositor shows it.
        window->requestActivate();
    });
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
    qDebug() << "[WlrLayerShell] updated exclusiveZone=" << exclusiveZone
             << "for" << window->title();
    return true;
}

// ── WlrLayerSurfaceV1 ─────────────────────────────────────────────────────────

WlrLayerSurfaceV1::WlrLayerSurfaceV1(struct ::zwlr_layer_surface_v1 *surface,
                                     QObject *parent)
    : QObject(parent)
    , QtWayland::zwlr_layer_surface_v1(surface)
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
