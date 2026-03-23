import QtQuick 2.15
import Slm_Desktop

Rectangle {
    id: root

    property bool enabled: false
    property var rows: []
    property int panelHeight: 0
    property real frameDt: 0
    property real timeScale: 1
    property int droppedFrameCount: 0

    visible: enabled
    z: 950
    width: 320
    height: Math.min(parent ? parent.height * 0.45 : 400, 120 + ((rows || []).length * 20))
    anchors.top: parent ? parent.top : undefined
    anchors.right: parent ? parent.right : undefined
    anchors.topMargin: panelHeight + 10
    anchors.rightMargin: 10
    radius: Theme.radiusWindow
    color: Qt.rgba(0.08, 0.12, 0.18, 0.86)
    border.width: Theme.borderWidthThin
    border.color: Qt.rgba(0.64, 0.76, 0.90, 0.72)
    clip: true

    Column {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        Text {
            text: "Motion Debug"
            color: Theme.color("textOnGlass")
            font.family: Theme.fontFamilyUi
            font.pixelSize: Theme.fontSize("body")
            font.weight: Theme.fontWeight("bold")
        }

        Text {
            text: "dt: " + Number(root.frameDt || 0).toFixed(4)
            color: Theme.color("textSecondary")
            font.family: Theme.fontFamilyUi
            font.pixelSize: Theme.fontSize("small")
        }

        Text {
            text: "scale: " + Number(root.timeScale || 1).toFixed(2)
                  + "  dropped: " + Number(root.droppedFrameCount || 0)
            color: Theme.color("textSecondary")
            font.family: Theme.fontFamilyUi
            font.pixelSize: Theme.fontSize("small")
        }

        Rectangle {
            width: parent.width
            height: 1
            color: Qt.rgba(0.75, 0.84, 0.94, 0.30)
        }

        Flickable {
            width: parent.width
            height: parent.height - 64
            clip: true
            contentWidth: width
            contentHeight: debugColumn.implicitHeight

            Column {
                id: debugColumn
                width: parent.width
                spacing: 4

                Repeater {
                    model: root.rows
                    delegate: Text {
                        readonly property var row: modelData || ({})
                        text: String(row.channel || "")
                              + "  v=" + Number(row.value || 0).toFixed(3)
                              + "  vel=" + Number(row.velocity || 0).toFixed(1)
                              + "  tgt=" + Number(row.target || 0).toFixed(3)
                              + (row.active ? "  *" : "")
                        color: row.active ? "#F5FAFF" : "#B7C9DA"
                        font.family: Theme.fontFamilyUi
                        font.pixelSize: Theme.fontSize("small")
                        elide: Text.ElideRight
                        width: parent.width
                    }
                }
            }
        }
    }
}

