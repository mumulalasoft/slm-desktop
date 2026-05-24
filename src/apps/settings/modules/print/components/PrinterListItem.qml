import QtQuick 2.15
import QtQuick.Controls 2.15
import SlmStyle

ItemDelegate {
    id: root

    property string title: ""
    property string subtitle: ""
    property bool selected: false
    property int revealDelayMs: 0

    signal selectedByUser()

    height: 60
    padding: 0
    highlighted: selected
    opacity: 0
    y: 8

    onClicked: selectedByUser()

    Component.onCompleted: enterAnim.restart()

    SequentialAnimation {
        id: enterAnim
        running: false
        PauseAnimation { duration: Math.max(0, root.revealDelayMs) }
        ParallelAnimation {
            NumberAnimation { target: root; property: "opacity"; to: 1; duration: 170; easing.type: Easing.OutCubic }
            NumberAnimation { target: root; property: "y"; to: 0; duration: 180; easing.type: Easing.OutCubic }
        }
    }

    background: Rectangle {
        id: bg
        property color targetColor: root.selected ? Theme.color("accentSoft")
                                                 : root.hovered ? Theme.color("controlBgHover")
                                                                : "transparent"
        color: targetColor
        border.width: root.selected ? Theme.borderWidthThin : 0
        border.color: Theme.color("accent")

        Behavior on color {
            ColorAnimation { duration: root.selected ? 165 : 110; easing.type: Easing.OutQuad }
        }
        Behavior on border.width {
            NumberAnimation { duration: 130; easing.type: Easing.OutQuad }
        }

        Rectangle {
            visible: index > 0
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 14
            height: Theme.borderWidthThin
            color: Theme.color("panelBorder")
            opacity: root.selected ? 0 : 1
            Behavior on opacity {
                NumberAnimation { duration: 100; easing.type: Easing.OutQuad }
            }
        }
    }

    contentItem: Item {
        anchors {
            left: parent.left
            right: parent.right
            leftMargin: 14
            rightMargin: 14
            verticalCenter: parent.verticalCenter
        }

        Text {
            id: titleLabel
            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
                topMargin: 8
            }
            text: root.title
            font.pixelSize: Theme.fontSize("small")
            font.weight: Theme.fontWeight("medium")
            color: root.selected ? Theme.color("accentText") : Theme.color("textPrimary")
            elide: Text.ElideRight

            Behavior on color {
                ColorAnimation { duration: 125; easing.type: Easing.OutCubic }
            }
        }

        Row {
            anchors { left: parent.left; top: titleLabel.bottom; topMargin: 3 }
            spacing: 6

            Rectangle {
                width: 5
                height: 5
                radius: Theme.radiusSm
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.color("success")
            }

            Text {
                text: root.subtitle
                font.pixelSize: Theme.fontSize("xs")
                color: Theme.color("textSecondary")
            }
        }
    }
}
