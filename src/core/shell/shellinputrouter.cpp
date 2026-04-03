#include "shellinputrouter.h"
#include "shellstatecontroller.h"

// ── Action name constants ─────────────────────────────────────────────────────
// These string literals are the canonical action names used in both C++ and QML.
namespace ShellAction {
// Shell overlay / mode toggles
static const char *const FileManager         = "shell.file_manager";
static const char *const Print               = "shell.print";
static const char *const ToTheSpot           = "shell.tothespot";
static const char *const Clipboard           = "shell.clipboard";
static const char *const Settings            = "shell.settings";
static const char *const WorkspaceOverview   = "shell.workspace_overview";
static const char *const Lock                = "shell.lock";
// Workspace navigation (blocked during fullscreen overlays)
static const char *const WorkspacePrev       = "workspace.prev";
static const char *const WorkspaceNext       = "workspace.next";
static const char *const WindowMovePrev      = "window.move_workspace_prev";
static const char *const WindowMoveNext      = "window.move_workspace_next";
// Overlay dismiss
static const char *const OverlayDismiss      = "overlay.dismiss";
// Screenshot (always allowed — no shell-mode guard)
static const char *const ScreenshotArea      = "screenshot.area";
static const char *const ScreenshotFullscreen = "screenshot.fullscreen";
// Debug
static const char *const DebugMotionOverlay  = "debug.motion_overlay";
}

// ── Routing table ─────────────────────────────────────────────────────────────
// Returns true when the action is permitted in the given layer.
// Higher layers (LockScreen) are most restrictive; BaseLayer allows everything.
bool ShellInputRouter::actionAllowedInLayer(const QString &action, ShellLayer layer)
{
    using L = ShellLayer;

    // Lock screen: only lock action is meaningful (others are silently blocked).
    if (layer == L::LockScreen) {
        return action == QLatin1String(ShellAction::Lock);
    }

    // Launchpad fullscreen: workspace navigation is not useful; allow dismiss + safe actions.
    if (layer == L::Launchpad) {
        if (action == QLatin1String(ShellAction::WorkspacePrev)
                || action == QLatin1String(ShellAction::WorkspaceNext)
                || action == QLatin1String(ShellAction::WindowMovePrev)
                || action == QLatin1String(ShellAction::WindowMoveNext)) {
            return false;
        }
        return true;
    }

    // WorkspaceOverview: full navigation allowed; shell overlays blocked while overview is up.
    if (layer == L::WorkspaceOverview) {
        if (action == QLatin1String(ShellAction::ToTheSpot)
                || action == QLatin1String(ShellAction::Clipboard)) {
            return false;
        }
        return true;
    }

    // ToTheSpot: shell mode toggles blocked (only dismiss + screenshot allowed).
    if (layer == L::ToTheSpot) {
        if (action == QLatin1String(ShellAction::WorkspaceOverview)
                || action == QLatin1String(ShellAction::WorkspacePrev)
                || action == QLatin1String(ShellAction::WorkspaceNext)
                || action == QLatin1String(ShellAction::WindowMovePrev)
                || action == QLatin1String(ShellAction::WindowMoveNext)) {
            return false;
        }
        return true;
    }

    // BaseLayer: all registered actions allowed.
    return true;
}

// ── Implementation ────────────────────────────────────────────────────────────

ShellInputRouter::ShellInputRouter(ShellStateController *stateController, QObject *parent)
    : QObject(parent)
    , m_stateController(stateController)
{
    m_dismissTimeoutTimer.setSingleShot(true);
    m_dismissTimeoutTimer.setInterval(m_dismissTimeoutMs);

    QObject::connect(&m_dismissTimeoutTimer, &QTimer::timeout, this, [this]() {
        if (!m_stateController || m_pendingDismissOverlay.isEmpty()) {
            return;
        }
        // Check whether the overlay is still visible — if so, force-dismiss everything.
        bool stillVisible = false;
        const QString ov = m_pendingDismissOverlay;
        if (ov == QLatin1String("launchpad")) {
            stillVisible = m_stateController->launchpadVisible();
        } else if (ov == QLatin1String("workspace")) {
            stillVisible = m_stateController->workspaceOverviewVisible();
        } else if (ov == QLatin1String("tothespot")) {
            stillVisible = m_stateController->toTheSpotVisible();
        } else if (ov == QLatin1String("style_gallery")) {
            stillVisible = m_stateController->styleGalleryVisible();
        }

        m_pendingDismissOverlay.clear();

        if (stillVisible) {
            m_stateController->dismissAllOverlays();
            emit overlayDismissTimedOut(ov);
        }
    });

    if (m_stateController) {
        QObject::connect(m_stateController, &ShellStateController::launchpadVisibleChanged,
                         this, &ShellInputRouter::onStateChanged);
        QObject::connect(m_stateController, &ShellStateController::workspaceOverviewVisibleChanged,
                         this, &ShellInputRouter::onStateChanged);
        QObject::connect(m_stateController, &ShellStateController::toTheSpotVisibleChanged,
                         this, &ShellInputRouter::onStateChanged);
        QObject::connect(m_stateController, &ShellStateController::showDesktopChanged,
                         this, &ShellInputRouter::onStateChanged);
        QObject::connect(m_stateController, &ShellStateController::lockScreenActiveChanged,
                         this, &ShellInputRouter::onStateChanged);
    }
}

ShellInputRouter::ShellLayer ShellInputRouter::activeLayer() const
{
    if (!m_stateController) {
        return ShellLayer::BaseLayer;
    }
    // Evaluate highest-priority active layer.
    if (m_stateController->lockScreenActive()) {
        return ShellLayer::LockScreen;
    }
    if (m_stateController->toTheSpotVisible()) {
        return ShellLayer::ToTheSpot;
    }
    if (m_stateController->launchpadVisible()) {
        return ShellLayer::Launchpad;
    }
    if (m_stateController->workspaceOverviewVisible()) {
        return ShellLayer::WorkspaceOverview;
    }
    return ShellLayer::BaseLayer;
}

bool ShellInputRouter::canDispatch(const QString &action) const
{
    return actionAllowedInLayer(action, activeLayer());
}

ShellInputRouter::DispatchResult ShellInputRouter::dispatch(const QString &action)
{
    const ShellLayer layer = activeLayer();
    if (!actionAllowedInLayer(action, layer)) {
        return DispatchResult::BlockedByMode;
    }
    emit actionDispatched(action);
    return DispatchResult::Dispatched;
}

void ShellInputRouter::scheduleForceDismiss(const QString &overlayName)
{
    m_pendingDismissOverlay = overlayName;
    m_dismissTimeoutTimer.start(m_dismissTimeoutMs);
}

void ShellInputRouter::cancelForceDismiss(const QString &overlayName)
{
    if (m_pendingDismissOverlay == overlayName) {
        m_dismissTimeoutTimer.stop();
        m_pendingDismissOverlay.clear();
    }
}

int ShellInputRouter::dismissTimeoutMs() const
{
    return m_dismissTimeoutMs;
}

void ShellInputRouter::setDismissTimeoutMs(int ms)
{
    m_dismissTimeoutMs = ms;
    m_dismissTimeoutTimer.setInterval(ms);
}

void ShellInputRouter::onStateChanged()
{
    const ShellLayer newLayer = activeLayer();
    if (newLayer != m_currentLayer) {
        m_currentLayer = newLayer;
        emit layerChanged(m_currentLayer);
    }
}
