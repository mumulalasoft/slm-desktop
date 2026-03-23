import QtQuick 2.15
import QtQuick.Window 2.15

Window {
    id: root

    property var rootWindow: null
    property bool overlayVisible: false
    signal dismissRequested()

    visible: !!rootWindow && !!rootWindow.visible && overlayVisible
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus
    transientParent: rootWindow
    title: "Tothespot Dismiss Layer"
    x: rootWindow ? rootWindow.x : 0
    y: rootWindow ? rootWindow.y : 0
    width: rootWindow ? rootWindow.width : 0
    height: rootWindow ? rootWindow.height : 0

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        hoverEnabled: false
        onPressed: function(mouse) {
            root.dismissRequested()
            mouse.accepted = true
        }
    }
}

