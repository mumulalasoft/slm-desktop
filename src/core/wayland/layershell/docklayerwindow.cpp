#include "docklayerwindow.h"
#include "wlrlayershell.h"

#include <QDebug>
#include <QExposeEvent>
#include <QGuiApplication>
#include <QScreen>

DockLayerWindow::DockLayerWindow(WlrLayerShell *layerShell, QWindow *parent)
    : QQuickWindow(parent)
    , m_layerShell(layerShell)
{
    Q_ASSERT(layerShell);

    setTitle(QStringLiteral("SLM Dock Surface"));
    setColor(Qt::transparent);
    setFlags(Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);

    // Layer-shell windows must not have a transient parent — they are top-level.
    setTransientParent(nullptr);

    // Retry configuring if the Wayland native surface isn't ready immediately.
    m_retryTimer.setInterval(50);
    m_retryTimer.setSingleShot(false);
    connect(&m_retryTimer, &QTimer::timeout, this, &DockLayerWindow::tryConfigureLayerSurface);

    // Connect to layerShell activation in case the protocol isn't ready yet.
    connect(layerShell, &WlrLayerShell::activeChanged, this, [this]() {
        if (!m_layerConfigured) {
            tryConfigureLayerSurface();
        }
    });
}

DockLayerWindow::~DockLayerWindow() = default;

bool DockLayerWindow::isLayerConfigured() const
{
    return m_layerConfigured;
}

bool DockLayerWindow::isLayerShellSupported() const
{
    return m_layerShell && m_layerShell->isActive();
}

int DockLayerWindow::exclusiveZone() const
{
    return m_exclusiveZone;
}

void DockLayerWindow::setExclusiveZone(int zone)
{
    if (m_exclusiveZone == zone) {
        return;
    }
    m_exclusiveZone = zone;
    emit exclusiveZoneChanged();
    // If already configured, we can't easily update — for now log a warning.
    // Future: support set_exclusive_zone updates.
    if (m_layerConfigured) {
        qDebug() << "[DockLayerWindow] exclusiveZone changed after config — zone=" << zone;
    }
}

void DockLayerWindow::exposeEvent(QExposeEvent *event)
{
    QQuickWindow::exposeEvent(event);
    if (!m_layerConfigured && isExposed()) {
        tryConfigureLayerSurface();
    }
}

void DockLayerWindow::tryConfigureLayerSurface()
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

    // Layer TOP puts dock above normal windows and Launchpad overlays.
    // AnchorBottom pins the surface to the bottom of the screen.
    // AnchorLeft | AnchorRight stretches the dock horizontally.
    constexpr int layer   = WlrLayerShell::LayerTop;
    constexpr int anchors = WlrLayerShell::AnchorBottom
                            | WlrLayerShell::AnchorLeft
                            | WlrLayerShell::AnchorRight;

    if (m_layerShell->configureAsLayerSurface(this, layer, anchors,
                                               m_exclusiveZone,
                                               QStringLiteral("slm-dock"))) {
        m_retryTimer.stop();
        m_layerConfigured = true;
        emit layerConfiguredChanged();
        qInfo() << "[DockLayerWindow] layer surface configured";
    } else {
        if (!m_retryTimer.isActive()) {
            m_retryTimer.start();
        }
    }
}
