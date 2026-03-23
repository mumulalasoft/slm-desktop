import QtQuick
import QtQuick.Templates as T
import Slm_Desktop

T.RadioButton {
    id: control

    spacing: 8

    indicator: Rectangle {
        implicitWidth: 18
        implicitHeight: 18
        radius: width / 2
        color: "transparent"
        border.width: 2
        border.color: control.enabled ? Theme.color("fileManagerControlBorder") : Theme.color("controlDisabledBorder")
        anchors.verticalCenter: parent.verticalCenter

        Rectangle {
            width: 8
            height: 8
            radius: width / 2
            anchors.centerIn: parent
            color: control.enabled ? Theme.color("accent") : Theme.color("controlDisabledBorder")
            visible: control.checked
        }
    }

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.enabled ? Theme.color("textPrimary") : Theme.color("textDisabled")
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
    }
}
