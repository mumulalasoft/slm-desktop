import QtQuick 2.15
import QtQuick.Window 2.15

Window {
    id: root

    required property var rootWindow
    property bool overlayVisible: false

    visible: overlayVisible
    color: "transparent"
    flags: Qt.FramelessWindowHint
           | Qt.WindowDoesNotAcceptFocus
           | Qt.WindowTransparentForInput
    transientParent: null
    title: "Desktop Global Progress"
    width: 460
    height: 96
    x: (rootWindow ? rootWindow.x : 0) + Math.round(((rootWindow ? rootWindow.width : 0) - width) * 0.5)
    y: (rootWindow ? rootWindow.y : 0) + 10
    GlobalProgressOverlay {
        anchors.fill: parent
    }
}
