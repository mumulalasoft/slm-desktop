#include "appdecklayershellcontroller.h"

#include "appdeckbootstrapstate.h"
#include "wlrlayershell.h"

#include <QDebug>
#include <QRegion>
#include <QSize>
#include <QTimer>
#include <QWindow>

#ifdef SLM_HAVE_LAYERSHELLQT
#include <LayerShellQt/Window>
#endif

AppDeckLayerShellController::AppDeckLayerShellController(WlrLayerShell *fallbackLayerShell,
                                                         QObject *parent)
    : QObject(parent)
    , m_fallbackLayerShell(fallbackLayerShell)
{
    if (m_fallbackLayerShell) {
        connect(m_fallbackLayerShell, &WlrLayerShell::activeChanged,
                this, &AppDeckLayerShellController::supportedChanged);
    }
}

void AppDeckLayerShellController::setBootstrapState(AppDeckBootstrapState *state)
{
    m_bootstrapState = state;
}

bool AppDeckLayerShellController::isSupported() const
{
#ifdef SLM_HAVE_LAYERSHELLQT
    return true;
#else
    return m_fallbackLayerShell && m_fallbackLayerShell->isSupported();
#endif
}

void AppDeckLayerShellController::prepareWindow(QWindow *window)
{
#ifdef SLM_HAVE_LAYERSHELLQT
    if (!window || m_attached) {
        return;
    }
    m_layerWindow = LayerShellQt::Window::get(window);
    if (!m_layerWindow) {
        return;
    }
    m_layerWindow->setAnchors(static_cast<LayerShellQt::Window::Anchor>(
        LayerShellQt::Window::AnchorBottom
        | LayerShellQt::Window::AnchorLeft
        | LayerShellQt::Window::AnchorRight));
    m_layerWindow->setExclusiveZone(0);
    m_layerWindow->setScope(QStringLiteral("slm-appdeck"));
    m_layerWindow->setLayer(LayerShellQt::Window::LayerTop);
    m_layerWindow->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    watchWindow(window);
    m_attached = true;
    startGeometryGrace();
    m_lastLayer = WlrLayerShell::LayerTop;
    m_lastKeyboardInteractivity = WlrLayerShell::KeyboardInteractivityNone;
    if (m_bootstrapState) {
        m_bootstrapState->setLayerRoleBound(true);
    }
    // Watch for the first expose event, which LayerShellQt fires (via sendExpose)
    // only after receiving and acking zwlr_layer_surface_v1.configure. That is the
    // correct moment to advance bootstrap — not a fixed-delay timer.
    m_exposeSeen = false;
    window->installEventFilter(this);
#else
    Q_UNUSED(window)
#endif
}

void AppDeckLayerShellController::prepareTopBarWindow(QWindow *window)
{
#ifdef SLM_HAVE_LAYERSHELLQT
    if (!window || m_attached) {
        return;
    }
    m_layerWindow = LayerShellQt::Window::get(window);
    if (!m_layerWindow) {
        return;
    }
    m_layerWindow->setAnchors(static_cast<LayerShellQt::Window::Anchor>(
        LayerShellQt::Window::AnchorTop
        | LayerShellQt::Window::AnchorLeft
        | LayerShellQt::Window::AnchorRight));
    m_layerWindow->setExclusiveZone(0);
    m_layerWindow->setScope(QStringLiteral("slm-crown"));
    m_layerWindow->setLayer(LayerShellQt::Window::LayerTop);
    m_layerWindow->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    watchWindow(window);
    m_attached = true;
    startGeometryGrace();
    m_lastLayer = WlrLayerShell::LayerTop;
    m_lastKeyboardInteractivity = WlrLayerShell::KeyboardInteractivityNone;
#else
    Q_UNUSED(window)
#endif
}

void AppDeckLayerShellController::onWindowAboutToShow()
{
#ifdef SLM_HAVE_LAYERSHELLQT
    startGeometryGrace();
#endif
}

void AppDeckLayerShellController::suspendRendering(QWindow *window)
{
    Q_UNUSED(window)
}

void AppDeckLayerShellController::resumeRendering(QWindow *window)
{
    Q_UNUSED(window)
}

bool AppDeckLayerShellController::setDock(QWindow *window,
                                               int width,
                                               int height,
                                               int inputX,
                                               int inputY,
                                               int inputWidth,
                                               int inputHeight)
{
    return configure(window,
                     WlrLayerShell::LayerTop,
                     WlrLayerShell::KeyboardInteractivityNone,
                     WlrLayerShell::AnchorBottom
                         | WlrLayerShell::AnchorLeft
                         | WlrLayerShell::AnchorRight,
                     0,
                     QStringLiteral("slm-appdeck"),
                     width,
                     height,
                     QRect(inputX, inputY, inputWidth, inputHeight));
}

bool AppDeckLayerShellController::setGrid(QWindow *window,
                                              int width,
                                              int height,
                                              int inputX,
                                              int inputY,
                                              int inputWidth,
                                              int inputHeight)
{
    return configure(window,
                     WlrLayerShell::LayerOverlay,
                     WlrLayerShell::KeyboardInteractivityExclusive,
                     WlrLayerShell::AnchorBottom
                         | WlrLayerShell::AnchorLeft
                         | WlrLayerShell::AnchorRight,
                     0,
                     QStringLiteral("slm-appdeck"),
                     width,
                     height,
                     QRect(inputX, inputY, inputWidth, inputHeight));
}

bool AppDeckLayerShellController::setTopBar(QWindow *window,
                                            int width,
                                            int height,
                                            int inputX,
                                            int inputY,
                                            int inputWidth,
                                            int inputHeight,
                                            int exclusiveZone)
{
    return configure(window,
                     WlrLayerShell::LayerTop,
                     WlrLayerShell::KeyboardInteractivityNone,
                     WlrLayerShell::AnchorTop
                         | WlrLayerShell::AnchorLeft
                         | WlrLayerShell::AnchorRight,
                     qMax(0, exclusiveZone),
                     QStringLiteral("slm-crown"),
                     width,
                     height,
                     QRect(inputX, inputY, inputWidth, inputHeight));
}

bool AppDeckLayerShellController::configure(QWindow *window,
                                            int layer,
                                            int keyboardInteractivity,
                                            int anchors,
                                            int exclusiveZone,
                                            const QString &scope,
                                            int width,
                                            int height,
                                            const QRect &inputRegion)
{
    if (!window || !window->isExposed()) {
        return false;
    }
    if (!isSupported()) {
        return false;
    }

    watchWindow(window);

    const bool prevAttached = m_attached;

#ifdef SLM_HAVE_LAYERSHELLQT
    if (!m_layerWindow || !prevAttached) {
        m_layerWindow = LayerShellQt::Window::get(window);
    }
    if (!m_layerWindow) {
        return false;
    }
    if (!prevAttached) {
        int qtAnchors = 0;
        if (anchors & WlrLayerShell::AnchorTop) {
            qtAnchors |= LayerShellQt::Window::AnchorTop;
        }
        if (anchors & WlrLayerShell::AnchorBottom) {
            qtAnchors |= LayerShellQt::Window::AnchorBottom;
        }
        if (anchors & WlrLayerShell::AnchorLeft) {
            qtAnchors |= LayerShellQt::Window::AnchorLeft;
        }
        if (anchors & WlrLayerShell::AnchorRight) {
            qtAnchors |= LayerShellQt::Window::AnchorRight;
        }
        m_layerWindow->setAnchors(static_cast<LayerShellQt::Window::Anchor>(qtAnchors));
        m_layerWindow->setExclusiveZone(qMax(0, exclusiveZone));
        m_layerWindow->setScope(scope);
    }
    m_layerWindow->setExclusiveZone(qMax(0, exclusiveZone));
    if (m_lastLayer != layer) {
        m_layerWindow->setLayer(layer == WlrLayerShell::LayerOverlay
                                ? LayerShellQt::Window::LayerOverlay
                                : LayerShellQt::Window::LayerTop);
    }
    if (m_lastKeyboardInteractivity != keyboardInteractivity) {
        m_layerWindow->setKeyboardInteractivity(
            keyboardInteractivity == WlrLayerShell::KeyboardInteractivityExclusive
                ? LayerShellQt::Window::KeyboardInteractivityExclusive
                : LayerShellQt::Window::KeyboardInteractivityNone);
    }
    m_attached = true;
    if (!prevAttached) {
        // Re-attach after screen change: need a fresh configure/ack cycle.
        startGeometryGrace();
        if (m_bootstrapState) {
            m_bootstrapState->setIntegrationEnabled(true);
            m_bootstrapState->setLayerRoleBound(true);
            m_bootstrapState->markFirstConfigureReceived(0);
            m_bootstrapState->markConfigureAcked(0);
        }
    }
#else
    if (!m_attached) {
        if (!m_fallbackLayerShell->configureAsLayerSurface(window,
                                                           layer,
                                                           anchors,
                                                           qMax(0, exclusiveZone),
                                                           scope)) {
            return false;
        }
        m_attached = true;
    }
    m_fallbackLayerShell->setExclusiveZone(window, qMax(0, exclusiveZone));
    if (m_lastLayer != layer) {
        m_fallbackLayerShell->setLayer(window, layer);
    }
    if (m_lastKeyboardInteractivity != keyboardInteractivity) {
        m_fallbackLayerShell->setKeyboardInteractivity(window, keyboardInteractivity);
    }
#endif

    m_lastLayer = layer;
    m_lastKeyboardInteractivity = keyboardInteractivity;
    return applyGeometry(window, width, height, inputRegion);
}

bool AppDeckLayerShellController::applyGeometry(QWindow *window,
                                                int width,
                                                int height,
                                                const QRect &inputRegion)
{
    if (!window || !window->isExposed()) {
        return false;
    }
    const int safeWidth = qMax(1, width);
    const int safeHeight = qMax(1, height);
    const QRect safeInput(qMax(0, inputRegion.x()),
                          qMax(0, inputRegion.y()),
                          qMax(1, inputRegion.width()),
                          qMax(1, inputRegion.height()));

#ifdef SLM_HAVE_LAYERSHELLQT
    // Block all geometry commits during the grace period that follows every new
    // surface attach. Qt's setMask() calls wl_surface.commit immediately; any
    // commit before KWin's configure/ack violates zwlr_layer_surface_v1 and
    // causes KWin to send wl_display.error. Return true (optimistic) so callers
    // don't stall; the layerShellRetryTimer will apply geometry after grace.
    if (!m_geometrySafe) {
        return true;
    }
    window->resize(safeWidth, safeHeight);
    // Qt Wayland translates setMask() to wl_surface_set_input_region so that
    // transparent areas outside the dock/grid don't intercept pointer events.
    window->setMask(QRegion(safeInput));
    return true;
#else
    if (!m_fallbackLayerShell) {
        return false;
    }
    const bool sizeOk = m_fallbackLayerShell->setLayerSurfaceSize(window, safeWidth, safeHeight);
    const bool inputOk = m_fallbackLayerShell->setLayerSurfaceInputRegion(window,
                                                                          safeInput.x(),
                                                                          safeInput.y(),
                                                                          safeInput.width(),
                                                                          safeInput.height());
    return sizeOk && inputOk;
#endif
}

bool AppDeckLayerShellController::eventFilter(QObject *obj, QEvent *event)
{
#ifdef SLM_HAVE_LAYERSHELLQT
    if (!m_exposeSeen && event->type() == QEvent::Expose) {
        auto *window = qobject_cast<QWindow *>(obj);
        if (window && window->isExposed()) {
            m_exposeSeen = true;
            window->removeEventFilter(this);
            if (m_bootstrapState) {
                m_bootstrapState->markFirstConfigureReceived(0);
                m_bootstrapState->markConfigureAcked(0);
            }
        }
    }
#endif
    return QObject::eventFilter(obj, event);
}

void AppDeckLayerShellController::startGeometryGrace()
{
#ifdef SLM_HAVE_LAYERSHELLQT
    m_geometrySafe = false;
    // 50ms covers the KWin configure/ack round-trip in all tested configurations.
    QTimer::singleShot(50, this, [this]() { m_geometrySafe = true; });
#endif
}

void AppDeckLayerShellController::watchWindow(QWindow *window)
{
    if (m_window == window) {
        return;
    }
    m_window = window;
    connect(window, &QWindow::screenChanged, this, [this, window]() {
        if (!window || !window->isExposed()) {
            return;
        }
        QTimer::singleShot(300, this, [this, window]() {
            if (!window || !window->isExposed()) {
                return;
            }
            m_attached = false;
            m_geometrySafe = false;
            m_lastLayer = -1;
            m_lastKeyboardInteractivity = -1;
        });
    });
}
