import QtQuick 2.15
import Slm_Desktop

Rectangle {
    id: root

    property real startX: 0
    property real startY: 0
    property real endX: 0
    property real endY: 0

    x: Math.min(startX, endX)
    y: Math.min(startY, endY)
    width: Math.abs(endX - startX)
    height: Math.abs(endY - startY)
    color: Theme.color("selectionRectFill")
    border.width: Theme.borderWidthThin
    border.color: Theme.color("selectionRectBorder")
}
