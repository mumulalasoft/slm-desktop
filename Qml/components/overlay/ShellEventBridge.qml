import QtQuick 2.15
import "../../apps/filemanager/FileManagerGlobalMenuController.js" as FileManagerGlobalMenuController
import "../shell/ShellUtils.js" as ShellUtils

Item {
    id: root

    required property var shellApi
    required property var desktopScene
    required property var globalMenuActionRouter
    property var globalMenuManager: null
    property var desktopMenuProvider: null

    visible: false

    Connections {
        target: root.globalMenuManager
        ignoreUnknownSignals: true
        function onOverrideMenuActivated(menuId, label, context) {
            if (String(context || "") === "slm-filemanager") {
                FileManagerGlobalMenuController.handleMenu(root.shellApi,
                                                           menuId,
                                                           root.shellApi ? root.shellApi.fileManagerContent : null)
                return
            }
            if (String(context || "") === "desktop-view") {
                // Top-level only; dropdown items are resolved from DesktopMenuProvider.
                return
            }
        }
        function onOverrideMenuItemActivated(menuId, itemId, label, context) {
            if (String(context || "") === "desktop-view") {
                if (root.desktopMenuProvider && root.desktopMenuProvider.activateMenuItem) {
                    root.desktopMenuProvider.activateMenuItem(menuId, itemId)
                }
                return
            }
            root.globalMenuActionRouter.handleMenuItem(menuId, itemId, label, context)
        }
    }

    Connections {
        target: root.desktopScene
        ignoreUnknownSignals: true
        function onShellContextMenuRequested(x, y) {
            ShellUtils.openShellContextMenu(root.shellApi, x, y)
        }
    }
}
