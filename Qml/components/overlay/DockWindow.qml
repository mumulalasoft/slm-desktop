import QtQuick 2.15
import Slm_Desktop
import "../dock" as DockComp

Item {
    id: root

    required property var rootWindow
    required property var desktopScene
    required property var appsModel
    readonly property alias dockItem: dockSurface

    // DockLayer is persistent — visibility never toggled by mode.
    // Opacity driven by ShellState: hidden only during show-desktop.
    visible: !!rootWindow && rootWindow.visible
    z: ShellZOrder.dock
    readonly property int zoomHeadroom: 76
    readonly property bool headroomActive: dockSurface.hovered
                                           || (desktopScene ? desktopScene.pointerNearDock : false)
    x: Math.round(((rootWindow ? rootWindow.width : width) - width) / 2)
    y: Math.round((rootWindow ? rootWindow.height : height) - height
                  - (desktopScene ? desktopScene.dockBottomMargin : 0))
    width: Math.max(1, Math.ceil(dockSurface.width))
    height: Math.max(1, Math.ceil(dockSurface.height + (headroomActive ? zoomHeadroom : 0)))

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
