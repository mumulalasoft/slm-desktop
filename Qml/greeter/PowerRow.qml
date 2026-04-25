import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

// Single power/action button with icon + label below.
Column {
    id: root
    spacing: Math.round(8 * uiScale)

    property string icon: ""
    property string label: ""
    property real   uiScale: 1.0

    signal actionTriggered()

    Button {
        width: Math.round(48 * root.uiScale)
        height: width
        enabled: parent.enabled
        onClicked: root.actionTriggered()
        background: Rectangle {
            radius: width / 2
            color: parent.down ? Qt.rgba(1, 1, 1, 0.267)
                   : (parent.hovered ? Qt.rgba(1, 1, 1, 0.133) : "transparent")
            border.width: Theme.borderWidthThin
            border.color: parent.hovered ? Qt.rgba(1, 1, 1, 0.933) : Qt.rgba(1, 1, 1, 0.800)
            Behavior on color { ColorAnimation { duration: Theme.durationSm; easing.type: Theme.easingStandard } }
        }
        contentItem: Text {
            text: root.icon
            color: Qt.rgba(1, 1, 1, 0.961)
            font.pixelSize: Math.round(24 * root.uiScale)
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    Label {
        text: root.label
        color: Qt.rgba(1, 1, 1, 0.847)
        anchors.horizontalCenter: parent.horizontalCenter
        font.pixelSize: Math.round(14 * root.uiScale)
    }
}
