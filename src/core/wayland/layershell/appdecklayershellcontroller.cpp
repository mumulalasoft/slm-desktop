#include "appdecklayershellcontroller.h"

#include "wlrlayershell.h"

#include <QDebug>
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

bool AppDeckLayerShellController::isSupported() const
{
#ifdef SLM_HAVE_LAYERSHELLQT
    return true;
#else
    return m_fallbackLayerShell && m_fallbackLayerShell->isSupported();
#endif
}

bool AppDeckLayerShellController::attach(QWindow *window,
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
                     width,
                     height,
                     QRect(inputX, inputY, inputWidth, inputHeight));
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
                     width,
                     height,
                     QRect(inputX, inputY, inputWidth, inputHeight));
}

bool AppDeckLayerShellController::configure(QWindow *window,
                                            int layer,
                                            int keyboardInteractivity,
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

#ifdef SLM_HAVE_LAYERSHELLQT
    auto *layerWindow = LayerShellQt::Window::get(window);
    if (!layerWindow) {
        return false;
    }
    layerWindow->setAnchors(static_cast<LayerShellQt::Window::Anchor>(
        LayerShellQt::Window::AnchorBottom
        | LayerShellQt::Window::AnchorLeft
        | LayerShellQt::Window::AnchorRight));
    layerWindow->setExclusiveZone(0);
    layerWindow->setScope(QStringLiteral("slm-appdeck"));
    layerWindow->setLayer(layer == WlrLayerShell::LayerOverlay
                          ? LayerShellQt::Window::LayerOverlay
                          : LayerShellQt::Window::LayerTop);
    layerWindow->setKeyboardInteractivity(
        keyboardInteractivity == WlrLayerShell::KeyboardInteractivityExclusive
            ? LayerShellQt::Window::KeyboardInteractivityExclusive
            : LayerShellQt::Window::KeyboardInteractivityNone);
    m_attached = true;
#else
    if (!m_attached) {
        constexpr int anchors = WlrLayerShell::AnchorBottom
                                | WlrLayerShell::AnchorLeft
                                | WlrLayerShell::AnchorRight;
        if (!m_fallbackLayerShell->configureAsLayerSurface(window,
                                                           layer,
                                                           anchors,
                                                           0,
                                                           QStringLiteral("slm-appdeck"))) {
            return false;
        }
        m_attached = true;
    }
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
    window->resize(safeWidth, safeHeight);
    Q_UNUSED(safeInput)
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
            m_lastLayer = -1;
            m_lastKeyboardInteractivity = -1;
        });
    });
}
