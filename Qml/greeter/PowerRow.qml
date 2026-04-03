import QtQuick 2.15
import QtQuick.Controls 2.15

// Single power/action button with icon + label below, matching SDDM style.
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
            color: parent.down ? "#44ffffff" : "transparent"
            border.width: 1
            border.color: "#ccffffff"
        }
        contentItem: Text {
            text: root.icon
            color: "#f5ffffff"
            font.pixelSize: Math.round(24 * root.uiScale)
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    Label {
        text: root.label
        color: "#d8ffffff"
        anchors.horizontalCenter: parent.horizontalCenter
        font.pixelSize: Math.round(14 * root.uiScale)
    }
}
