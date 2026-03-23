import QtQuick
import QtQuick.Templates as T
import Slm_Desktop

T.Button {
    id: control
    readonly property int transitionDuration: Math.round(Theme.transitionDuration * 0.6)
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
                return Qt.darker(Theme.color("accent"), 1.1)
            }
            if (control.hovered) {
                return Qt.lighter(Theme.color("accent"), 1.06)
            }
            return Theme.color("accent")
        }
        if (control.down) {
            return Theme.color("fileManagerControlActive")
        }
        if (control.hovered) {
            return Theme.color("windowCard")
        }
        return Theme.color("fileManagerControlBg")
    }
    readonly property color strokeColor: {
        if (!control.enabled) {
            return Theme.color("controlDisabledBorder")
        }
        if (control.effectiveDefaultAction) {
            return Qt.darker(Theme.color("accent"), 1.12)
        }
        return control.hovered ? Theme.color("focusRing")
                               : Theme.color("fileManagerControlBorder")
    }
    implicitWidth: Math.max(84, implicitContentWidth + leftPadding + rightPadding)
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
        radius: Theme.radiusPill
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: control.fillColor
            }
            GradientStop {
                position: 1.0
                color: control.fillColor
            }
        }
        border.width: Theme.borderWidthThin
        border.color: control.strokeColor
        opacity: 1.0

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 1
            height: Math.max(6, parent.height * 0.42)
            radius: Math.max(4, parent.radius - 1)
            color: Qt.rgba(1, 1, 1, Theme.darkMode ? 0.07 : 0.25)
            visible: !control.down
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -2
            radius: parent.radius + 2
            color: "transparent"
            border.width: 2
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
