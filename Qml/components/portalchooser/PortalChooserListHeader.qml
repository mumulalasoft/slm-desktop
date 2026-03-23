import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

Rectangle {
    id: root

    property real listWidth: 0
    property real nameColumnWidth: 220
    property real dateColumnWidth: 140
    property real kindColumnWidth: 92

    signal dateColumnWidthDragged(real value, real rowWidth)
    signal kindColumnWidthDragged(real value, real rowWidth)
    signal resizeReleased()

    width: root.listWidth
    height: 28
    color: Theme.color("menuBg")
    border.width: Theme.borderWidthThin
    border.color: Theme.color("fileManagerWindowBorder")

    readonly property real rowW: Math.max(220, width - 20)

    Row {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 8

        Label {
            width: root.nameColumnWidth
            height: parent.height
            text: "Name"
            color: Theme.color("textSecondary")
            font.weight: Theme.fontWeight("bold")
            verticalAlignment: Text.AlignVCenter
        }
        Label {
            width: Math.max(90, Number(root.dateColumnWidth || 140))
            height: parent.height
            text: "Date Modified"
            color: Theme.color("textSecondary")
            font.weight: Theme.fontWeight("bold")
            verticalAlignment: Text.AlignVCenter
        }
        Label {
            width: Math.max(80, Number(root.kindColumnWidth || 92))
            height: parent.height
            text: "Kind / Size"
            color: Theme.color("textSecondary")
            font.weight: Theme.fontWeight("bold")
            verticalAlignment: Text.AlignVCenter
        }
    }

    MouseArea {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 8
        x: 10 + root.nameColumnWidth + 4
        cursorShape: Qt.SplitHCursor
        property real startX: 0
        property real startDateW: 0
        onPressed: {
            startX = mouse.x
            startDateW = root.dateColumnWidth
        }
        onPositionChanged: {
            var delta = mouse.x - startX
            root.dateColumnWidthDragged(startDateW - delta, root.rowW)
        }
        onReleased: root.resizeReleased()
    }

    MouseArea {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 8
        x: width - 10 - Math.max(80, Number(root.kindColumnWidth || 92)) - 4
        cursorShape: Qt.SplitHCursor
        property real startX: 0
        property real startKindW: 0
        onPressed: {
            startX = mouse.x
            startKindW = root.kindColumnWidth
        }
        onPositionChanged: {
            var delta = mouse.x - startX
            root.kindColumnWidthDragged(startKindW - delta, root.rowW)
        }
        onReleased: root.resizeReleased()
    }
}
