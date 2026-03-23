import QtQuick
import QtQuick.Templates as T
import Slm_Desktop

T.Switch {
    id: control
    readonly property int transitionDuration: 220

    spacing: 8
    padding: 0

    implicitWidth: indicator.implicitWidth + leftPadding + rightPadding +
                   (text.length > 0 ? spacing + contentItem.implicitWidth : 0)
    implicitHeight: Math.max(indicator.implicitHeight, contentItem.implicitHeight) +
                    topPadding + bottomPadding

    indicator: Rectangle {
        implicitWidth: 38
        implicitHeight: 24
        radius: height / 2
        color: !control.enabled ? Theme.color("controlDisabledBg")
              : (control.checked ? Theme.color("accent") : Theme.color("controlDisabledBg"))
        border.width: control.checked ? 0 : Theme.borderWidthThin
        border.color: control.enabled ? Theme.color("controlDisabledBorder") : Theme.color("controlDisabledBorder")
        opacity: 1.0
        y: Math.round((control.height - height) / 2)

        Behavior on color {
            ColorAnimation {
                duration: control.transitionDuration
                easing.type: Easing.InOutQuad
            }
        }

        Rectangle {
            width: 20
            height: 20
            radius: width / 2
            y: 2
            x: control.checked ? parent.width - width - 2 : 2
            color: control.enabled ? Theme.color("windowCard") : Theme.color("controlDisabledBorder")

            Behavior on x {
                NumberAnimation {
                    duration: control.transitionDuration
                    easing.type: Easing.OutCubic
                }
            }
        }
    }

    contentItem: Text {
        text: control.text
        color: control.enabled ? Theme.color("textPrimary") : Theme.color("textDisabled")
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        opacity: 1.0
        leftPadding: control.indicator.width + control.spacing
    }
}
