import QtQuick 2.15
import QtQuick.Window 2.15
import Slm_Desktop
import "../dock" as DockComp

// DockWindow is a frameless, non-focusable, always-on-top Window so KWin
// stacks it above every other surface — including overlay Windows like
// LaunchpadWindow.  Using a single Window instance means dock hover/zoom
// state is never duplicated or reset on launchpad open/close.
Window {
    id: root

    required property var rootWindow
    required property var desktopScene
    required property var appsModel
    readonly property alias dockItem: dockSurface

    readonly property int zoomHeadroom: 76
    readonly property bool headroomActive: dockSurface.hovered
                                           || (desktopScene ? desktopScene.pointerNearDock : false)

    visible: !!rootWindow && rootWindow.visible
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowDoesNotAcceptFocus | Qt.WindowStaysOnTopHint
    transientParent: rootWindow
    title: "Desktop Dock"

    width:  Math.max(1, Math.ceil(dockSurface.width))
    height: Math.max(1, Math.ceil(dockSurface.height + (headroomActive ? zoomHeadroom : 0)))
    x: rootWindow ? Math.round(rootWindow.x + (rootWindow.width  - width)  / 2) : 0
    y: rootWindow ? Math.round(rootWindow.y +  rootWindow.height - height
                               - (desktopScene ? desktopScene.dockBottomMargin : 0)) : 0

    DockComp.Dock {
        id: dockSurface
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        opacity: ShellState.dockOpacity
        appsModel: root.appsModel

        Behavior on opacity {
            NumberAnimation {
                duration: Theme.durationFast
                easing.type: Theme.easingLight
            }
        }
    }
}
