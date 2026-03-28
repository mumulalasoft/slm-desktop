import QtQuick
import QtQuick.Templates as T
import Slm_Desktop

T.CheckBox {
    id: control

    spacing: 8

    indicator: Rectangle {
        implicitWidth: 18
        implicitHeight: 18
        radius: Theme.radiusSmPlus
        color: !control.enabled ? Theme.color("controlDisabledBg")
              : (control.checked ? Theme.color("accent") : Theme.color("controlBg"))
        border.width: Theme.borderWidthThin
        border.color: control.enabled
                      ? (control.checked ? Qt.darker(Theme.color("accent"), Theme.darkMode ? 1.22 : 1.12)
                                         : Theme.color("panelBorder"))
                      : Theme.color("controlDisabledBorder")
        anchors.verticalCenter: parent.verticalCenter

        Text {
            anchors.centerIn: parent
            text: "✓"
            visible: control.checked
            color: Theme.color("accentText")
            font.pixelSize: Theme.fontSize("small")
            font.bold: true
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
