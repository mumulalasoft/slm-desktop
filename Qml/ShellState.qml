pragma Singleton

import QtQuick 2.15

// Centralized shell mode state — thin reactive view over ShellStateController (C++).
//
// ShellStateController (C++) is the sole writer and source of truth.
// QML components read from this singleton; overlay/layer components must NOT
// bind directly to DesktopScene or Main properties.
//
// Phase 2 (current): all property reads/writes go through ShellStateController.
//   DesktopScene / Main.qml call ShellStateController.setXxx() instead of
//   writing local booleans.
//
QtObject {
    id: root

    // ── Overlay visibility ────────────────────────────────────────────────────
    // Read-only from QML; backed by ShellStateController C++ object.
    readonly property bool launchpadVisible:         ShellStateController ? ShellStateController.launchpadVisible : false
    readonly property bool workspaceOverviewVisible: ShellStateController ? ShellStateController.workspaceOverviewVisible : false
    readonly property bool toTheSpotVisible:         ShellStateController ? ShellStateController.toTheSpotVisible : false
    readonly property bool styleGalleryVisible:      ShellStateController ? ShellStateController.styleGalleryVisible : false

    // ── Shell mode flags ──────────────────────────────────────────────────────
    readonly property bool showDesktop: ShellStateController ? ShellStateController.showDesktop : false

    // ── Derived per-layer state ───────────────────────────────────────────────
    // Computed in C++ and exposed via ShellStateController properties.

    // TopBarLayer: always visible — opacity dims slightly while launchpad is open.
    readonly property real topBarOpacity:   ShellStateController ? ShellStateController.topBarOpacity : 1.0

    // DockLayer: hidden while launchpad or show-desktop is active.
    readonly property real dockOpacity:     ShellStateController ? ShellStateController.dockOpacity : 1.0

    // WorkspaceLayer: blurred during fullscreen overlays.
    readonly property bool workspaceBlurred:      ShellStateController ? ShellStateController.workspaceBlurred : false
    readonly property real workspaceBlurAlpha:    ShellStateController ? ShellStateController.workspaceBlurAlpha : 0.0
    readonly property bool workspaceInteractionBlocked: ShellStateController ? ShellStateController.workspaceInteractionBlocked : false

    // ── Convenience predicates ────────────────────────────────────────────────
    readonly property bool anyOverlayVisible: ShellStateController ? ShellStateController.anyOverlayVisible : false
}
