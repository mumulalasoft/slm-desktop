import QtQuick 2.15
import "../notification" as NotificationComp
import "../shell" as ShellComp
import "../shell/ShellUtils.js" as ShellUtils

Item {
    id: root

    required property var rootWindow
    required property var desktopScene
    required property var shellApi

    visible: false

    NotificationComp.NotificationBubbleWindow {
        id: notificationBubbleWindow
        parentWindow: root.rootWindow
        panelHeight: root.desktopScene ? root.desktopScene.panelHeight : 0
        notificationManager: (typeof NotificationManager !== "undefined") ? NotificationManager : null
    }

    ShellComp.ShellContextWindow {
        id: shellContextWindow
        parentWindow: root.rootWindow
        contextOpen: root.shellApi ? root.shellApi.shellContextMenuOpen : false
        contextMenuX: root.shellApi ? root.shellApi.shellContextMenuX : 0
        contextMenuY: root.shellApi ? root.shellApi.shellContextMenuY : 0
        contextMenuWidth: root.shellApi ? root.shellApi.shellContextMenuWidth : 190
        contextMenuHeight: root.shellApi ? root.shellApi.shellContextMenuHeight : 40
        openedAtMs: root.shellApi ? root.shellApi.shellContextMenuOpenedAtMs : 0
        onRequestClose: ShellUtils.closeShellContextMenu(root.shellApi)
        onRequestArrangeShortcut: {
            root.desktopScene.arrangeShellShortcuts()
            ShellUtils.closeShellContextMenu(root.shellApi)
        }
        onRequestReopenAt: function(x, y) {
            ShellUtils.openShellContextMenu(root.shellApi, x, y)
        }
    }
}
