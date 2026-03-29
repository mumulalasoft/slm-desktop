import QtQuick 2.15
import Slm_Desktop

Item {
    id: root

    property var notificationManager: (typeof NotificationManager !== "undefined") ? NotificationManager : null
    readonly property int bannerRightMargin: 16
    readonly property int bannerTopMargin: 18

    BannerContainer {
        id: banners
        z: 20
        width: 380
        notificationManager: root.notificationManager
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: root.bannerTopMargin
        anchors.rightMargin: root.bannerRightMargin

        onDismissRequested: function(id) {
            if (root.notificationManager && root.notificationManager.dismiss) {
                root.notificationManager.dismiss(id)
            } else if (root.notificationManager && root.notificationManager.closeById) {
                root.notificationManager.closeById(id)
            }
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
        onCloseRequested: {
            if (root.notificationManager && root.notificationManager.toggleCenter) {
                root.notificationManager.toggleCenter()
            }
        }
    }
}
