import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import SlmStyle as DSStyle

Rectangle {
    id: root

    property string text: ""
    signal cancelRequested()
    signal editRequested()
    signal executeNowRequested()

    implicitWidth: Math.min(520, label.implicitWidth + 210)
    implicitHeight: 40
    radius: Theme.radiusControl
    color: Theme.color("surface")
    border.color: Theme.color("panelBorder")
    border.width: Theme.borderWidthThin
    visible: text.length > 0

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 8
        spacing: 8

        DSStyle.Label {
            id: label
            Layout.fillWidth: true
            text: root.text
            elide: Text.ElideRight
            color: Theme.color("textPrimary")
            Accessible.name: text
        }

        DSStyle.Button {
            text: qsTr("Cancel")
            Accessible.name: qsTr("Cancel scheduled power action")
            onClicked: root.cancelRequested()
        }
        DSStyle.Button {
            text: qsTr("Edit")
            Accessible.name: qsTr("Edit scheduled power action")
            onClicked: root.editRequested()
        }
        DSStyle.Button {
            text: qsTr("Now")
            Accessible.name: qsTr("Execute scheduled power action now")
            onClicked: root.executeNowRequested()
        }
    }
}
