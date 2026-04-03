import QtQuick 2.15
import Slm_Desktop
import "../dock" as DockComp

// ShellDockHost — the single dock renderer in the shell's ApplicationWindow scene.
//
// This is the ONLY dock instance.  LaunchpadWindow shortens its height to
// exclude the dock zone, so when the launchpad is open this host remains
// visible and interactive at the bottom of the screen — no second renderer
// is needed and no state is ever duplicated or reset.
Item {
    id: root

    required property var rootWindow
    required property var desktopScene
    required property var appsModel

    // Expose dock surface for external geometry queries (DesktopScene.dockItem).
    readonly property alias dockItem: dockSurface

    // ── z / visibility ──────────────────────────────────────────────────────
    z: ShellZOrder.dock
    visible: !!rootWindow && rootWindow.visible

    // ── Input ───────────────────────────────────────────────────────────────
    // Always enabled — DockSystem.shellAcceptsInput is unconditionally true
    // because the dock zone is not covered by LaunchpadWindow's surface.
    enabled: DockSystem.shellAcceptsInput

    // ── Layout ─────────────────────────────────────────────────────────────
    readonly property bool headroomActive: dockSurface.hovered
                                           || (desktopScene ? desktopScene.pointerNearDock : false)

    width:  Math.max(1, Math.ceil(dockSurface.width))
    height: Math.max(1, Math.ceil(dockSurface.height + (headroomActive ? DockSystem.zoomHeadroom : 0)))
    x: rootWindow ? Math.round((rootWindow.width  - width)  / 2) : 0
    y: rootWindow ? Math.round( rootWindow.height - height
                                - (desktopScene ? desktopScene.dockBottomMargin : 0)) : 0

    // ── Renderer ────────────────────────────────────────────────────────────
    DockComp.Dock {
        id: dockSurface
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        // DockSystem overrides opacity to 1.0 when launchpad is active so
        // the dock stays visible at the bottom while launchpad is open.
        opacity: DockSystem.shellHostOpacity
        appsModel: root.appsModel

        Behavior on opacity {
            NumberAnimation {
                duration: Theme.durationFast
                easing.type: Theme.easingLight
            }
        }
    }
}
