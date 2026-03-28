import QtQuick
import QtQuick.Templates as T
import Slm_Desktop

T.ToolButton {
    id: control
    readonly property int transitionDuration: Math.round(Theme.transitionDuration * 0.55)
    implicitWidth: Math.max(30, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Theme.metric("controlHeightRegular")
    leftPadding: 10
    rightPadding: 10
    topPadding: 5
    bottomPadding: 5

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.enabled ? Theme.color("textPrimary") : Theme.color("textDisabled")
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.radiusControl
        border.width: Theme.borderWidthThin
        border.color: control.enabled ? Theme.color("panelBorder")
                                      : Theme.color("controlDisabledBorder")
        color: !control.enabled ? Theme.color("controlDisabledBg")
              : (control.down ? Theme.color("controlBgPressed")
                              : (control.hovered ? Theme.color("controlBgHover")
                                                 : Theme.color("controlBg")))
        opacity: 1.0

        Rectangle {
            anchors.fill: parent
            anchors.margins: -2
            radius: parent.radius + 2
            color: "transparent"
            border.width: 2
            border.color: Theme.color("focusRing")
            visible: control.visualFocus && control.enabled
        }

        Behavior on color {
            ColorAnimation {
                duration: control.transitionDuration
                easing.type: Easing.InOutQuad
            }
        }
    }
}
