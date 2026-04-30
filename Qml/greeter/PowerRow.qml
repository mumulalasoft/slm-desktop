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
    property bool   busy: false

    signal actionTriggered()

    Button {
        id: actionButton
        width: Math.round(48 * root.uiScale)
        height: width
        enabled: parent.enabled
        onClicked: root.actionTriggered()
        hoverEnabled: true
        background: Rectangle {
            radius: width / 2
            color: !actionButton.enabled ? Qt.rgba(1, 1, 1, 0.055)
                   : (actionButton.down ? Qt.rgba(1, 1, 1, 0.267)
                      : (actionButton.hovered ? Qt.rgba(1, 1, 1, 0.157) : Qt.rgba(1, 1, 1, 0.055)))
            border.width: Theme.borderWidthThin
            border.color: actionButton.enabled
                          ? (actionButton.hovered ? Qt.rgba(1, 1, 1, 0.933) : Qt.rgba(1, 1, 1, 0.627))
                          : Qt.rgba(1, 1, 1, 0.188)
            Behavior on color { ColorAnimation { duration: Theme.durationSm; easing.type: Theme.easingStandard } }
        }
        contentItem: Text {
            text: root.icon
            color: actionButton.enabled ? Qt.rgba(1, 1, 1, 0.961) : Qt.rgba(1, 1, 1, 0.345)
            font.pixelSize: Math.round(24 * root.uiScale)
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        ToolTip.visible: actionButton.hovered
        ToolTip.delay: 450
        ToolTip.text: root.label
    }

    Label {
        text: root.label
        color: root.enabled ? Qt.rgba(1, 1, 1, 0.847) : Qt.rgba(1, 1, 1, 0.361)
        anchors.horizontalCenter: parent.horizontalCenter
        font.pixelSize: Math.round(14 * root.uiScale)
    }
}
