import QtQuick
import QtQuick.Templates as T
import Slm_Desktop

T.SpinBox {
    id: control
    readonly property int transitionDuration: Math.round(Theme.transitionDuration * 0.65)

    implicitWidth: 120
    implicitHeight: Theme.metric("controlHeightLarge")
    leftPadding: 8
    rightPadding: 40

    validator: IntValidator {
        bottom: Math.min(control.from, control.to)
        top: Math.max(control.from, control.to)
    }

    contentItem: TextInput {
        z: 2
        text: control.displayText
        font: control.font
        color: Theme.color("textPrimary")
        horizontalAlignment: Qt.AlignLeft
        verticalAlignment: Qt.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        selectByMouse: true
    }

    up.indicator: Rectangle {
        x: control.width - width - 4
        y: 4
        implicitWidth: 28
        implicitHeight: Math.round((control.height - 12) / 2)
        radius: 6
        color: control.up.pressed ? Theme.color("fileManagerControlActive")
                                  : Theme.color("fileManagerControlBg")

        Text {
            anchors.centerIn: parent
            text: "▲"
            font.pixelSize: Theme.fontSize("tiny")
            color: Theme.color("textPrimary")
        }
    }

    down.indicator: Rectangle {
        x: control.width - width - 4
        y: control.height - height - 4
        implicitWidth: 28
        implicitHeight: Math.round((control.height - 12) / 2)
        radius: 6
        color: control.down.pressed ? Theme.color("fileManagerControlActive")
                                    : Theme.color("fileManagerControlBg")

        Text {
            anchors.centerIn: parent
            text: "▼"
            font.pixelSize: Theme.fontSize("tiny")
            color: Theme.color("textPrimary")
        }
    }

    background: Rectangle {
        radius: Theme.radiusControlLarge
        color: Theme.color("fileManagerControlBg")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("fileManagerControlBorder")
    }
}
