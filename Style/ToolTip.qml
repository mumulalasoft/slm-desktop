import QtQuick
import QtQuick.Templates as T
import Slm_Desktop

T.ToolTip {
    id: control

    padding: 8
    delay: 350
    timeout: 5000
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

    contentItem: Text {
        text: control.text
        font: control.font
        color: Theme.color("textPrimary")
        wrapMode: Text.Wrap
    }

    background: Rectangle {
        radius: Theme.radiusControl
        color: Theme.color("windowCard")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("windowCardBorder")
        opacity: 0.96
    }
}
