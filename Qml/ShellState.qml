pragma Singleton

import QtQuick 2.15

// Centralized shell mode state — thin reactive view over ShellStateController (C++).
//
// ShellStateController (C++) is the sole writer and source of truth.
// QML components read from this singleton; overlay/layer components must NOT
// bind directly to DesktopScene or Main properties.
//
// Hierarchy:
//   ShellState
//   ├─ overlayState   — transient overlays (launchpad, overview, toTheSpot, notifications)
//   ├─ modeState      — persistent shell modes (showDesktop, focusMode)
//   ├─ dockState      — dock visibility/opacity
//   ├─ topBarState    — top bar visibility/opacity
//   └─ workspaceState — workspace blur, interaction
//
QtObject {
    id: root

    // ── Sub-objects ───────────────────────────────────────────────────────────

    readonly property QtObject overlayState: QtObject {
        readonly property bool launchpadVisible:         ShellStateController ? ShellStateController.launchpadVisible : false
        readonly property bool overviewVisible:          ShellStateController ? ShellStateController.workspaceOverviewVisible : false
        readonly property bool toTheSpotVisible:         ShellStateController ? ShellStateController.toTheSpotVisible : false
        readonly property bool notificationsVisible:     ShellStateController ? ShellStateController.notificationsVisible : false
        readonly property bool styleGalleryVisible:      ShellStateController ? ShellStateController.styleGalleryVisible : false
        readonly property bool anyVisible:               ShellStateController ? ShellStateController.anyOverlayVisible : false
    }

    readonly property QtObject modeState: QtObject {
        readonly property bool showDesktop: ShellStateController ? ShellStateController.showDesktop : false
        readonly property bool focusMode:   ShellStateController ? ShellStateController.focusMode : false
        readonly property bool lockScreen:  ShellStateController ? ShellStateController.lockScreenActive : false
    }

    readonly property QtObject dockState: QtObject {
        readonly property real opacity: ShellStateController ? ShellStateController.dockOpacity : 1.0
    }

    readonly property QtObject topBarState: QtObject {
        readonly property real opacity: ShellStateController ? ShellStateController.topBarOpacity : 1.0
    }

    readonly property QtObject workspaceState: QtObject {
        readonly property bool blurred:             ShellStateController ? ShellStateController.workspaceBlurred : false
        readonly property real blurAlpha:           ShellStateController ? ShellStateController.workspaceBlurAlpha : 0.0
        readonly property bool interactionBlocked:  ShellStateController ? ShellStateController.workspaceInteractionBlocked : false
    }

    // ── Backward-compatible flat aliases ─────────────────────────────────────
    // Keep existing bindings working; new code should use sub-objects above.

    readonly property bool launchpadVisible:         overlayState.launchpadVisible
    readonly property bool workspaceOverviewVisible: overlayState.overviewVisible
    readonly property bool toTheSpotVisible:         overlayState.toTheSpotVisible
    readonly property bool styleGalleryVisible:      overlayState.styleGalleryVisible
    readonly property bool showDesktop:              modeState.showDesktop
    readonly property bool anyOverlayVisible:        overlayState.anyVisible

    readonly property real topBarOpacity:            topBarState.opacity
    readonly property real dockOpacity:              dockState.opacity

    readonly property bool workspaceBlurred:         workspaceState.blurred
    readonly property real workspaceBlurAlpha:       workspaceState.blurAlpha
    readonly property bool workspaceInteractionBlocked: workspaceState.interactionBlocked
}
