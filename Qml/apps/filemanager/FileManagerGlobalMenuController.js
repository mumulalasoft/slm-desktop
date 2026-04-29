.pragma library

function overrideMenus() {
    return [
        {
            "id": 2001,
            "label": "File",
            "enabled": true,
            "items": [
                { "id": 1, "label": "New Folder", "enabled": true },
                { "id": 2, "label": "New File", "enabled": true },
                { "id": 3, "label": "Open", "enabled": true },
                { "id": 4, "label": "Open With...", "enabled": true },
                { "id": 5, "label": "Open Terminal Here", "enabled": true },
                { "id": 6, "label": "Reveal in Home", "enabled": true },
                { "id": 7, "label": "Compress", "enabled": true },
                { "id": 8, "label": "Move to Trash", "enabled": true },
                { "id": 9, "label": "Delete Permanently", "enabled": true }
            ]
        },
        {
            "id": 2002,
            "label": "Edit",
            "enabled": true,
            "items": [
                { "id": 1, "label": "Undo", "enabled": true },
                { "id": 2, "label": "Redo", "enabled": true },
                { "id": 3, "label": "Cut", "enabled": true },
                { "id": 4, "label": "Copy", "enabled": true },
                { "id": 5, "label": "Paste", "enabled": true },
                { "id": 6, "label": "Duplicate", "enabled": true },
                { "id": 7, "label": "Select All", "enabled": true },
                { "id": 8, "label": "Deselect", "enabled": true },
                { "id": 9, "label": "Rename", "enabled": true }
            ]
        },
        {
            "id": 2003,
            "label": "View",
            "enabled": true,
            "items": [
                { "id": 1, "label": "View as Icons", "enabled": true },
                { "id": 2, "label": "Align Items", "enabled": true },
                { "id": 3, "label": "Sort By", "enabled": true },
                { "id": 4, "label": "Group By", "enabled": true },
                { "id": 5, "label": "Show Hidden Files", "enabled": true },
                { "id": 6, "label": "Show File Extensions", "enabled": true },
                { "id": 7, "label": "Icon Size", "enabled": true },
                { "id": 8, "label": "Grid Spacing", "enabled": true },
                { "id": 9, "label": "Show Item Info", "enabled": true }
            ]
        },
        {
            "id": 2101,
            "label": "Go",
            "enabled": true,
            "items": [
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
        },
        {
            "id": 2004,
            "label": "Tools",
            "enabled": true,
            "items": [
                { "id": 1, "label": "Open in New Window", "enabled": true },
                { "id": 2, "label": "Open in New Tab", "enabled": true },
                { "id": 3, "label": "Search in Desktop", "enabled": true },
                { "id": 4, "label": "Batch Rename", "enabled": true },
                { "id": 5, "label": "Archive Selected", "enabled": true },
                { "id": 6, "label": "Create Shortcut / Link", "enabled": true },
                { "id": 7, "label": "Properties", "enabled": true }
            ]
        },
        {
            "id": 2005,
            "label": "Workspace",
            "enabled": true,
            "items": [
                { "id": 1, "label": "Show Desktop", "enabled": true },
                { "id": 2, "label": "Move Focus Left", "enabled": true },
                { "id": 3, "label": "Move Focus Right", "enabled": true },
                { "id": 4, "label": "Pin File Manager to Workspace", "enabled": true },
                { "id": 5, "label": "Open Desktop Folder in Split View", "enabled": true }
            ]
        }
    ]
}

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
        GlobalMenuManager.setOverrideMenus(overrideMenus(), "filemanager")
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
    // No top-level direct actions for filemanager override.
    // Actual actions are handled through overrideMenuItemActivated.
}
