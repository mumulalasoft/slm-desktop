#include "appdecklayershellcontroller.h"

#include "appdeckbootstrapstate.h"
#include "wlrlayershell.h"

#include <QDebug>
#include <QLoggingCategory>
#include <QRegion>
#include <QSize>
#include <QStringList>
#include <QTimer>
#include <QWindow>

#ifdef SLM_HAVE_LAYERSHELLQT
#include <LayerShellQt/Window>
#endif

Q_LOGGING_CATEGORY(slmSurfaceStacking, "slm.stacking")

namespace {
QString layerName(int layer)
{
    switch (layer) {
    case WlrLayerShell::LayerOverlay:
        return QStringLiteral("OverlayLayer");
    case WlrLayerShell::LayerTop:
        return QStringLiteral("TopLayer");
    case WlrLayerShell::LayerBottom:
        return QStringLiteral("BottomLayer");
    case WlrLayerShell::LayerBackground:
        return QStringLiteral("BackgroundLayer");
    default:
        return QStringLiteral("UnknownLayer");
    }
}

QString anchorName(int anchors)
{
    QStringList parts;
    if (anchors & WlrLayerShell::AnchorTop) parts << QStringLiteral("top");
    if (anchors & WlrLayerShell::AnchorBottom) parts << QStringLiteral("bottom");
    if (anchors & WlrLayerShell::AnchorLeft) parts << QStringLiteral("left");
    if (anchors & WlrLayerShell::AnchorRight) parts << QStringLiteral("right");
    return parts.join(QLatin1Char('|'));
}
}

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

AppDeckLayerShellController::SurfaceState &
AppDeckLayerShellController::stateFor(QWindow *window)
{
    SurfaceState &state = m_surfaces[window];
    state.window = window;
    return state;
}

void AppDeckLayerShellController::prepareWindow(QWindow *window)
{
#ifdef SLM_HAVE_LAYERSHELLQT
    if (!window) {
        return;
    }
    SurfaceState &state = stateFor(window);
    if (state.attached) {
        return;
    }
    state.layerWindow = LayerShellQt::Window::get(window);
    if (!state.layerWindow) {
        return;
    }
    state.layerWindow->setAnchors(static_cast<LayerShellQt::Window::Anchor>(
        LayerShellQt::Window::AnchorBottom
        | LayerShellQt::Window::AnchorLeft
        | LayerShellQt::Window::AnchorRight));
    state.layerWindow->setExclusiveZone(0);
    state.layerWindow->setScope(QStringLiteral("slm-appdeck"));
    state.layerWindow->setLayer(LayerShellQt::Window::LayerTop);
    state.layerWindow->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    watchWindow(window);
    state.attached = true;
    startGeometryGrace(window);
    state.lastLayer = WlrLayerShell::LayerTop;
    state.lastKeyboardInteractivity = WlrLayerShell::KeyboardInteractivityNone;
    if (m_bootstrapState) {
        m_bootstrapState->setLayerRoleBound(true);
    }
    qCInfo(slmSurfaceStacking).noquote()
        << "[APPDECK] [LAYERSHELL] [SURFACE_READY] prepared layer=TopLayer anchors=bottom|left|right exclusiveZone=0 scope=slm-appdeck";
#else
    Q_UNUSED(window)
#endif
}

void AppDeckLayerShellController::prepareTopBarWindow(QWindow *window)
{
#ifdef SLM_HAVE_LAYERSHELLQT
    if (!window) {
        return;
    }
    SurfaceState &state = stateFor(window);
    if (state.attached) {
        return;
    }
    state.layerWindow = LayerShellQt::Window::get(window);
    if (!state.layerWindow) {
        return;
    }
    state.layerWindow->setAnchors(static_cast<LayerShellQt::Window::Anchor>(
        LayerShellQt::Window::AnchorTop
        | LayerShellQt::Window::AnchorLeft
        | LayerShellQt::Window::AnchorRight));
    state.layerWindow->setExclusiveZone(0);
    state.layerWindow->setScope(QStringLiteral("slm-crown"));
    state.layerWindow->setLayer(LayerShellQt::Window::LayerOverlay);
    state.layerWindow->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    watchWindow(window);
    state.attached = true;
    startGeometryGrace(window);
    state.lastLayer = WlrLayerShell::LayerOverlay;
    state.lastKeyboardInteractivity = WlrLayerShell::KeyboardInteractivityNone;
    qCInfo(slmSurfaceStacking).noquote()
        << "[CROWN] [LAYERSHELL] [SURFACE_READY] prepared layer=OverlayLayer anchors=top|left|right exclusiveZone=0 scope=slm-crown";
#else
    Q_UNUSED(window)
#endif
}

void AppDeckLayerShellController::prepareNotificationWindow(QWindow *window)
{
#ifdef SLM_HAVE_LAYERSHELLQT
    if (!window) {
        return;
    }
    SurfaceState &state = stateFor(window);
    if (state.attached) {
        return;
    }
    state.layerWindow = LayerShellQt::Window::get(window);
    if (!state.layerWindow) {
        return;
    }
    state.layerWindow->setAnchors(static_cast<LayerShellQt::Window::Anchor>(
        LayerShellQt::Window::AnchorTop
        | LayerShellQt::Window::AnchorRight));
    state.layerWindow->setExclusiveZone(0);
    state.layerWindow->setScope(QStringLiteral("slm-notifyd"));
    state.layerWindow->setLayer(LayerShellQt::Window::LayerOverlay);
    state.layerWindow->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    watchWindow(window);
    state.attached = true;
    startGeometryGrace(window);
    state.lastLayer = WlrLayerShell::LayerOverlay;
    state.lastKeyboardInteractivity = WlrLayerShell::KeyboardInteractivityNone;
    qCInfo(slmSurfaceStacking).noquote()
        << "[NOTIFY] [LAYERSHELL] [SURFACE_READY] prepared layer=OverlayLayer anchors=top|right exclusiveZone=0 scope=slm-notifyd";
#else
    Q_UNUSED(window)
#endif
}

void AppDeckLayerShellController::prepareSecurityOverlayWindow(QWindow *window)
{
#ifdef SLM_HAVE_LAYERSHELLQT
    if (!window) {
        return;
    }
    SurfaceState &state = stateFor(window);
    if (state.attached) {
        return;
    }
    state.layerWindow = LayerShellQt::Window::get(window);
    if (!state.layerWindow) {
        return;
    }
    state.layerWindow->setAnchors(static_cast<LayerShellQt::Window::Anchor>(
        LayerShellQt::Window::AnchorTop
        | LayerShellQt::Window::AnchorBottom
        | LayerShellQt::Window::AnchorLeft
        | LayerShellQt::Window::AnchorRight));
    state.layerWindow->setExclusiveZone(0);
    state.layerWindow->setScope(QStringLiteral("slm-lockscreen"));
    state.layerWindow->setLayer(LayerShellQt::Window::LayerOverlay);
    state.layerWindow->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityExclusive);
    watchWindow(window);
    state.attached = true;
    startGeometryGrace(window);
    state.lastLayer = WlrLayerShell::LayerOverlay;
    state.lastKeyboardInteractivity = WlrLayerShell::KeyboardInteractivityExclusive;
    qCInfo(slmSurfaceStacking).noquote()
        << "[LOCKSCREEN] [LAYERSHELL] [SURFACE_READY] prepared layer=OverlayLayer anchors=top|bottom|left|right exclusiveZone=0 scope=slm-lockscreen";
#else
    Q_UNUSED(window)
#endif
}

void AppDeckLayerShellController::onWindowAboutToShow()
{
#ifdef SLM_HAVE_LAYERSHELLQT
    for (auto it = m_surfaces.begin(); it != m_surfaces.end(); ++it) {
        if (it.value().window) {
            startGeometryGrace(it.value().window);
        }
    }
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
                     WlrLayerShell::LayerOverlay,
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

bool AppDeckLayerShellController::setNotificationSurface(QWindow *window,
                                                         int width,
                                                         int height,
                                                         int inputX,
                                                         int inputY,
                                                         int inputWidth,
                                                         int inputHeight)
{
    return configure(window,
                     WlrLayerShell::LayerOverlay,
                     WlrLayerShell::KeyboardInteractivityNone,
                     WlrLayerShell::AnchorTop
                         | WlrLayerShell::AnchorRight,
                     0,
                     QStringLiteral("slm-notifyd"),
                     width,
                     height,
                     QRect(inputX, inputY, inputWidth, inputHeight));
}

bool AppDeckLayerShellController::setSecurityOverlay(QWindow *window,
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
                     WlrLayerShell::AnchorTop
                         | WlrLayerShell::AnchorBottom
                         | WlrLayerShell::AnchorLeft
                         | WlrLayerShell::AnchorRight,
                     0,
                     QStringLiteral("slm-lockscreen"),
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

    SurfaceState &state = stateFor(window);
    const bool prevAttached = state.attached;

#ifdef SLM_HAVE_LAYERSHELLQT
    if (!state.layerWindow || !prevAttached) {
        state.layerWindow = LayerShellQt::Window::get(window);
    }
    if (!state.layerWindow) {
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
        state.layerWindow->setAnchors(static_cast<LayerShellQt::Window::Anchor>(qtAnchors));
        state.layerWindow->setExclusiveZone(qMax(0, exclusiveZone));
        state.layerWindow->setScope(scope);
    }
    state.layerWindow->setExclusiveZone(qMax(0, exclusiveZone));
    if (state.lastLayer != layer) {
        state.layerWindow->setLayer(layer == WlrLayerShell::LayerOverlay
                                ? LayerShellQt::Window::LayerOverlay
                                : LayerShellQt::Window::LayerTop);
    }
    if (state.lastKeyboardInteractivity != keyboardInteractivity) {
        state.layerWindow->setKeyboardInteractivity(
            keyboardInteractivity == WlrLayerShell::KeyboardInteractivityExclusive
                ? LayerShellQt::Window::KeyboardInteractivityExclusive
                : LayerShellQt::Window::KeyboardInteractivityNone);
    }
    state.attached = true;
    if (!prevAttached) {
        // Re-attach after screen change: need a fresh configure/ack cycle.
        startGeometryGrace(window);
        if (m_bootstrapState) {
            m_bootstrapState->setIntegrationEnabled(true);
            m_bootstrapState->setLayerRoleBound(true);
            m_bootstrapState->markFirstConfigureReceived(0);
            m_bootstrapState->markConfigureAcked(0);
        }
    }
#else
    if (!state.attached) {
        if (!m_fallbackLayerShell->configureAsLayerSurface(window,
                                                           layer,
                                                           anchors,
                                                           qMax(0, exclusiveZone),
                                                           scope)) {
            return false;
        }
        state.attached = true;
    }
    m_fallbackLayerShell->setExclusiveZone(window, qMax(0, exclusiveZone));
    if (state.lastLayer != layer) {
        m_fallbackLayerShell->setLayer(window, layer);
    }
    if (state.lastKeyboardInteractivity != keyboardInteractivity) {
        m_fallbackLayerShell->setKeyboardInteractivity(window, keyboardInteractivity);
    }
#endif

    state.lastLayer = layer;
    state.lastKeyboardInteractivity = keyboardInteractivity;
    qCInfo(slmSurfaceStacking).noquote()
        << "[STACKING] [LAYERSHELL]"
        << "scope=" + scope
        << "layer=" + layerName(layer)
        << "anchors=" + anchorName(anchors)
        << "exclusiveZone=" + QString::number(qMax(0, exclusiveZone))
        << "size=" + QStringLiteral("%1x%2").arg(qMax(1, width)).arg(qMax(1, height))
        << "input=" + QStringLiteral("%1,%2 %3x%4")
                        .arg(qMax(0, inputRegion.x()))
                        .arg(qMax(0, inputRegion.y()))
                        .arg(qMax(1, inputRegion.width()))
                        .arg(qMax(1, inputRegion.height()));
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
    SurfaceState &state = stateFor(window);
    if (!state.geometrySafe) {
        return true;
    }
    window->resize(safeWidth, safeHeight);
    // Qt Wayland translates setMask() to wl_surface_set_input_region so that
    // transparent areas outside the dock/grid don't intercept pointer events.
    window->setMask(QRegion(safeInput));
    qCInfo(slmSurfaceStacking).noquote()
        << "[INPUT_REGION]"
        << "window=" + window->title()
        << "size=" + QStringLiteral("%1x%2").arg(safeWidth).arg(safeHeight)
        << "input=" + QStringLiteral("%1,%2 %3x%4")
                        .arg(safeInput.x())
                        .arg(safeInput.y())
                        .arg(safeInput.width())
                        .arg(safeInput.height());
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
    auto *window = qobject_cast<QWindow *>(obj);
    if (window && event->type() == QEvent::Expose) {
        SurfaceState &state = stateFor(window);
        if (!state.exposeSeen && window->isExposed()) {
            state.exposeSeen = true;
            window->removeEventFilter(this);
            state.watched = false;
            if (m_bootstrapState) {
                m_bootstrapState->markFirstConfigureReceived(0);
                m_bootstrapState->markConfigureAcked(0);
            }
            qCInfo(slmSurfaceStacking).noquote()
                << "[SURFACE_READY] [LAYERSHELL]"
                << "window=" + window->title()
                << "layer=" + layerName(state.lastLayer);
            emit surfaceReady();
        }
    }
#endif
    return QObject::eventFilter(obj, event);
}

void AppDeckLayerShellController::startGeometryGrace(QWindow *window)
{
#ifdef SLM_HAVE_LAYERSHELLQT
    if (!window) {
        return;
    }
    SurfaceState &state = stateFor(window);
    state.geometrySafe = false;
    // 50ms covers the KWin configure/ack round-trip in all tested configurations.
    QPointer<QWindow> guardedWindow(window);
    QTimer::singleShot(50, this, [this, guardedWindow]() {
        if (!guardedWindow) {
            return;
        }
        SurfaceState &state = stateFor(guardedWindow);
        state.geometrySafe = true;
    });
#endif
}

void AppDeckLayerShellController::watchWindow(QWindow *window)
{
    if (!window) {
        return;
    }
    SurfaceState &state = stateFor(window);
    if (state.watched) {
        return;
    }
    // LayerShellQt emits Expose after zwlr_layer_surface_v1.configure is acked.
    // Use that event as surface readiness instead of fixed startup delays.
    state.exposeSeen = false;
    state.watched = true;
    window->installEventFilter(this);
    connect(window, &QObject::destroyed, this, [this, window]() {
        m_surfaces.remove(window);
    });
    connect(window, &QWindow::screenChanged, this, [this, window]() {
        if (!window || !window->isExposed()) {
            return;
        }
        QTimer::singleShot(300, this, [this, window]() {
            if (!window || !window->isExposed()) {
                return;
            }
            SurfaceState &state = stateFor(window);
            state.attached = false;
            state.geometrySafe = false;
            state.lastLayer = -1;
            state.lastKeyboardInteractivity = -1;
        });
    });
}
