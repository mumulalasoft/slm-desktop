import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

RowLayout {
    id: root
    property var model: null
    property int focusedIndex: -1

    signal actionTriggered(int index)

    spacing: 8

    Repeater {
        model: root.model
        delegate: Rectangle {
            required property int index
            required property string label
            readonly property bool focused: root.focusedIndex === index

            height: 30
            width: chipLabel.implicitWidth + 22
            radius: Theme.radiusControlLarge
            color: focused ? Theme.color("pulseBadgeActive") : Theme.color("pulseBadgeBg")
            border.width: Theme.borderWidthThin
            border.color: Theme.color("pulseBadgeBorder")

            Label {
                id: chipLabel
                anchors.centerIn: parent
                text: label
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("small")
            }

            MouseArea {
                anchors.fill: parent
                onClicked: root.actionTriggered(index)
            }
        }
    }
}
