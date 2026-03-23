import QtQuick 2.15
import QtQuick.Window 2.15
import Slm_Desktop

Window {
    id: root

    property var parentWindow: null
    property bool contextOpen: false
    property real contextMenuX: 0
    property real contextMenuY: 0
    property int contextMenuWidth: 190
    property int contextMenuHeight: 40
    property double openedAtMs: 0

    signal requestClose()
    signal requestArrangeShortcut()
    signal requestReopenAt(real x, real y)

    visible: !!parentWindow && parentWindow.visible && contextOpen
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowDoesNotAcceptFocus
    transientParent: parentWindow
    title: "Slm Desktop Context"
    x: parentWindow ? parentWindow.x : 0
    y: parentWindow ? parentWindow.y : 0
    width: parentWindow ? parentWindow.width : 0
    height: parentWindow ? parentWindow.height : 0

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        onPressed: function(mouse) {
            if ((Date.now() - root.openedAtMs) < 120) {
                mouse.accepted = true
                return
            }
            var inside = mouse.x >= root.contextMenuX &&
                         mouse.x <= root.contextMenuX + root.contextMenuWidth &&
                         mouse.y >= root.contextMenuY &&
                         mouse.y <= root.contextMenuY + root.contextMenuHeight
            if (!inside) {
                if (mouse.button === Qt.RightButton) {
                    root.requestReopenAt(mouse.x, mouse.y)
                } else {
                    root.requestClose()
                }
            }
            mouse.accepted = true
        }
    }

    Rectangle {
        x: root.contextMenuX
        y: root.contextMenuY
        width: root.contextMenuWidth
        height: root.contextMenuHeight
        radius: Theme.radiusControlLarge
        color: Theme.color("menuBg")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("menuBorder")

        Rectangle {
            id: arrangeHover
            anchors.fill: parent
            anchors.margins: 3
            radius: Theme.radiusControl
            color: arrangeMouse.containsMouse ? Theme.color("accentSoft") : "transparent"
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 12
            text: "Arrange Shortcut"
            color: Theme.color("textPrimary")
            font.family: Theme.fontFamilyUi
            font.pixelSize: Theme.fontSize("smallPlus")
        }

        MouseArea {
            id: arrangeMouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: root.requestArrangeShortcut()
        }
    }
}
