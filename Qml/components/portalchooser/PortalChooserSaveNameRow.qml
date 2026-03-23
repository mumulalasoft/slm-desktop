import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

Row {
    id: root

    property string nameText: ""
    readonly property bool fieldActiveFocus: nameField.activeFocus

    signal nameEdited(string textValue)

    width: parent ? parent.width : 0
    height: 34
    spacing: 8

    Label {
        width: 72
        height: parent.height
        text: "Name"
        color: Theme.color("textSecondary")
        verticalAlignment: Text.AlignVCenter
    }

    TextField {
        id: nameField
        width: parent.width - 82
        height: 32
        text: root.nameText
        color: Theme.color("textPrimary")
        selectionColor: Theme.color("selectedItem")
        selectedTextColor: Theme.color("selectedItemText")
        placeholderText: "Filename"
        onTextEdited: root.nameEdited(text)
    }
}
