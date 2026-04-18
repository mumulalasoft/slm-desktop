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
    property string iconName: ""
    property string iconSource: ""

    Rectangle {
        id: bubble
        x: Math.max(8, Math.min(parent.width - width - 8, dragGhost.ghostX + 12))
        y: Math.max(8, Math.min(parent.height - height - 8, dragGhost.ghostY + 12))
        width: Math.min(320, ghostText.implicitWidth + 58)
        height: 38
        radius: Theme.radiusControl
        color: Theme.color("windowCard")
        border.width: Theme.borderWidthThin
        border.color: dragGhost.copyMode ? Theme.color("accent")
                                         : Theme.color("selectedItem")
        opacity: Theme.opacityElevated

        Image {
            id: ghostIcon
            width: 18
            height: 18
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            cache: true
            source: {
                if (String(dragGhost.iconName || "").length > 0) {
                    var rev = ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                               ? ThemeIconController.revision : 0)
                    return "image://themeicon/" + dragGhost.iconName + "?v=" + rev
                }
                if (String(dragGhost.iconSource || "").length > 0) {
                    return dragGhost.iconSource
                }
                var fallbackRev = ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                                   ? ThemeIconController.revision : 0)
                return "image://themeicon/text-x-generic-symbolic?v=" + fallbackRev
            }
        }

        Text {
            id: ghostText
            anchors.left: ghostIcon.right
            anchors.leftMargin: 8
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            text: (dragGhost.copyMode ? "Copy: " : "Move: ") + String(
                      dragGhost.label || "item")
            color: Theme.color("textPrimary")
            font.pixelSize: Theme.fontSize("small")
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignLeft
        }
    }
}
