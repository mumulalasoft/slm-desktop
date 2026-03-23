import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

Item {
    id: root

    property string entryName: ""
    property string entryIconName: ""
    property string entryIconSource: ""
    property real pointerX: 0
    property real pointerY: 0
    property int tileWidth: 96
    property int tileHeight: 108

    width: tileWidth
    height: tileHeight
    x: pointerX - width * 0.5
    y: pointerY - height * 0.5

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusCard
        color: Theme.color("dragGhostFill")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("dragGhostBorder")
        opacity: Theme.opacityGhost
    }

    Column {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        Rectangle {
            width: 58
            height: 58
            radius: Theme.radiusWindow
            anchors.horizontalCenter: parent.horizontalCenter
            color: Theme.color("shellIconPlateBg")
            border.width: Theme.borderWidthThin
            border.color: Theme.color("windowCardBorder")

            Image {
                anchors.centerIn: parent
                width: 38
                height: 38
                source: (root.entryIconName && root.entryIconName.length > 0)
                        ? ("image://themeicon/" + root.entryIconName + "?v=" +
                           ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                            ? ThemeIconController.revision : 0))
                        : ((root.entryIconSource && root.entryIconSource.length > 0)
                           ? root.entryIconSource : "qrc:/icons/logo.svg")
                fillMode: Image.PreserveAspectFit
            }
        }

        Label {
            width: parent.width
            text: root.entryName
            color: Theme.color("selectedItemText")
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            lineHeightMode: Text.ProportionalHeight
            lineHeight: Theme.lineHeight("tight")
            maximumLineCount: 2
            elide: Text.ElideRight
            font.pixelSize: Theme.fontSize("small")
        }
    }
}
