import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle
import SlmStyle as DSStyle

Rectangle {
    id: root

    required property var hostRoot

    color: "transparent"
    border.width: Theme.borderWidthNone
    border.color: "transparent"

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.07) : Qt.rgba(0, 0, 0, 0.07)
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.metric("spacingMd")
        anchors.rightMargin: Theme.metric("spacingMd")
        spacing: Theme.metric("spacingSm")

        DSStyle.Label {
            Layout.fillWidth: true
            text: {
                var items = Number(hostRoot.fileModel
                                   ? Number(hostRoot.fileModel.count || 0) : 0)
                var selected = Number(
                            hostRoot.selectedEntryIndexes
                            ? hostRoot.selectedEntryIndexes.length : 0)
                if (selected > 0) {
                    return String(selected) + " of " + String(items) + " selected"
                }
                return String(items) + (items === 1 ? " item" : " items")
            }
            color: Theme.color("textSecondary")
            font.pixelSize: Theme.fontSize("small")
            horizontalAlignment: Text.AlignLeft
            elide: Text.ElideMiddle
        }
    }
}
