import QtQuick
import Slm_Desktop

Item {
    id: root
    default property alias contentData: contentLayer.data

    property real popupRadius: Theme.radiusPopup
    property color popupColor: Theme.color("menuBg")
    property color popupBorderColor: Theme.color("menuBorder")
    property real popupOpacity: Theme.popupSurfaceOpacityStrong
    property string elevation: "medium" // low | medium | high

    implicitWidth: 240
    implicitHeight: 40

    function getShadowToken() {
        if (elevation === "high") {
            return Theme.shadowLg
        }
        if (elevation === "low") {
            return Theme.shadowSm
        }
        return Theme.shadowMd
    }

    Rectangle {
        property var shadowT: root.getShadowToken()
        anchors.fill: parent
        anchors.margins: -(shadowT.radius * 0.22)
        radius: root.popupRadius + shadowT.radius * 0.22
        color: "transparent"
        border.width: shadowT.radius
        border.color: Qt.rgba(0, 0, 0, shadowT.opacity * (Theme.darkMode ? 1.6 : 1.0))
        opacity: 0.45
        z: 0
    }

    Rectangle {
        anchors.fill: parent
        radius: root.popupRadius
        antialiasing: true
        color: Qt.rgba(root.popupColor.r, root.popupColor.g, root.popupColor.b, 1.0)
        border.width: Theme.borderWidthThin
        border.color: root.popupBorderColor
        opacity: 1.0
        z: 1
    }

    Item {
        id: contentLayer
        anchors.fill: parent
        z: 2
    }
}
