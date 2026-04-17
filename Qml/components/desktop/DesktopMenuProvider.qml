import QtQuick 2.15

QtObject {
    id: root

    property string appId: "filemanager"
    property string role: "desktop_view"
    property string path: "~/Desktop"
    property var selection: []
    property var menuContext: ({
        "selectionCount": 0,
        "selectionTypes": []
    })
    property var desktopSurface: null
    property bool enabled: true
    property string overrideContext: "desktop-view"

    function selectionCount() {
        if (menuContext && menuContext.selectionCount !== undefined) {
            return Number(menuContext.selectionCount || 0)
        }
        return selection ? Number(selection.length || 0) : 0
    }

    function hasSingleSelection() {
        return selectionCount() === 1
    }

    function hasMultiSelection() {
        return selectionCount() > 1
    }

    function topLevelMenus() {
        return [
            { "id": 3101, "label": "File", "enabled": true, "source": "desktop-provider" },
            { "id": 3102, "label": "Edit", "enabled": true, "source": "desktop-provider" },
            { "id": 3103, "label": "Go", "enabled": true, "source": "desktop-provider" },
            { "id": 3104, "label": "Workspace", "enabled": true, "source": "desktop-provider" },
            { "id": 3105, "label": "Help", "enabled": true, "source": "desktop-provider" }
        ]
    }

    function ownsMenu(menuId) {
        var id = Number(menuId || 0)
        return id >= 3101 && id <= 3105
    }

    function menuItemsFor(menuId) {
        var id = Number(menuId || 0)
        var count = selectionCount()
        if (id === 3101) {
            if (count <= 0) {
                return [
                    { "id": 1, "label": "New Folder", "enabled": true },
                    { "id": 2, "label": "Open Terminal Here", "enabled": true }
                ]
            }
            if (count === 1) {
                return [
                    { "id": 11, "label": "Open", "enabled": true },
                    { "id": 12, "label": "Rename", "enabled": true },
                    { "id": 13, "label": "Compress", "enabled": true },
                    { "id": 14, "label": "Move to Trash", "enabled": true },
                    { "id": 15, "label": "Properties", "enabled": true }
                ]
            }
            return [
                { "id": 21, "label": "Compress", "enabled": true },
                { "id": 22, "label": "Move to Trash", "enabled": true }
            ]
        }
        if (id === 3102) {
            return [
                { "id": 31, "label": "Select All", "enabled": true },
                { "id": 32, "label": "Clear Selection", "enabled": true }
            ]
        }
        if (id === 3103) {
            return [
                { "id": 41, "label": "Home", "enabled": true },
                { "id": 42, "label": "Desktop", "enabled": true }
            ]
        }
        if (id === 3104) {
            return [
                { "id": 51, "label": "Workspace Overview", "enabled": true }
            ]
        }
        if (id === 3105) {
            return [
                { "id": 61, "label": "Desktop View Help", "enabled": true }
            ]
        }
        return []
    }

    function activateMenuItem(menuId, itemId) {
        if (!desktopSurface) {
            return
        }
        var menu = Number(menuId || 0)
        var item = Number(itemId || 0)
        if (menu === 3101) {
            if (item === 1 && desktopSurface.createNewFolder) {
                desktopSurface.createNewFolder()
                return
            }
            if (item === 2 && desktopSurface.openTerminalHere) {
                desktopSurface.openTerminalHere()
                return
            }
            if (item === 11 && desktopSurface.openSelection) {
                desktopSurface.openSelection()
                return
            }
            if (item === 12 && desktopSurface.renameSelection) {
                desktopSurface.renameSelection()
                return
            }
            if (item === 13 && desktopSurface.compressSelection) {
                desktopSurface.compressSelection()
                return
            }
            if (item === 14 && desktopSurface.moveSelectionToTrash) {
                desktopSurface.moveSelectionToTrash()
                return
            }
            if (item === 15 && desktopSurface.openPropertiesSelection) {
                desktopSurface.openPropertiesSelection()
                return
            }
            if (item === 21 && desktopSurface.compressSelection) {
                desktopSurface.compressSelection()
                return
            }
            if (item === 22 && desktopSurface.moveSelectionToTrash) {
                desktopSurface.moveSelectionToTrash()
                return
            }
        }
        if (menu === 3102) {
            if (item === 31 && desktopSurface.selectAllEntries) {
                desktopSurface.selectAllEntries()
                return
            }
            if (item === 32 && desktopSurface.clearSelection) {
                desktopSurface.clearSelection()
                return
            }
        }
        if (menu === 3103) {
            if (item === 41 && desktopSurface.openHomePath) {
                desktopSurface.openHomePath()
                return
            }
            if (item === 42 && desktopSurface.openDesktopPath) {
                desktopSurface.openDesktopPath()
                return
            }
        }
        if (menu === 3104 && item === 51) {
            if (typeof AppCommandRouter !== "undefined" && AppCommandRouter && AppCommandRouter.route) {
                AppCommandRouter.route("workspace.toggle", {}, "desktop-menu-provider")
            }
            return
        }
    }

    function syncGlobalMenuOverride(noActiveWindow) {
        if (typeof GlobalMenuManager === "undefined" || !GlobalMenuManager) {
            return
        }
        if (!enabled || !noActiveWindow) {
            if (GlobalMenuManager.diagnosticSnapshot && GlobalMenuManager.clearOverrideMenus) {
                var diag = GlobalMenuManager.diagnosticSnapshot()
                if (diag && diag.overrideEnabled && String(diag.overrideContext || "") === overrideContext) {
                    GlobalMenuManager.clearOverrideMenus()
                }
            }
            return
        }
        if (GlobalMenuManager.setOverrideMenus) {
            GlobalMenuManager.setOverrideMenus(topLevelMenus(), overrideContext)
        }
    }
}
