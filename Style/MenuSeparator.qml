import QtQuick
import QtQuick.Templates as T
import Slm_Desktop

T.MenuSeparator {
    id: control

    implicitWidth: 188
    implicitHeight: 9
    leftPadding: 8
    rightPadding: 8
    topPadding: 4
    bottomPadding: 4

    contentItem: Rectangle {
        implicitHeight: 1
        radius: 1
        color: Theme.color("menuSeparator")
        opacity: Theme.opacitySeparator
    }
}
