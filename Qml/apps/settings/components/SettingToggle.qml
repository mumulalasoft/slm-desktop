import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop
import SlmStyle as DSStyle

DSStyle.Switch {
    id: control

    indicator: Rectangle {
        implicitWidth: 38
        implicitHeight: 22
        x: control.leftPadding
        y: parent.height / 2 - height / 2
        radius: 11
        color: control.checked ? Theme.color("accent") : Theme.color("controlDisabledBg")

        Behavior on color {
            ColorAnimation { duration: Theme.durationMd; easing.type: Theme.easingDefault }
        }

        Rectangle {
            x: control.checked ? parent.width - width - 2 : 2
            y: 2
            width: 18
            height: 18
            radius: 9
            color: "white"

            Behavior on x {
                NumberAnimation { duration: Theme.durationMd; easing.type: Theme.easingSpring }
            }
        }
    }

    contentItem: Item {}
}
