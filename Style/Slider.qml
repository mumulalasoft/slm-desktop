import QtQuick
import QtQuick.Templates as T
import Slm_Desktop

T.Slider {
    id: control
    readonly property int transitionDuration: Math.round(Theme.transitionDuration * 0.6)

    implicitWidth: 180
    implicitHeight: 24
    padding: 0

    background: Item {
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        width: control.availableWidth
        height: 6

        Rectangle {
            anchors.fill: parent
            radius: height / 2
            color: Theme.color("controlDisabledBorder")
        }
        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: height / 2
            color: Theme.color("accent")
        }
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + (control.availableHeight - height) / 2
        implicitWidth: 18
        implicitHeight: 18
        radius: width / 2
        color: Theme.color("windowCard")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("fileManagerControlBorder")

        Behavior on x {
            enabled: !control.pressed
            NumberAnimation {
                duration: control.transitionDuration
                easing.type: Easing.OutCubic
            }
        }
    }
}
