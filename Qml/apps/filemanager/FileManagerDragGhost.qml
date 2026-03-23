import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

Item {
    id: dragGhost
    anchors.fill: parent
    z: 1000
    visible: active

    property bool active: false
    property bool copyMode: false
    property real ghostX: 0
    property real ghostY: 0
    property string label: ""

    Rectangle {
        id: bubble
        x: Math.max(8, Math.min(parent.width - width - 8, dragGhost.ghostX + 12))
        y: Math.max(8, Math.min(parent.height - height - 8, dragGhost.ghostY + 12))
        width: Math.min(280, ghostText.implicitWidth + 22)
        height: 30
        radius: Theme.radiusControl
        color: Theme.color("windowCard")
        border.width: Theme.borderWidthThin
        border.color: dragGhost.copyMode ? Theme.color("accent")
                                         : Theme.color("selectedItem")
        opacity: Theme.opacityElevated

        Text {
            id: ghostText
            anchors.centerIn: parent
            text: (dragGhost.copyMode ? "Copy: " : "Move: ") + String(
                      dragGhost.label || "item")
            color: Theme.color("textPrimary")
            font.pixelSize: Theme.fontSize("small")
            elide: Text.ElideRight
            width: parent.width - 14
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
