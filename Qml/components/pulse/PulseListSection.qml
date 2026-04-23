import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import "." as PulseComp

PulseComp.PulseSection {
    id: root

    property var model: null
    property int focusedIndex: -1
    property bool showPath: false

    signal itemTriggered(int index)

    ListView {
        anchors.fill: parent
        clip: true
        spacing: 4
        model: root.model

        delegate: Rectangle {
            required property int index
            required property string name
            property string path: ""

            readonly property bool focused: root.focusedIndex === index
            width: ListView.view.width
            height: 40
            radius: Theme.radiusControl
            color: focused ? Theme.color("pulseRowActive") : "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    text: name
                    color: Theme.color("textPrimary")
                    elide: Text.ElideRight
                }
                Label {
                    visible: root.showPath
                    text: path
                    color: Theme.color("textSecondary")
                    font.pixelSize: Theme.fontSize("xs")
                    elide: Text.ElideMiddle
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: root.itemTriggered(index)
            }
        }
    }
}
