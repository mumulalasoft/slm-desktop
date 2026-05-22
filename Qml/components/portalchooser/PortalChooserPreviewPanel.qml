import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop
import SlmStyle as DSStyle

Rectangle {
    id: root

    property string previewPath: ""

    width: previewPath.length > 0 ? 240 : 0
    height: parent ? parent.height : 0
    radius: Theme.radiusControl
    color: Theme.color("fileManagerContentBg")
    border.width: previewPath.length > 0 ? 1 : 0
    border.color: Theme.color("fileManagerContentBorder")
    visible: previewPath.length > 0
    clip: true

    Column {
        anchors.fill: parent
        anchors.margins: Theme.metric("spacingSm")
        spacing: Theme.metric("spacingSm")

        DSStyle.Label {
            text: "Preview"
            color: Theme.color("textSecondary")
            font.pixelSize: Theme.fontSize("small")
        }

        Rectangle {
            width: parent.width
            height: parent.height - 26
            radius: Theme.radiusControl
            color: Theme.color("controlBg")
            border.width: Theme.borderWidthThin
            border.color: Theme.color("panelBorder")
            clip: true

            Image {
                anchors.fill: parent
                anchors.margins: 4
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                cache: false
                source: root.previewPath.length > 0
                        ? "file://" + root.previewPath
                        : ""
            }
        }
    }
}
