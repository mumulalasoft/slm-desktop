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

    Loader {
        id: notificationWindowLoader
        active: !!root.rootWindow
                && !!root.rootWindow.visible
                && !!root.shellApi
                && !!root.shellApi.startupNonCriticalWindowsReady
        asynchronous: false
        sourceComponent: Component {
            NotificationComp.NotificationManagerWindow {
                parentWindow: root.rootWindow
                notificationManager: (typeof NotificationManager !== "undefined") ? NotificationManager : null
            }
        }
    }

    Loader {
        id: shellContextWindowLoader
        active: !!root.shellApi && !!root.shellApi.shellContextMenuOpen
        asynchronous: false
        sourceComponent: Component {
            ShellComp.ShellContextWindow {
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
    }
}
