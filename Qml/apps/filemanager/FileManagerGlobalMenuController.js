.pragma library
.import "../../components/shell/ShellUtils.js" as ShellUtils
.import "../../components/screenshot/ScreenshotController.js" as ScreenshotController

function applyOverride(enabled) {
    if (typeof GlobalMenuManager === "undefined" || !GlobalMenuManager) {
        return
    }
    if (!enabled) {
        if (GlobalMenuManager.clearOverrideMenus) {
            GlobalMenuManager.clearOverrideMenus()
        }
        return
    }
    if (GlobalMenuManager.setOverrideMenus) {
        GlobalMenuManager.setOverrideMenus([
                                           { "id": 2001, "label": "File", "enabled": true },
                                           { "id": 2002, "label": "Edit", "enabled": true },
                                           { "id": 2003, "label": "View", "enabled": true },
                                           { "id": 2004, "label": "Go", "enabled": true },
                                           { "id": 2005, "label": "Window", "enabled": true },
                                           { "id": 2006, "label": "Help", "enabled": true }
                                       ], "filemanager")
    }
}

function syncOverride(shell, detachedWindow) {
    var detachedActive = false
    try {
        detachedActive = !!(detachedWindow
                            && detachedWindow.visible
                            && detachedWindow.active)
    } catch (e) {
        detachedActive = false
    }
    applyOverride(detachedActive)
}

function handleMenu(shell, menuId, fileManagerContent) {
    var id = Number(menuId || 0)
    if (!fileManagerContent) {
        return
    }
    if (id === 2001) { // File
        fileManagerContent.openHome()
        return
    }
    if (id === 2002) { // Edit
        fileManagerContent.copyOrMoveSelected(false)
        return
    }
    if (id === 2003) { // View
        fileManagerContent.viewMode = (fileManagerContent.viewMode === "grid") ? "list" : "grid"
        return
    }
    if (id === 2004) { // Go
        if (fileManagerContent.fileModel && fileManagerContent.fileModel.goUp) {
            fileManagerContent.fileModel.goUp()
        }
        return
    }
    if (id === 2005) { // Window
        ShellUtils.openDetachedFileManager(shell,
                                           fileManagerContent.fileModel && fileManagerContent.fileModel.currentPath
                                           ? String(fileManagerContent.fileModel.currentPath)
                                           : "~")
        return
    }
    if (id === 2006) { // Help
        ScreenshotController.showResultNotification(shell, true, "", "FileManager: Ctrl+P Print, Ctrl+C/Ctrl+V Copy/Paste, F2 Rename, Delete Trash.")
    }
}
