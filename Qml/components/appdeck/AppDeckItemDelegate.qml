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
    readonly property bool hoverActive: mouse.containsMouse
    // Apple HIG-style hover/selected affordance lives as a soft-fill rounded
    // rect over the whole tile. Radius is wider (14px) than the legacy
    // card-token to read as a Launchpad-style highlight rather than a card.
    readonly property real cornerRadius: 14

    signal activated(var appData)
    signal hovered()
    signal contextMenuRequested(var appData)

    implicitWidth: compact ? 96 : 118
    implicitHeight: compact ? 100 : 128

    Rectangle {
        id: hoverPlate
        anchors.fill: parent
        radius: root.cornerRadius
        // Translucent white fill — no border, just the soft rect. Selected
        // state is brighter than hover so keyboard focus reads clearly.
        // Alpha tuned so the plate sits at ~Launchpad-style visibility on
        // both light and dark backgrounds.
        color: root.selected
               ? Qt.rgba(1, 1, 1, Theme.darkMode ? 0.20 : 0.34)
               : Qt.rgba(1, 1, 1, Theme.darkMode ? 0.12 : 0.22)
        border.width: Theme.borderWidthNone
        opacity: root.selected || root.hoverActive ? 1.0 : 0.0

        Behavior on opacity {
            NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingDecelerate }
        }
        Behavior on color {
            ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easingLight }
        }
    }

    Item {
        id: iconWrap
        width: root.compact ? 54 : 62
        height: width
        anchors.top: parent.top
        anchors.topMargin: root.compact ? 9 : 13
        anchors.horizontalCenter: parent.horizontalCenter
        // Subtle scale up on hover — gives a tactile, Launchpad-style lift
        // without needing a custom shadow layer. The app icons already
        // ship with baked shadows, so we don't add another one here.
        scale: root.hoverActive ? 1.05 : 1.0

        Behavior on scale {
            NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingDecelerate }
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
