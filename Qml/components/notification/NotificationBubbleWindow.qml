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

    visible: !!parentWindow && parentWindow.visible && bubbleActive
    color: "transparent"
    flags: Qt.FramelessWindowHint
           | Qt.WindowStaysOnTopHint
           | Qt.WindowDoesNotAcceptFocus
           | Qt.WindowTransparentForInput
    title: "Desktop Notification Bubble"
    width: 360
    height: Math.max(1, Math.ceil(notificationBubbleItem.height))
    x: parentWindow ? parentWindow.x + parentWindow.width - width - bubbleRightMargin : 0
    y: parentWindow ? parentWindow.y + (panelHeight * 2) + bubbleTopMargin : 0

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
