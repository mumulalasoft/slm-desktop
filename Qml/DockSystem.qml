pragma Singleton

import QtQuick 2.15
import Slm_Desktop

// DockSystem — single source of truth for the dock rendering lifecycle.
//
// Architecture: ONE visual renderer (ShellDockHost, a persistent Item in
// ApplicationWindow) is always active.  LaunchpadWindow no longer embeds a
// second dock renderer — instead, LaunchpadWindow's height is shortened to
// exclude the dock zone, so mouse events in the dock zone fall through to
// ApplicationWindow → ShellDockHost directly.
//
// This gives a truly single dock instance: one model, one hover state, one
// animation pipeline, no stale state at transition time.
QtObject {
    id: root

    // ─── Animation phase ───────────────────────────────────────────────────
    readonly property string phase: ShellState.launchpadVisible ? "launchpad" : "idle"
    readonly property bool shellHostActive:     true   // ShellDockHost is ALWAYS active
    readonly property bool launchpadHostActive: phase === "launchpad"  // informational only

    // ─── Input policy ──────────────────────────────────────────────────────
    // ShellDockHost is always enabled — the dock zone is excluded from
    // LaunchpadWindow's surface so events reach ApplicationWindow directly.
    readonly property bool shellAcceptsInput: true

    // ─── Opacity policy ────────────────────────────────────────────────────
    // When launchpad is open the dock stays fully visible (macOS behaviour:
    // dock is interactive at the bottom while launchpad is open above it).
    // All other modes follow ShellState.dockOpacity (show-desktop, etc.).
    readonly property real shellHostOpacity: launchpadHostActive ? 1.0 : ShellState.dockOpacity

    // ─── Layout constants ──────────────────────────────────────────────────
    // Used by ShellDockHost for its container height, and by LaunchpadWindow
    // to compute how much of the bottom to leave uncovered.
    readonly property int zoomHeadroom: 76
}
