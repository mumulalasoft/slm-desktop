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
        implicitWidth: 42
        implicitHeight: 26
        radius: height / 2
        color: !control.enabled ? Theme.color("controlDisabledBg")
              : (control.checked ? Theme.color("accent") : Theme.color("controlBg"))
        border.width: Theme.borderWidthThin
        border.color: !control.enabled ? Theme.color("controlDisabledBorder")
                                       : (control.checked ? Qt.darker(Theme.color("accent"), Theme.darkMode ? 1.22 : 1.12)
                                                          : Theme.color("panelBorder"))
        opacity: 1.0
        y: Math.round((control.height - height) / 2)

        Behavior on color {
            ColorAnimation {
                duration: control.transitionDuration
                easing.type: Easing.InOutQuad
            }
        }

        Rectangle {
            width: 22
            height: 22
            radius: width / 2
            y: 2
            x: control.checked ? parent.width - width - 2 : 2
            color: control.enabled ? Theme.color("surface") : Theme.color("controlDisabledBorder")
            border.width: Theme.borderWidthThin
            border.color: Qt.rgba(0, 0, 0, Theme.darkMode ? 0.18 : 0.10)

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
