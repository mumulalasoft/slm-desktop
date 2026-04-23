import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

Item {
    id: root

    property var appData: ({})
    property string title: ""
    property string subtitle: ""
    property string iconSource: ""
    property bool running: false
    property bool selected: false
    property bool compact: false
    readonly property real cornerRadius: (typeof Theme !== "undefined"
                                          && Theme
                                          && Theme.radiusCard !== undefined)
                                         ? Number(Theme.radiusCard) : 12

    signal activated(var appData)
    signal hovered()
    signal contextMenuRequested(var appData)

    implicitWidth: compact ? 96 : 118
    implicitHeight: compact ? 100 : 128

    Rectangle {
        id: hoverPlate
        anchors.fill: parent
        radius: root.cornerRadius
        color: root.selected
               ? Theme.color("accentSoft")
               : (mouse.containsMouse ? Theme.color("hoverItem") : "transparent")
        border.width: root.selected ? Theme.borderWidthThin : Theme.borderWidthNone
        border.color: root.selected ? Theme.color("accent") : "transparent"
    }

    Item {
        id: iconWrap
        width: root.compact ? 54 : 62
        height: width
        anchors.top: parent.top
        anchors.topMargin: root.compact ? 8 : 12
        anchors.horizontalCenter: parent.horizontalCenter

        Rectangle {
            anchors.fill: parent
            radius: root.cornerRadius
            color: Theme.color("windowCard")
            // border.width: Theme.borderWidthThin
            // border.color: Theme.color("windowCardBorder")
        }

        Image {
            id: appIcon
            anchors.centerIn: parent
            width: Math.round(parent.width * 0.78)
            height: width
            source: root.iconSource
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: false
        }

        Label {
            anchors.centerIn: parent
            visible: appIcon.status === Image.Error || String(root.iconSource || "").length === 0
            text: root.title.length > 0 ? root.title.charAt(0).toUpperCase() : "?"
            font.pixelSize: Theme.fontSize("title")
            font.weight: Theme.fontWeight("semibold")
            color: Theme.color("textSecondary")
        }
    }

    Label {
        anchors.top: iconWrap.bottom
        anchors.topMargin: root.compact ? 7 : 9
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        maximumLineCount: 1
        text: root.title
        font.pixelSize: Theme.fontSize("menu")
        color: Theme.color("textPrimary")
    }

    Rectangle {
        width: 7
        height: 7
        radius: Theme.radiusMax
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.compact ? 8 : 10
        visible: root.running
        color: Theme.color("accent")
        opacity: Theme.opacityElevated
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        onEntered: root.hovered()
        onClicked: function(mouseEvent) {
            if (mouseEvent.button === Qt.LeftButton) {
                root.activated(root.appData)
            } else if (mouseEvent.button === Qt.RightButton) {
                root.contextMenuRequested(root.appData)
            }
        }
    }
}
