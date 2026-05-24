import QtQuick 2.15
import Slm_Desktop

Rectangle {
    id: root

    property bool active: false
    property real markerX: -1000
    property real markerY: 0
    property real draggingDeltaX: 0

    visible: active && markerX > -500
    width: 28
    height: 4
    radius: Theme.radiusTiny
    color: Theme.color("selectedItemText")
    opacity: visible ? 0.96 : 0
    x: markerX
    y: markerY
    z: 120

    Behavior on x {
        enabled: Math.abs(root.draggingDeltaX) > 0.1
        SpringAnimation {
            spring: Theme.physicsSpringSnappy
            damping: Theme.physicsDampingSnappy
            mass: Theme.physicsMassSnappy
        }
    }
    Behavior on opacity {
        NumberAnimation {
            duration: Theme.durationMicro
            easing.type: Theme.easingLight
        }
    }
}
