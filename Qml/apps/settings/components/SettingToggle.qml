import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop
import SlmStyle as DSStyle

DSStyle.Switch {
    id: control

    function microAnimationAllowed() {
        if (!Theme.animationsEnabled) return false
        if (typeof MotionController === "undefined" || !MotionController || !MotionController.allowMotionPriority) return true
        return MotionController.allowMotionPriority(MotionController.LowPriority)
    }

    indicator: Rectangle {
        implicitWidth: 38
        implicitHeight: 22
        x: control.leftPadding
        y: parent.height / 2 - height / 2
        radius: Theme.radiusLg
        color: control.checked ? Theme.color("accent") : Theme.color("controlDisabledBg")

        Behavior on color {
            enabled: control.microAnimationAllowed()
            ColorAnimation { duration: Theme.durationMd; easing.type: Theme.easingDefault }
        }

        Rectangle {
            x: control.checked ? parent.width - width - 2 : 2
            y: 2
            width: 18
            height: 18
            radius: Theme.radiusLg - 2
            color: "white"

            Behavior on x {
                enabled: control.microAnimationAllowed()
                NumberAnimation { duration: Theme.durationMd; easing.type: Theme.easingSpring }
            }
        }
    }

    contentItem: Item {}
}
