import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Desktop_Shell
import Style

Rectangle {
    id: root

    required property var hostRoot

    color: "transparent"
    border.width: Theme.borderWidthNone
    border.color: "transparent"

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 6

        Label {
            Layout.fillWidth: true
            text: {
                var items = Number(hostRoot.fileModel
                                   ? Number(hostRoot.fileModel.count || 0) : 0)
                var selected = Number(
                            hostRoot.selectedEntryIndexes
                            ? hostRoot.selectedEntryIndexes.length : 0)
                return String(items) + " items"
                        + (selected > 0 ? ("  •  " + String(selected)
                                           + " selected") : "")
            }
            color: Theme.color("textSecondary")
            font.pixelSize: Theme.fontSize("small")
            elide: Text.ElideMiddle
        }
    }
}
