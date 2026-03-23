import QtQuick 2.15
import QtQuick.Window 2.15

Window {
    id: root

    required property var rootWindow
    property bool overlayVisible: false

    visible: overlayVisible
    color: "transparent"
    flags: Qt.FramelessWindowHint
           | Qt.WindowStaysOnTopHint
           | Qt.WindowDoesNotAcceptFocus
           | Qt.WindowTransparentForInput
    transientParent: rootWindow
    title: "Desktop Global Progress"
    width: 460
    height: 96
    x: (rootWindow ? rootWindow.x : 0) + Math.round(((rootWindow ? rootWindow.width : 0) - width) * 0.5)
    y: (rootWindow ? rootWindow.y : 0) + 10
    onVisibleChanged: {
        if (visible) {
            raise()
        }
    }
    onXChanged: {
        if (visible) {
            raise()
        }
    }
    onYChanged: {
        if (visible) {
            raise()
        }
    }

    Timer {
        interval: 16
        running: root.visible
        repeat: true
        onTriggered: {
            root.raise()
        }
    }

    GlobalProgressOverlay {
        anchors.fill: parent
    }
}
