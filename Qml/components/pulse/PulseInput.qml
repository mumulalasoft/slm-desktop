import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

Rectangle {
    id: root
    property bool commandMode: false
    property string text: ""
    property string commandPlaceholder: "Type command (e.g. > open settings)"
    property string searchPlaceholder: "Search apps, files, settings..."

    signal textEdited(string text)

    height: 44
    radius: Theme.radiusWindow
    color: Theme.color("pulseQueryBg")
    border.width: Theme.borderWidthThin
    border.color: Theme.color("pulseQueryBorder")

    function focusAndSelectAll() {
        inputField.forceActiveFocus()
        inputField.selectAll()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 8

        Label {
            text: root.commandMode ? ">" : "Pulse"
            color: Theme.color("textSecondary")
            font.pixelSize: Theme.fontSize("smallPlus")
        }

        TextField {
            id: inputField
            Layout.fillWidth: true
            background: null
            text: root.text
            placeholderText: root.commandMode ? root.commandPlaceholder : root.searchPlaceholder
            selectByMouse: true
            onTextEdited: root.textEdited(text)
        }
    }
}
