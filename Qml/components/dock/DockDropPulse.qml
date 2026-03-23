import QtQuick 2.15
import Slm_Desktop

Rectangle {
    id: root

    property real pulseCenterX: -1000
    property real pulseY: 0
    property real pulseSize: 52
    property real startScale: 0.78

    visible: opacity > 0.01
    width: pulseSize
    height: pulseSize
    radius: width / 2
    color: "transparent"
    border.width: Theme.borderWidthThick
    border.color: Theme.color("dockDropPulseBorder")
    x: pulseCenterX - width * 0.5
    y: pulseY
    z: 130
    opacity: 0
    scale: startScale
    transformOrigin: Item.Center
}
