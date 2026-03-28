import QtQuick
import QtQuick.Templates as T
import Slm_Desktop

T.Button {
    id: control
    readonly property int transitionDuration: Math.round(Theme.transitionDuration * 0.55)
    property bool defaultAction: false
    property bool allowAutoDefaultAction: true
    readonly property bool autoDefaultAction: {
        if (!allowAutoDefaultAction) {
            return false
        }
        var t = String(control.text || "").trim().toLowerCase()
        return t === "save"
                || t === "open"
                || t === "apply"
                || t === "connect"
                || t === "replace"
                || t === "select"
                || t === "ok"
                || t === "take screenshot"
    }
    readonly property bool effectiveDefaultAction: (defaultAction || autoDefaultAction)
    readonly property color fillColor: {
        if (!control.enabled) {
            return Theme.color("controlDisabledBg")
        }
        if (control.effectiveDefaultAction) {
            if (control.down) {
                return Qt.darker(Theme.color("accent"), Theme.darkMode ? 1.24 : 1.14)
            }
            if (control.hovered) {
                return Qt.lighter(Theme.color("accent"), Theme.darkMode ? 1.14 : 1.08)
            }
            return Theme.color("accent")
        }
        if (control.down) {
            return Theme.color("controlBgPressed")
        }
        if (control.hovered) {
            return Theme.color("controlBgHover")
        }
        return Theme.color("controlBg")
    }
    readonly property color strokeColor: {
        if (!control.enabled) {
            return Theme.color("controlDisabledBorder")
        }
        if (control.effectiveDefaultAction) {
            return "transparent"
        }
        if (control.hovered) {
            return Theme.color("panelBorderStrong")
        }
        return control.visualFocus ? Theme.color("focusRing") : Theme.color("panelBorder")
    }
    implicitWidth: Math.max(82, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Theme.metric("controlHeightRegular")
    leftPadding: 12
    rightPadding: 12
    topPadding: 5
    bottomPadding: 5

    contentItem: Text {
        text: control.text
        font: control.font
        color: !control.enabled ? Theme.color("textDisabled")
              : (control.effectiveDefaultAction ? Theme.color("accentText") : Theme.color("textPrimary"))
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.radiusControl
        color: control.fillColor
        border.width: control.effectiveDefaultAction ? Theme.borderWidthNone : Theme.borderWidthThin
        border.color: control.strokeColor
        opacity: 1.0

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: Math.round(parent.height * 0.5)
            radius: parent.radius
            color: Qt.rgba(1, 1, 1, control.effectiveDefaultAction ? (Theme.darkMode ? 0.10 : 0.22) : (Theme.darkMode ? 0.04 : 0.12))
            opacity: control.enabled ? 1.0 : 0.0
            visible: control.enabled
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -1
            radius: parent.radius + 1
            color: "transparent"
            border.width: 1
            border.color: Theme.color("focusRing")
            visible: control.visualFocus && control.enabled
        }

        Behavior on border.color {
            ColorAnimation {
                duration: control.transitionDuration
                easing.type: Easing.InOutQuad
            }
        }
    }
}
