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

    function _route(action, payload) {
        if (typeof AppCommandRouter === "undefined" || !AppCommandRouter || !AppCommandRouter.route) {
            return
        }
        AppCommandRouter.route(String(action || ""),
                               payload ? payload : ({}),
                               "desktop-menu-provider")
    }

    function _notifyInfo(message) {
        var body = String(message || "").trim()
        if (body.length <= 0) {
            return
        }
        if (typeof NotificationManager !== "undefined"
                && NotificationManager
                && NotificationManager.Notify) {
            NotificationManager.Notify("Desktop", 0,
                                       "dialog-information-symbolic",
                                       "Desktop View", body, [], {}, 2600)
        } else {
            console.log("[desktop-menu-provider]", body)
        }
    }

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

    function hasSelection() {
        return selectionCount() > 0
    }

    function topLevelMenus() {
        return [
            { "id": 3101, "label": "File", "enabled": true, "source": "desktop-provider" },
            { "id": 3102, "label": "Edit", "enabled": true, "source": "desktop-provider" },
            { "id": 3103, "label": "View", "enabled": true, "source": "desktop-provider" },
            { "id": 3104, "label": "Go", "enabled": true, "source": "desktop-provider" },
            { "id": 3105, "label": "Tools", "enabled": true, "source": "desktop-provider" },
            { "id": 3106, "label": "Workspace", "enabled": true, "source": "desktop-provider" }
        ]
    }

    function ownsMenu(menuId) {
        var id = Number(menuId || 0)
        return id >= 3101 && id <= 3106
    }

    function menuItemsFor(menuId) {
        var id = Number(menuId || 0)
        if (id === 3101) {
            return [
                { "id": 1, "label": "New Folder", "enabled": true },
                { "id": 2, "label": "New File", "enabled": true },
                { "id": 3, "label": "Open", "enabled": hasSelection() },
                { "id": 4, "label": "Open With...", "enabled": hasSelection() },
                { "id": 5, "label": "Open Terminal Here", "enabled": true },
                { "id": 6, "label": "Reveal in Home", "enabled": true },
                { "id": 7, "label": "Compress", "enabled": hasSelection() },
                { "id": 8, "label": "Move to Trash", "enabled": hasSelection() },
                { "id": 9, "label": "Delete Permanently", "enabled": hasSelection() }
            ]
        }
        if (id === 3102) {
            return [
                { "id": 1, "label": "Undo", "enabled": true },
                { "id": 2, "label": "Redo", "enabled": true },
                { "id": 3, "label": "Cut", "enabled": hasSelection() },
                { "id": 4, "label": "Copy", "enabled": hasSelection() },
                { "id": 5, "label": "Paste", "enabled": !!(desktopSurface && desktopSurface.clipboardPaths && desktopSurface.clipboardPaths.length > 0) },
                { "id": 6, "label": "Duplicate", "enabled": hasSelection() },
                { "id": 7, "label": "Select All", "enabled": true },
                { "id": 8, "label": "Deselect", "enabled": hasSelection() },
                { "id": 9, "label": "Rename", "enabled": hasSelection() }
            ]
        }
        if (id === 3103) {
            return [
                { "id": 1, "label": "View as Icons", "enabled": true },
                { "id": 2, "label": "Align Items", "enabled": true },
                { "id": 3, "label": "Sort By", "enabled": true },
                { "id": 4, "label": "Group By", "enabled": true },
                { "id": 5, "label": "Show Hidden Files", "enabled": true },
                { "id": 6, "label": "Show File Extensions", "enabled": true },
                { "id": 7, "label": "Icon Size", "enabled": true },
                { "id": 8, "label": "Grid Spacing", "enabled": true },
                { "id": 9, "label": "Show Item Info", "enabled": hasSelection() }
            ]
        }
        if (id === 3104) {
            return [
                { "id": 1, "label": "Home", "enabled": true },
                { "id": 2, "label": "Desktop", "enabled": true },
                { "id": 3, "label": "Documents", "enabled": true },
                { "id": 4, "label": "Downloads", "enabled": true },
                { "id": 5, "label": "Pictures", "enabled": true },
                { "id": 6, "label": "Videos", "enabled": true },
                { "id": 7, "label": "Recent Files", "enabled": true },
                { "id": 8, "label": "Recent Folders", "enabled": true },
                { "id": 9, "label": "Network", "enabled": true },
                { "id": 10, "label": "Mounted Devices", "enabled": true },
                { "id": 11, "label": "Trash", "enabled": true },
                { "id": 12, "label": "Connect Server...", "enabled": true }
            ]
        }
        if (id === 3105) {
            return [
                { "id": 1, "label": "Open in New Window", "enabled": true },
                { "id": 2, "label": "Open in New Tab", "enabled": true },
                { "id": 3, "label": "Search in Desktop", "enabled": true },
                { "id": 4, "label": "Batch Rename", "enabled": hasSelection() },
                { "id": 5, "label": "Archive Selected", "enabled": hasSelection() },
                { "id": 6, "label": "Create Shortcut / Link", "enabled": hasSelection() },
                { "id": 7, "label": "Properties", "enabled": hasSelection() }
            ]
        }
        if (id === 3106) {
            return [
                { "id": 1, "label": "Show Desktop", "enabled": true },
                { "id": 2, "label": "Move Focus Left", "enabled": true },
                { "id": 3, "label": "Move Focus Right", "enabled": true },
                { "id": 4, "label": "Pin File Manager to Workspace", "enabled": true },
                { "id": 5, "label": "Open Desktop Folder in Split View", "enabled": true }
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
            if (item === 1 && desktopSurface.createNewFolder) { desktopSurface.createNewFolder(); return }
            if (item === 2 && desktopSurface.createNewFile) { desktopSurface.createNewFile(); return }
            if (item === 3 && desktopSurface.openSelection) { desktopSurface.openSelection(); return }
            if (item === 4 && desktopSurface.openSelectionWithAppChooser) { desktopSurface.openSelectionWithAppChooser(); return }
            if (item === 5 && desktopSurface.openTerminalHere) { desktopSurface.openTerminalHere(); return }
            if (item === 6 && desktopSurface.revealSelectionInHome) { desktopSurface.revealSelectionInHome(); return }
            if (item === 7 && desktopSurface.compressSelection) { desktopSurface.compressSelection(); return }
            if (item === 8 && desktopSurface.moveSelectionToTrash) { desktopSurface.moveSelectionToTrash(); return }
            if (item === 9 && desktopSurface.deleteSelectionPermanently) { desktopSurface.deleteSelectionPermanently(); return }
        }
        if (menu === 3102) {
            if (item === 1 && desktopSurface.desktopUndo) { desktopSurface.desktopUndo(); return }
            if (item === 2 && desktopSurface.desktopRedo) { desktopSurface.desktopRedo(); return }
            if (item === 3 && desktopSurface.copySelection) { desktopSurface.copySelection(true); return }
            if (item === 4 && desktopSurface.copySelection) { desktopSurface.copySelection(false); return }
            if (item === 5 && desktopSurface.pasteSelectionToDesktop) { desktopSurface.pasteSelectionToDesktop(); return }
            if (item === 6 && desktopSurface.duplicateSelection) { desktopSurface.duplicateSelection(); return }
            if (item === 7 && desktopSurface.selectAllEntries) { desktopSurface.selectAllEntries(); return }
            if (item === 8 && desktopSurface.clearSelection) { desktopSurface.clearSelection(); return }
            if (item === 9 && desktopSurface.renameSelection) { desktopSurface.renameSelection(); return }
        }
        if (menu === 3103) {
            if (item === 1) { desktopSurface.viewMode = "grid"; return }
            if (item === 2) {
                if (desktopSurface.fileModel && desktopSurface.fileModel.setSort) {
                    desktopSurface.fileModel.setSort("name", false)
                }
                return
            }
            if (item === 3) {
                if (desktopSurface.fileModel && desktopSurface.fileModel.setSort) {
                    var nextKey = "name"
                    var currentKey = String(desktopSurface.fileModel.sortKey || "name")
                    if (currentKey === "name") {
                        nextKey = "dateModified"
                    } else if (currentKey === "dateModified") {
                        nextKey = "size"
                    } else {
                        nextKey = "name"
                    }
                    desktopSurface.fileModel.setSort(nextKey, false)
                }
                return
            }
            if (item === 4) {
                if (desktopSurface.fileModel && desktopSurface.fileModel.directoriesFirst !== undefined) {
                    desktopSurface.fileModel.directoriesFirst = !desktopSurface.fileModel.directoriesFirst
                }
                return
            }
            if (item === 5) {
                if (desktopSurface.fileModel && desktopSurface.fileModel.includeHidden !== undefined) {
                    desktopSurface.fileModel.includeHidden = !desktopSurface.fileModel.includeHidden
                }
                return
            }
            if (item === 6 && desktopSurface.toggleShowFileExtensions) { desktopSurface.toggleShowFileExtensions(); return }
            if (item === 7 && desktopSurface.cycleIconSizeMode) { desktopSurface.cycleIconSizeMode(); return }
            if (item === 8 && desktopSurface.cycleGridSpacingMode) { desktopSurface.cycleGridSpacingMode(); return }
            if (item === 9 && desktopSurface.openPropertiesSelection) { desktopSurface.openPropertiesSelection(); return }
        }
        if (menu === 3104) {
            if (item === 1 && desktopSurface.openPathInFileManager) { desktopSurface.openPathInFileManager("~"); return }
            if (item === 2 && desktopSurface.openPathInFileManager) { desktopSurface.openPathInFileManager(desktopSurface.desktopPath || "~/Desktop"); return }
            if (item === 3 && desktopSurface.openPathInFileManager) { desktopSurface.openPathInFileManager("~/Documents"); return }
            if (item === 4 && desktopSurface.openPathInFileManager) { desktopSurface.openPathInFileManager("~/Downloads"); return }
            if (item === 5 && desktopSurface.openPathInFileManager) { desktopSurface.openPathInFileManager("~/Pictures"); return }
            if (item === 6 && desktopSurface.openPathInFileManager) { desktopSurface.openPathInFileManager("~/Videos"); return }
            if (item === 7 && desktopSurface.openPathInFileManager) { desktopSurface.openPathInFileManager("__recent__"); return }
            if (item === 8 && desktopSurface.openRecentFoldersPath) { desktopSurface.openRecentFoldersPath(); return }
            if (item === 9 && desktopSurface.openPathInFileManager) { desktopSurface.openPathInFileManager("__network__"); return }
            if (item === 10 && desktopSurface.openMountedDevicesPath) { desktopSurface.openMountedDevicesPath(); return }
            if (item === 11 && desktopSurface.openPathInFileManager) { desktopSurface.openPathInFileManager("~/.local/share/Trash/files"); return }
            if (item === 12 && desktopSurface.openConnectServerDialog) { desktopSurface.openConnectServerDialog(); return }
        }
        if (menu === 3105) {
            if (item === 1) { _route("filemanager.open", { "target": desktopSurface.desktopPath || "~/Desktop" }); return }
            if (item === 2) { _route("filemanager.open", { "target": desktopSurface.desktopPath || "~/Desktop" }); return }
            if (item === 3) {
                if (typeof ShellStateController !== "undefined" && ShellStateController && ShellStateController.setPulseVisible) {
                    ShellStateController.setPulseVisible(true)
                }
                return
            }
            if (item === 4 && desktopSurface.renameSelection) { desktopSurface.renameSelection(); return }
            if (item === 5 && desktopSurface.compressSelection) { desktopSurface.compressSelection(); return }
            if (item === 6) {
                if (desktopSurface.fileManagerApiRef && desktopSurface.fileManagerApiRef.copyTextToClipboard) {
                    var rows = desktopSurface.selectedPaths ? desktopSurface.selectedPaths() : []
                    var links = []
                    for (var i = 0; i < rows.length; ++i) {
                        var p = String(rows[i] || "")
                        if (p.length > 0) {
                            links.push("file://" + encodeURIComponent(p).replace(/%2F/g, "/"))
                        }
                    }
                    if (links.length > 0) {
                        desktopSurface.fileManagerApiRef.copyTextToClipboard(links.join("\n"))
                        _notifyInfo("File links copied to clipboard.")
                    }
                }
                return
            }
            if (item === 7 && desktopSurface.openPropertiesSelection) { desktopSurface.openPropertiesSelection(); return }
        }
        if (menu === 3106) {
            if (item === 1) { _route("workspace.toggle", {}); return }
            if (item === 2 || item === 3) {
                if (typeof WindowingBackend !== "undefined" && WindowingBackend && WindowingBackend.sendCommand) {
                    WindowingBackend.sendCommand(item === 2 ? "focus left" : "focus right")
                } else {
                    _notifyInfo(item === 2 ? "Move Focus Left is unavailable." : "Move Focus Right is unavailable.")
                }
                return
            }
            if (item === 4) { _route("workspace.pin_current", {}); return }
            if (item === 5) {
                _route("workspace.split_right", {})
                _route("filemanager.open", { "target": "~/Desktop" })
                return
            }
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
