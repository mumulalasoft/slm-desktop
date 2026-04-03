import QtQuick 2.15
import QtQuick.Window 2.15
import Slm_Desktop

Window {
    id: root

    property var parentWindow: null
    property int panelHeight: 0
    property var notificationManager: null

    readonly property int bubbleRightMargin: 16
    readonly property int bubbleTopMargin: 18
    readonly property bool bubbleActive: (notificationBubbleItem.showing || notificationBubbleItem.opacity > 0.01)
    readonly property var anchorScreen: (parentWindow && parentWindow.screen) ? parentWindow.screen : screen

    visible: !!parentWindow && parentWindow.visible && bubbleActive
    color: "transparent"
    flags: Qt.ToolTip
           | Qt.FramelessWindowHint
           | Qt.WindowDoesNotAcceptFocus
           | Qt.WindowTransparentForInput
    transientParent: parentWindow
    title: "Desktop Notification Bubble"
    width: 360
    height: Math.max(1, Math.ceil(notificationBubbleItem.height))
    x: {
        if (anchorScreen && anchorScreen.availableGeometry) {
            var g = anchorScreen.availableGeometry
            return Math.round(g.x + g.width - width - bubbleRightMargin)
        }
        return parentWindow ? Math.round(parentWindow.x + parentWindow.width - width - bubbleRightMargin) : bubbleRightMargin
    }
    y: {
        if (anchorScreen && anchorScreen.availableGeometry) {
            var g = anchorScreen.availableGeometry
            return Math.round(g.y + bubbleTopMargin)
        }
        return parentWindow ? Math.round(parentWindow.y + bubbleTopMargin) : bubbleTopMargin
    }

    Item {
        anchors.fill: parent
        clip: true

        NotificationBubble {
            id: notificationBubbleItem
            anchors.top: parent.top
            anchors.right: parent.right
            notificationManager: root.notificationManager
        }
    }
}
