import QtQuick 2.15
import QtQuick.Window 2.15
import Slm_Desktop

Window {
    id: root

    property var parentWindow: null
    property var notificationManager: null
    readonly property bool centerOpen: !!(notificationLayer && notificationLayer.centerOpen)
    readonly property bool bannerVisible: !!(notificationLayer && notificationLayer.bannerVisible)

    readonly property var anchorScreen: (parentWindow && parentWindow.screen) ? parentWindow.screen : screen

    visible: !!parentWindow && parentWindow.visible && (centerOpen || bannerVisible)
    color: "transparent"
    flags: Qt.Tool
           | Qt.FramelessWindowHint
           | Qt.NoDropShadowWindowHint
           | (centerOpen ? 0 : Qt.WindowDoesNotAcceptFocus)
    transientParent: parentWindow
    title: "Desktop Notification Manager"

    x: {
        if (anchorScreen && anchorScreen.geometry) {
            if (centerOpen) {
                return Math.round(anchorScreen.geometry.x)
            }
            return Math.round(anchorScreen.geometry.x + anchorScreen.geometry.width - width)
        }
        if (centerOpen) {
            return parentWindow ? Math.round(parentWindow.x) : 0
        }
        return parentWindow ? Math.round(parentWindow.x + parentWindow.width - width) : 0
    }
    y: {
        if (anchorScreen && anchorScreen.geometry) {
            return Math.round(anchorScreen.geometry.y)
        }
        return parentWindow ? Math.round(parentWindow.y) : 0
    }
    width: {
        if (!centerOpen && notificationLayer) {
            return Math.max(1, Math.round(notificationLayer.bannerSurfaceWidth))
        }
        if (anchorScreen && anchorScreen.geometry) {
            return Math.max(1, Math.round(anchorScreen.geometry.width))
        }
        return parentWindow ? Math.max(1, Math.round(parentWindow.width)) : 1
    }
    height: {
        if (!centerOpen && notificationLayer) {
            return Math.max(1, Math.round(notificationLayer.bannerSurfaceHeight))
        }
        if (anchorScreen && anchorScreen.geometry) {
            return Math.max(1, Math.round(anchorScreen.geometry.height))
        }
        return parentWindow ? Math.max(1, Math.round(parentWindow.height)) : 1
    }

    NotificationManager {
        id: notificationLayer
        anchors.fill: parent
        notificationManager: root.notificationManager
    }
}
