pragma Singleton

import QtQuick 2.15

// Centralized shell mode state — single source of truth for overlay visibility
// and per-layer derived state.
//
// Phase 1 (current): DesktopScene / Main.qml write here on state change.
//   Overlay components read here instead of directly binding to desktopScene.
// Phase 2 (future): ShellStateController becomes the sole writer; all callers
//   go through requestMode() / toggleOverlay() instead of setting these directly.
//
// NOTE: Use ShellState.xxx bindings in overlay components, not desktopScene.xxx,
//   so the binding point can be migrated in Phase 2 without touching every file.
//
QtObject {
    id: root

    // ── Overlay visibility ────────────────────────────────────────────────────
    // Written by DesktopScene.qml and Main.qml on every state change.
    property bool launchpadVisible:          false
    property bool workspaceOverviewVisible:  false
    property bool toTheSpotVisible:          false
    property bool styleGalleryVisible:       false

    // ── Shell mode flags ──────────────────────────────────────────────────────
    property bool showDesktop: false

    // ── Derived per-layer state ───────────────────────────────────────────────
    // These are computed from the overlay flags above; overlay components should
    // bind to these rather than recomputing the same logic independently.

    // TopBarLayer: always visible — opacity dims slightly while launchpad is open
    // so it blends with the launchpad frosted backdrop.
    readonly property real topBarOpacity: launchpadVisible ? 0.72 : 1.0

    // DockLayer: visible in all modes; dims while launchpad occupies full screen.
    readonly property real dockOpacity: {
        if (showDesktop) return 0.0
        if (launchpadVisible) return 0.0  // LaunchpadWindow shows its own dock
        return 1.0
    }

    // WorkspaceLayer: blurred during fullscreen overlays.
    readonly property bool workspaceBlurred: launchpadVisible || showDesktop
    readonly property real workspaceBlurAlpha: {
        if (launchpadVisible) return 0.50
        if (showDesktop) return 0.40
        return 0.0
    }
    // Workspace interaction blocked only during fullscreen overlays (not overview,
    // where users can still drag windows).
    readonly property bool workspaceInteractionBlocked: launchpadVisible

    // ── Convenience predicates ────────────────────────────────────────────────
    readonly property bool anyOverlayVisible:
        launchpadVisible || workspaceOverviewVisible || toTheSpotVisible || styleGalleryVisible
}
