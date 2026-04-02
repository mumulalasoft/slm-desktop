import QtQuick 2.15
import QtQuick.Window 2.15
import Slm_Desktop

Window {
    id: root

    property var parentWindow: null
    property var notificationManager: null

    readonly property var anchorScreen: (parentWindow && parentWindow.screen) ? parentWindow.screen : screen

    visible: !!parentWindow && parentWindow.visible
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint
    transientParent: parentWindow
    title: "Desktop Notification Manager"

    x: {
        if (anchorScreen && anchorScreen.geometry) {
            return Math.round(anchorScreen.geometry.x)
        }
        return parentWindow ? Math.round(parentWindow.x) : 0
    }
    y: {
        if (anchorScreen && anchorScreen.geometry) {
            return Math.round(anchorScreen.geometry.y)
        }
        return parentWindow ? Math.round(parentWindow.y) : 0
    }
    width: {
        if (anchorScreen && anchorScreen.geometry) {
            return Math.max(1, Math.round(anchorScreen.geometry.width))
        }
        return parentWindow ? Math.max(1, Math.round(parentWindow.width)) : 1
    }
    height: {
        if (anchorScreen && anchorScreen.geometry) {
            return Math.max(1, Math.round(anchorScreen.geometry.height))
        }
        return parentWindow ? Math.max(1, Math.round(parentWindow.height)) : 1
    }

    NotificationManager {
        anchors.fill: parent
        notificationManager: root.notificationManager
    }
}

