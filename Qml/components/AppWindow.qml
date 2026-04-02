import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

Rectangle {
    id: root

    property string title: "Window"
    property string subtitle: ""
    property string bodyText: ""

    radius: Theme.radiusCard
    clip: true
    antialiasing: true
    color: Theme.color("windowCard")
    border.color: Theme.color("windowCardBorder")
    border.width: Theme.borderWidthThin
    Behavior on color {
        ColorAnimation {
            duration: Theme.transitionDuration
            easing.type: Theme.easingStandard
        }
    }

    Rectangle {
        id: titleBar
        width: parent.width
        height: 32
        radius: root.radius
        color: Theme.color("surface")
        Behavior on color {
            ColorAnimation {
                duration: Theme.transitionDuration
                easing.type: Theme.easingStandard
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            width: 10
            height: 10
            radius: Theme.radiusSmPlus
            color: Theme.color("windowControlClose")
        }

        Rectangle {
            anchors.left: parent.left
            anchors.leftMargin: 28
            anchors.verticalCenter: parent.verticalCenter
            width: 10
            height: 10
            radius: Theme.radiusSmPlus
            color: Theme.color("windowControlMinimize")
        }

        Rectangle {
            anchors.left: parent.left
            anchors.leftMargin: 44
            anchors.verticalCenter: parent.verticalCenter
            width: 10
            height: 10
            radius: Theme.radiusSmPlus
            color: Theme.color("windowControlMaximize")
        }

        Column {
            anchors.centerIn: parent
            spacing: 1

            Label {
                text: root.title
                color: Theme.color("textPrimary")
                font.pixelSize: Theme.fontSize("small")
                font.weight: Theme.fontWeight("bold")
                horizontalAlignment: Text.AlignHCenter
            }

            Label {
                text: root.subtitle
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("tiny")
                horizontalAlignment: Text.AlignHCenter
            }
        }

        MouseArea {
            anchors.fill: parent
            drag.target: root
            drag.minimumX: 0
            drag.minimumY: 34
            drag.maximumX: root.parent ? root.parent.width - root.width : root.x
            drag.maximumY: root.parent ? root.parent.height - root.height : root.y
        }
    }

    Item {
        anchors.top: titleBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 16

        Label {
            anchors.fill: parent
            text: root.bodyText
            color: Theme.color("textPrimary")
            wrapMode: Text.WordWrap
            lineHeightMode: Text.ProportionalHeight
            lineHeight: Theme.lineHeight("normal")
            verticalAlignment: Text.AlignTop
            font.pixelSize: Theme.fontSize("body")
        }
    }
}
