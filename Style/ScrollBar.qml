import QtQuick
import QtQuick.Templates as T
import Slm_Desktop

T.ScrollBar {
    id: control

    implicitWidth: orientation === Qt.Vertical ? 10 : 96
    implicitHeight: orientation === Qt.Horizontal ? 10 : 96
    padding: 2

    contentItem: Rectangle {
        radius: width / 2
        color: Theme.color("fileManagerControlBorder")
        opacity: control.pressed ? 1 : 0.86
    }

    background: Rectangle {
        radius: width / 2
        color: Theme.color("controlDisabledBg")
        visible: control.policy !== T.ScrollBar.AlwaysOff
    }
}
