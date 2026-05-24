#include "appdecklayerwindow.h"
#include "wlrlayershell.h"
#include "../../utils/slmlogcategories.h"

#include <QDebug>
#include <QExposeEvent>
#include <QGuiApplication>
#include <QScreen>

AppDeckLayerWindow::AppDeckLayerWindow(WlrLayerShell *layerShell, QWindow *parent)
    : QQuickWindow(parent)
    , m_layerShell(layerShell)
{
    Q_ASSERT(layerShell);

    setTitle(QStringLiteral("SLM AppDeck Surface"));
    setColor(Qt::transparent);

    // Layer-shell windows must not have a transient parent — they are top-level.
    setTransientParent(nullptr);

    // Retry configuring if the Wayland native surface isn't ready immediately.
    m_retryTimer.setInterval(50);
    m_retryTimer.setSingleShot(false);
    connect(&m_retryTimer, &QTimer::timeout, this, &AppDeckLayerWindow::tryConfigureLayerSurface);

    // Connect to layerShell activation in case the protocol isn't ready yet.
    connect(layerShell, &WlrLayerShell::activeChanged, this, [this]() {
        if (!m_layerConfigured) {
            tryConfigureLayerSurface();
        }
    });
}

AppDeckLayerWindow::~AppDeckLayerWindow() = default;

bool AppDeckLayerWindow::isLayerConfigured() const
{
    return m_layerConfigured;
}

bool AppDeckLayerWindow::isLayerShellSupported() const
{
    return m_layerShell && m_layerShell->isActive();
}

int AppDeckLayerWindow::exclusiveZone() const
{
    return m_exclusiveZone;
}

void AppDeckLayerWindow::setExclusiveZone(int zone)
{
    if (m_exclusiveZone == zone) {
        return;
    }
    m_exclusiveZone = zone;
    emit exclusiveZoneChanged();
    // If already configured, we can't easily update — for now log a warning.
    // Future: support set_exclusive_zone updates.
    if (m_layerConfigured) {
        qCDebug(slmLayershell) << "AppDeckLayerWindow: exclusiveZone changed after config — zone=" << zone;
    }
}

void AppDeckLayerWindow::exposeEvent(QExposeEvent *event)
{
    QQuickWindow::exposeEvent(event);
    if (!m_layerConfigured && isExposed()) {
        tryConfigureLayerSurface();
    }
}

void AppDeckLayerWindow::tryConfigureLayerSurface()
{
    if (m_layerConfigured) {
        m_retryTimer.stop();
        return;
    }

    if (!m_layerShell || !m_layerShell->isActive()) {
        // Protocol not yet advertised — wait.
        if (!m_retryTimer.isActive()) {
            m_retryTimer.start();
        }
        return;
    }

    // Layer TOP puts appdeck above normal windows and AppDeck overlays.
    // AnchorBottom pins the surface to the bottom of the screen.
    // AnchorLeft | AnchorRight stretches the appdeck horizontally.
    constexpr int layer   = WlrLayerShell::LayerTop;
    constexpr int anchors = WlrLayerShell::AnchorBottom
                            | WlrLayerShell::AnchorLeft
                            | WlrLayerShell::AnchorRight;

    if (m_layerShell->configureAsLayerSurface(this, layer, anchors,
                                               m_exclusiveZone,
                                               QStringLiteral("slm-appdeck"))) {
        m_retryTimer.stop();
        m_layerConfigured = true;
        emit layerConfiguredChanged();
        qCInfo(slmLayershell) << "AppDeckLayerWindow: layer surface configured";
    } else {
        if (!m_retryTimer.isActive()) {
            m_retryTimer.start();
        }
    }
}
