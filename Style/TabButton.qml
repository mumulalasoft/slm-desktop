import QtQuick
import QtQuick.Templates as T
import Slm_Desktop

T.TabButton {
    id: control
    readonly property int transitionDuration: Math.round(Theme.transitionDuration * 0.65)

    implicitHeight: Theme.metric("controlHeightLarge")
    leftPadding: 12
    rightPadding: 12
    topPadding: 5
    bottomPadding: 5

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.checked ? Theme.color("textPrimary") : Theme.color("textSecondary")
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.radiusControl
        color: control.checked ? Theme.color("fileManagerControlActive")
                               : (control.hovered ? Theme.color("accentSoft") : "transparent")
        border.width: control.checked ? Theme.borderWidthThin : 0
        border.color: control.checked ? Theme.color("fileManagerControlBorder") : "transparent"
        Behavior on color {
            ColorAnimation {
                duration: control.transitionDuration
                easing.type: Easing.InOutQuad
            }
        }
    }
}
