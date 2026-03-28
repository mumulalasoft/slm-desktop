import QtQuick
import Slm_Desktop

Item {
    id: root
    default property alias contentData: contentLayer.data

    property real popupRadius: Theme.radiusPopup
    property color popupColor: Theme.color("surface")
    property color popupBorderColor: Theme.color("panelBorder")
    property real popupOpacity: Theme.popupSurfaceOpacityStrong
    property string elevation: "medium" // low | medium | high

    implicitWidth: 220
    implicitHeight: 40

    function getShadowToken() {
        if (elevation === "high") {
            return Theme.shadowXl
        }
        if (elevation === "low") {
            return Theme.shadowSm
        }
        return Theme.shadowMd
    }

    Rectangle {
        property var shadowT: root.getShadowToken()
        anchors.fill: parent
        anchors.margins: -(shadowT.radius * 0.14)
        radius: root.popupRadius + shadowT.radius * 0.14
        color: "transparent"
        border.width: shadowT.radius
        border.color: Qt.rgba(0, 0, 0, shadowT.opacity * (Theme.darkMode ? 1.25 : 0.85))
        opacity: Theme.darkMode ? 0.28 : 0.18
        z: 0
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: -1
        radius: root.popupRadius + 1
        color: "transparent"
        border.width: Theme.borderWidthThin
        border.color: Qt.rgba(1, 1, 1, Theme.darkMode ? 0.06 : 0.24)
        z: 1
    }

    Rectangle {
        anchors.fill: parent
        radius: root.popupRadius
        antialiasing: true
        color: Qt.rgba(root.popupColor.r, root.popupColor.g, root.popupColor.b, root.popupOpacity)
        border.width: Theme.borderWidthThin
        border.color: root.popupBorderColor
        z: 2
    }

    Item {
        id: contentLayer
        anchors.fill: parent
        z: 3
    }
}
