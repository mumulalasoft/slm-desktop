.pragma library

function handleContextMenuRequested(root, index, x, y) {
    if (!root.fileModel || !root.fileModel.entryAt || index < 0) {
        return
    }
    var row = root.fileModel.entryAt(index)
    if (!row || !row.ok) {
        return
    }
    var selected = root.selectedEntryIndexes || []
    if (selected.indexOf(index) < 0) {
        root.setSingleSelection(index)
    } else {
        root.selectedEntryIndex = index
        if (root.selectionAnchorIndex < 0) {
            root.selectionAnchorIndex = index
        }
    }
    root.contextEntryIndex = index
    root.contextEntryPath = String(row.path || "")
    root.contextEntryName = String(row.name || "")
    root.contextEntryIsDir = !!row.isDir
    if (!root.selectedEntryIndexes || root.selectedEntryIndexes.length <= 1) {
        root.refreshContextOpenWithApps()
    }
    root.popupContextEntryMenu(x, y)
    root.forceActiveFocus()
}

function handleActivateRequested(root, index) {
    root.setSingleSelection(index)
    if (!root.fileModel || !root.fileModel.activate) {
        return
    }
    var res = root.fileModel.activate(index)
    if (res && res.ok && res.type === "file") {
        var launchRes = root.openTargetViaExecutionGate(String(res.path || ""),
                                                        "filemanager-activate")
        if (!launchRes || !launchRes.ok) {
            root.notifyResult("Open", launchRes)
        }
    }
}

function handleRenameRequested(root, index) {
    if (!root.fileModel || !root.fileModel.entryAt || index < 0) {
        return
    }
    var row = root.fileModel.entryAt(index)
    if (!row || !row.ok) {
        return
    }
    root.setSingleSelection(index)
    root.contextEntryIndex = index
    root.contextEntryPath = String(row.path || "")
    root.contextEntryName = String(row.name || root.basename(root.contextEntryPath))
    root.contextEntryIsDir = !!row.isDir
    root.requestRenameContextEntry()
}

function handleBackgroundContextRequested(root, menusRef, x, y) {
    var currentDir = String((root.fileModel
                             && root.fileModel.currentPath !== undefined)
                            ? root.fileModel.currentPath : "~")
    if (currentDir.length <= 0) {
        currentDir = "~"
    }
    root.contextEntryIndex = -1
    root.contextEntryPath = currentDir
    root.contextEntryName = root.basename(currentDir)
    root.contextEntryIsDir = true
    root.refreshContextOpenWithApps()
    if (menusRef && menusRef.openFolderMenu) {
        menusRef.openFolderMenu(x, y)
    }
    root.forceActiveFocus()
}
