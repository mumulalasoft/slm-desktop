import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop
import SlmStyle as DSStyle

Row {
    id: root

    property string nameText: ""
    readonly property bool fieldActiveFocus: nameField.activeFocus

    signal nameEdited(string textValue)

    width: parent ? parent.width : 0
    height: Theme.metric("controlHeightLarge")
    spacing: Theme.metric("spacingSm")

    DSStyle.Label {
        width: 72
        height: parent.height
        text: "Name"
        color: Theme.color("textSecondary")
        verticalAlignment: Text.AlignVCenter
    }

    DSStyle.TextField {
        id: nameField
        width: parent.width - 82
        height: Theme.metric("controlHeightRegular")
        anchors.verticalCenter: parent.verticalCenter
        text: root.nameText
        placeholderText: "Filename"
        onTextEdited: root.nameEdited(text)
    }
}
