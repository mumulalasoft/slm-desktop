import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle as DSStyle

Column {
    id: root
    property string name: ""
    property string path: ""
    property string badgeText: ""
    property bool showHeader: false
    property string headerText: ""
    signal openRequested(string path)

    spacing: 2

    DSStyle.Label {
        text: root.headerText
        color: Theme.color("textSecondary")
        font.pixelSize: Theme.fontSize("xs")
        topPadding: 4
        visible: root.showHeader
    }

    Rectangle {
        width: parent ? parent.width : 220
        height: 32
        radius: Theme.radiusControlLarge
        color: recentMouse.containsMouse ? Theme.color("hoverItem") : "transparent"

        Row {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 12
            spacing: 8

            DSStyle.Label {
                text: "\u2022"
                color: Theme.color("textSecondary")
            }

            DSStyle.Label {
                text: root.name
                color: Theme.color("textPrimary")
                width: 140
                elide: Text.ElideRight
            }

            DSStyle.Label {
                text: root.badgeText
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("xs")
                width: 48
                horizontalAlignment: Text.AlignRight
            }
        }

        MouseArea {
            id: recentMouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: root.openRequested(root.path)
        }
    }
}
