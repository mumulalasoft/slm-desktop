import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Effects
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
    readonly property bool hoverActive: mouse.containsMouse
    readonly property real cornerRadius: (typeof Theme !== "undefined"
                                          && Theme
                                          && Theme.radiusCard !== undefined)
                                         ? Math.min(8, Number(Theme.radiusCard)) : 8

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
               : (root.hoverActive ? Theme.color("hoverItem") : "transparent")
        border.width: root.selected ? Theme.borderWidthThin : Theme.borderWidthNone
        border.color: root.selected ? Theme.color("accent") : "transparent"
        opacity: root.selected || root.hoverActive ? 1.0 : 0.0

        Behavior on opacity {
            NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingDecelerate }
        }
    }

    Item {
        id: iconWrap
        width: root.compact ? 54 : 62
        height: width
        anchors.top: parent.top
        anchors.topMargin: root.compact ? 9 : 13
        anchors.horizontalCenter: parent.horizontalCenter
        scale: root.hoverActive ? 1.045 : 1.0
        layer.enabled: root.hoverActive || root.selected
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Qt.rgba(0, 0, 0, Theme.darkMode ? 0.28 : 0.16)
            shadowBlur: 0.34
            shadowVerticalOffset: 3
            shadowHorizontalOffset: 0
        }

        Behavior on scale {
            NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingDecelerate }
        }

        Rectangle {
            anchors.fill: parent
            radius: root.cornerRadius
            color: root.hoverActive || root.selected ? Theme.color("surface") : "transparent"
            border.width: root.selected ? Theme.borderWidthThin : Theme.borderWidthNone
            border.color: root.selected ? Theme.color("accent") : "transparent"

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: 5
                anchors.rightMargin: 5
                anchors.topMargin: 1
                height: 1
                radius: Theme.radiusHairline
                color: Qt.rgba(1, 1, 1, Theme.darkMode ? 0.18 : 0.55)
                opacity: root.hoverActive || root.selected ? 1.0 : 0.0
            }
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
        font.weight: root.selected ? Theme.fontWeight("semibold") : Theme.fontWeight("regular")
        color: root.selected ? Theme.color("textPrimary") : Theme.color("textSecondary")
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
