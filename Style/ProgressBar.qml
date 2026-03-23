import QtQuick
import QtQuick.Templates as T
import Slm_Desktop

T.ProgressBar {
    id: control

    implicitWidth: 200
    implicitHeight: 8

    background: Rectangle {
        radius: height / 2
        color: Theme.color("controlDisabledBorder")
    }

    contentItem: Rectangle {
        radius: height / 2
        color: Theme.color("accent")
        width: control.position * control.width
    }
}
