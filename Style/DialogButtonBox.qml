import QtQuick
import QtQuick.Controls
import QtQuick.Templates as T
import Slm_Desktop

T.DialogButtonBox {
    id: control

    spacing: 8
    padding: Theme.metric("spacingSm")
    alignment: Qt.AlignRight

    delegate: Button {}

    contentItem: ListView {
        orientation: ListView.Horizontal
        spacing: control.spacing
        model: control.contentModel
        interactive: false
    }

    background: Rectangle {
        color: "transparent"
        border.width: 0
        radius: Theme.radiusControl
    }
}
