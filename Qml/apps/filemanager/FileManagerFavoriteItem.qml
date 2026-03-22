import QtQuick 2.15
import QtQuick.Controls 2.15
import Desktop_Shell

Rectangle {
    id: root
    property string name: ""
    property string path: ""
    signal openRequested(string path)

    width: parent ? parent.width : 220
    height: 32
    radius: Theme.radiusControlLarge
    color: favMouse.containsMouse ? Theme.color("hoverItem") : "transparent"

    Row {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 12
        spacing: 8

        Label {
            text: "\u2605"
            color: Theme.color("accent")
        }

        Label {
            text: root.name
            color: Theme.color("textPrimary")
            width: 180
            elide: Text.ElideRight
        }
    }

    MouseArea {
        id: favMouse
        anchors.fill: parent
        hoverEnabled: true
        onClicked: root.openRequested(root.path)
    }
}
