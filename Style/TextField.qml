import QtQuick
import QtQuick.Templates as T
import Slm_Desktop

T.TextField {
    id: control
    readonly property int transitionDuration: Math.round(Theme.transitionDuration * 0.7)

    implicitWidth: 200
    implicitHeight: Theme.metric("controlHeightRegular")
    leftPadding: 10
    rightPadding: 10
    topPadding: 5
    bottomPadding: 5
    color: control.enabled ? Theme.color("textPrimary") : Theme.color("textDisabled")
    placeholderTextColor: control.enabled ? Theme.color("textSecondary") : Theme.color("textDisabled")
    selectedTextColor: Theme.color("accentText")
    selectionColor: Theme.color("accent")

    background: Rectangle {
        radius: Theme.radiusControl
        color: control.enabled ? Theme.color("controlBg") : Theme.color("controlDisabledBg")
        border.width: Theme.borderWidthThin
        border.color: control.activeFocus ? Theme.color("accent")
                                          : (control.enabled ? Theme.color("panelBorder") : Theme.color("controlDisabledBorder"))
        opacity: 1.0

        Rectangle {
            anchors.fill: parent
            anchors.margins: -1
            radius: parent.radius + 1
            color: "transparent"
            border.width: 1
            border.color: Theme.color("focusRing")
            visible: control.activeFocus && control.enabled
        }

        Behavior on border.color {
            ColorAnimation {
                duration: control.transitionDuration
                easing.type: Easing.InOutQuad
            }
        }
    }
}
