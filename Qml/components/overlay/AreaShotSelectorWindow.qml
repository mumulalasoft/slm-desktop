import QtQuick 2.15
import QtQuick.Window 2.15
import Slm_Desktop

Window {
    id: root

    property var rootWindow: null
    property bool selecting: false
    property real startX: 0
    property real startY: 0
    property real endX: 0
    property real endY: 0
    property color scrimColor: Theme.color("screenshotScrim")
    property color selectionFillColor: Theme.color("screenshotSelectionFill")
    property color selectionBorderColor: Theme.color("screenshotSelectionBorder")
    property int selectionBorderWidth: Theme.borderWidthThick

    signal cancelRequested()
    signal selectionChanged(real x, real y)
    signal selectionCommitted(real x, real y)

    visible: !!rootWindow && !!rootWindow.visible && selecting
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    transientParent: rootWindow
    title: "Desktop Area Screenshot Selector"
    x: rootWindow ? rootWindow.x : 0
    y: rootWindow ? rootWindow.y : 0
    width: rootWindow ? rootWindow.width : 0
    height: rootWindow ? rootWindow.height : 0

    onVisibleChanged: {
        if (visible) {
            requestActivate()
        }
    }

    Item {
        anchors.fill: parent
        focus: root.visible
        Keys.onEscapePressed: root.cancelRequested()

        Rectangle {
            anchors.fill: parent
            color: root.scrimColor
        }

        Rectangle {
            x: Math.min(root.startX, root.endX)
            y: Math.min(root.startY, root.endY)
            width: Math.abs(root.endX - root.startX)
            height: Math.abs(root.endY - root.startY)
            color: root.selectionFillColor
            border.width: root.selectionBorderWidth
            border.color: root.selectionBorderColor
            visible: width > 0 && height > 0
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            hoverEnabled: true
            cursorShape: Qt.CrossCursor
            onPressed: function(mouse) {
                if (mouse.button === Qt.RightButton) {
                    root.cancelRequested()
                    mouse.accepted = true
                    return
                }
                root.startX = mouse.x
                root.startY = mouse.y
                root.endX = mouse.x
                root.endY = mouse.y
                root.selectionChanged(mouse.x, mouse.y)
                mouse.accepted = true
            }
            onPositionChanged: function(mouse) {
                root.endX = mouse.x
                root.endY = mouse.y
                root.selectionChanged(mouse.x, mouse.y)
            }
            onReleased: function(mouse) {
                if (mouse.button === Qt.LeftButton) {
                    root.endX = mouse.x
                    root.endY = mouse.y
                    root.selectionCommitted(mouse.x, mouse.y)
                }
                mouse.accepted = true
            }
        }
    }
}

