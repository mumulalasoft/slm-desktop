import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import "." as PulseComp

PulseComp.PulseSection {
    id: root

    property var model: null
    property int focusedIndex: -1

    signal itemTriggered(int index)

    ListView {
        anchors.fill: parent
        model: root.model
        clip: true
        orientation: ListView.Horizontal
        spacing: 8

        delegate: Rectangle {
            required property int index
            required property string name
            property string label: ""
            property string impact: ""

            readonly property bool focused: root.focusedIndex === index
            width: 220
            height: ListView.view.height
            radius: Theme.radiusControl
            color: focused ? Theme.color("pulseBadgeActive") : Theme.color("pulseBadgeBg")
            border.width: Theme.borderWidthThin
            border.color: Theme.color("pulseBadgeBorder")

            Column {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 2

                Label {
                    width: parent.width
                    text: String(name || label || "")
                    color: Theme.color("textPrimary")
                    elide: Text.ElideRight
                }

                Label {
                    width: parent.width
                    visible: String(impact || "").length > 0
                    text: impact
                    color: Theme.color("textSecondary")
                    font.pixelSize: Theme.fontSize("xs")
                    elide: Text.ElideRight
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: root.itemTriggered(index)
            }
        }
    }
}
