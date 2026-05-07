import QtQuick 2.15
import QtQuick.Window 2.15
import Slm_Desktop

Window {
    id: root

    property var parentWindow: null
    property var notificationManager: null
    readonly property var shellPolicy: (typeof ShellPolicyController !== "undefined")
                                       ? ShellPolicyController
                                       : null
    readonly property bool policyAllowsNotifications: !root.shellPolicy
                                                       || root.shellPolicy.notificationsAllowed
    readonly property bool policyAllowsNotificationCenter: !root.shellPolicy
                                                            || root.shellPolicy.visibilityPolicy === root.shellPolicy.Normal
    readonly property bool policyCompactNotifications: !!root.shellPolicy
                                                       && root.shellPolicy.compactNotifications
    readonly property bool rawCenterOpen: !!(notificationLayer && notificationLayer.centerOpen)
    readonly property bool rawBannerVisible: !!(notificationLayer && notificationLayer.bannerVisible)
    readonly property bool centerOpen: root.policyAllowsNotificationCenter && root.rawCenterOpen
    readonly property bool bannerVisible: root.policyAllowsNotifications && root.rawBannerVisible
    readonly property bool layerShellSupported: (typeof NotificationLayerShell !== "undefined")
                                                && !!NotificationLayerShell
                                                && !!NotificationLayerShell.supported
    property bool _layerPrepared: !layerShellSupported

    readonly property var anchorScreen: (parentWindow && parentWindow.screen) ? parentWindow.screen : screen

    visible: !!parentWindow
             && parentWindow.visible
             && root.layerShellSupported
             && root._layerPrepared
             && (centerOpen || bannerVisible)
    color: "transparent"
    flags: Qt.FramelessWindowHint
           | Qt.NoDropShadowWindowHint
           | (centerOpen ? 0 : Qt.WindowDoesNotAcceptFocus)
    transientParent: null
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
        compactMode: root.policyCompactNotifications
    }

    function syncLayerSurfaceSize() {
        if (!root.layerShellSupported
                || typeof NotificationLayerShell === "undefined"
                || !NotificationLayerShell) {
            return
        }
        NotificationLayerShell.setNotificationSurface(root,
                                                      Math.max(1, Math.round(root.width)),
                                                      Math.max(1, Math.round(root.height)),
                                                      0,
                                                      0,
                                                      Math.max(1, Math.round(root.width)),
                                                      Math.max(1, Math.round(root.height)))
    }

    onVisibleChanged: {
        if (visible) {
            console.info("[NOTIFY] [SURFACE_READY] visible layer=OverlayLayer anchors=top|right size="
                         + width + "x" + height)
        }
        root.syncLayerSurfaceSize()
    }
    onWidthChanged: root.syncLayerSurfaceSize()
    onHeightChanged: root.syncLayerSurfaceSize()
    onCenterOpenChanged: root.syncLayerSurfaceSize()
    onBannerVisibleChanged: root.syncLayerSurfaceSize()
    onPolicyAllowsNotificationsChanged: {
        console.info("[NOTIFY] [SHELL_POLICY] notificationsAllowed=" + root.policyAllowsNotifications
                     + " centerAllowed=" + root.policyAllowsNotificationCenter
                     + " compact=" + root.policyCompactNotifications
                     + " policy=" + (root.shellPolicy ? root.shellPolicy.visibilityPolicyName : "Normal"))
        root.syncLayerSurfaceSize()
    }
    onSceneGraphInitialized: root.syncLayerSurfaceSize()

    Timer {
        id: layerShellRetryTimer
        interval: 300
        repeat: true
        running: root.visible && root.layerShellSupported
        onTriggered: root.syncLayerSurfaceSize()
    }

    Component.onCompleted: {
        if (root.layerShellSupported && typeof NotificationLayerShell !== "undefined" && NotificationLayerShell) {
            NotificationLayerShell.prepareNotificationWindow(root)
        }
        root._layerPrepared = true
        Qt.callLater(function() { root.syncLayerSurfaceSize() })
    }
}
