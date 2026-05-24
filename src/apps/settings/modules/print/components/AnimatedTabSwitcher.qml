import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import SlmStyle

Item {
    id: root

    property var labels: []
    property var icons: []
    property int currentIndex: 0
    signal tabSelected(int index)

    implicitHeight: 34

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusControl
        color: Theme.color("controlBg")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("panelBorder")
    }

    readonly property int tabCount: Math.max(1, labels.length)
    readonly property real tabWidth: width / tabCount

    Rectangle {
        id: activePill
        x: root.currentIndex * root.tabWidth + 3
        y: 3
        width: root.tabWidth - 6
        height: root.height - 6
        radius: Theme.radiusControl - 2
        color: Theme.color("surface")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("panelBorder")

        Behavior on x {
            NumberAnimation { duration: 145; easing.type: Easing.OutCubic }
        }
        Behavior on opacity {
            NumberAnimation { duration: 140; easing.type: Easing.OutQuad }
        }
    }

    Row {
        anchors.fill: parent

        Repeater {
            model: root.tabCount

            delegate: Item {
                required property int index
                width: root.tabWidth
                height: root.height
                readonly property bool active: root.currentIndex === index

                Row {
                    anchors.centerIn: parent
                    spacing: 5

                    Text {
                        text: root.icons[index] || ""
                        font.pixelSize: Theme.fontSize("small")
                        opacity: parent.parent.active ? 1.0 : 0.75
                        Behavior on opacity {
                            NumberAnimation { duration: 130; easing.type: Easing.OutQuad }
                        }
                    }

                    Text {
                        text: root.labels[index] || ""
                        font.pixelSize: Theme.fontSize("small")
                        font.weight: parent.parent.active ? Theme.fontWeight("medium") : Theme.fontWeight("regular")
                        color: parent.parent.active ? Theme.color("textPrimary") : Theme.color("textSecondary")

                        Behavior on color {
                            ColorAnimation { duration: 130; easing.type: Easing.OutCubic }
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.tabSelected(index)
                }
            }
        }
    }
}
