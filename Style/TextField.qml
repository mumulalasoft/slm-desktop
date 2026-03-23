import QtQuick
import QtQuick.Templates as T
import Slm_Desktop

T.TextField {
    id: control
    readonly property int transitionDuration: Math.round(Theme.transitionDuration * 0.7)

    implicitWidth: 180
    implicitHeight: Theme.metric("controlHeightLarge")
    leftPadding: 10
    rightPadding: 10
    topPadding: 6
    bottomPadding: 6
    color: control.enabled ? Theme.color("textPrimary") : Theme.color("textDisabled")
    placeholderTextColor: control.enabled ? Theme.color("textSecondary") : Theme.color("textDisabled")
    selectedTextColor: Theme.color("accentText")
    selectionColor: Theme.color("accent")

    background: Rectangle {
        radius: Theme.radiusControlLarge
        color: control.enabled ? Theme.color("fileManagerControlBg") : Theme.color("controlDisabledBg")
        border.width: control.activeFocus ? Theme.borderWidthThick : Theme.borderWidthThin
        border.color: control.activeFocus ? Theme.color("focusRingStrong")
                                          : (control.enabled ? Theme.color("fileManagerControlBorder")
                                                             : Theme.color("controlDisabledBorder"))
        opacity: 1.0

        Rectangle {
            anchors.fill: parent
            anchors.margins: -2
            radius: parent.radius + 2
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
