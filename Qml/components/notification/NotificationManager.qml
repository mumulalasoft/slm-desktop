import QtQuick 2.15
import Slm_Desktop

Item {
    id: root

    property var notificationManager: (typeof NotificationManager !== "undefined") ? NotificationManager : null
    readonly property int bannerRightMargin: 16
    readonly property int bannerTopMargin: 18

    function dismissBannerOnly(id) {
        if (!root.notificationManager) {
            return
        }
        if (root.notificationManager.dismissBanner) {
            root.notificationManager.dismissBanner(id)
        } else if (root.notificationManager.dismiss) {
            root.notificationManager.dismiss(id)
        } else if (root.notificationManager.closeById) {
            root.notificationManager.closeById(id)
        }
    }

    function openNotification(id, fromBanner) {
        if (!root.notificationManager || id <= 0) {
            return
        }
        if (root.notificationManager.markRead) {
            root.notificationManager.markRead(id, true)
        }
        if (root.notificationManager.invokeAction) {
            root.notificationManager.invokeAction(id, "default")
        }
        if (fromBanner === true) {
            root.dismissBannerOnly(id)
        } else if (root.notificationManager.centerVisible && root.notificationManager.toggleCenter) {
            root.notificationManager.toggleCenter()
        }
    }

    BannerContainer {
        id: banners
        z: 20
        width: 380
        notificationManager: root.notificationManager
        autoDismissMs: {
            var v = root.notificationManager ? Number(root.notificationManager.bubbleDurationMs || 0) : 0
            return v > 0 ? v : 6200
        }
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: root.bannerTopMargin
        anchors.rightMargin: root.bannerRightMargin

        onDismissRequested: function(id) {
            root.dismissBannerOnly(id)
        }
        onNotificationClicked: function(id) {
            root.openNotification(id, true)
        }
    }

    NotificationCenter {
        id: center
        z: 30
        anchors.fill: parent
        notificationManager: root.notificationManager
        open: root.notificationManager && !!root.notificationManager.centerVisible

        onDismissRequested: function(id) {
            if (root.notificationManager && root.notificationManager.dismiss) {
                root.notificationManager.dismiss(id)
            } else if (root.notificationManager && root.notificationManager.closeById) {
                root.notificationManager.closeById(id)
            }
        }
        onNotificationClicked: function(id) {
            root.openNotification(id, false)
        }
        onCloseRequested: {
            if (root.notificationManager && root.notificationManager.toggleCenter) {
                root.notificationManager.toggleCenter()
            }
        }
    }
}
