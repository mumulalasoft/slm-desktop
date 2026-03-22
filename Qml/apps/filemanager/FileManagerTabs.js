.pragma library

function tabTitleFromPath(root, pathValue) {
    var p = String(pathValue || "")
    if (p.length === 0 || p === "~") {
        return "Home"
    }
    if (p === "__recent__") {
        return "Recent"
    }
    if (p === root.trashFilesPath || p.indexOf("/.local/share/Trash/files") >= 0
            || p.indexOf("trash://") === 0) {
        return "Trash"
    }
    return root.basename(p)
}

function tabCount(root) {
    return root.tabModel ? Number(root.tabModel.count || 0) : 0
}

function tabPathAt(root, indexValue) {
    var index = Number(indexValue)
    if (!(index >= 0) || !root.tabModel || index >= root.tabModel.count) {
        return "~"
    }
    var row = root.tabModel.get(index)
    return String((row && row.path) ? row.path : "~")
}

function tabModelRefAt(root, indexValue) {
    var index = Number(indexValue)
    if (!(index >= 0) || !root.tabModel || index >= root.tabModel.count) {
        return null
    }
    var refs = root.tabModelRefs || []
    return (index < refs.length) ? refs[index] : null
}

function createOwnedTabModel(root, fileManagerModelFactory, pathValue) {
    var p = String(pathValue || "~")
    var modelObj = null
    if (fileManagerModelFactory && fileManagerModelFactory.createModel) {
        modelObj = fileManagerModelFactory.createModel(root)
    }
    if (!modelObj) {
        return null
    }
    if (root.primaryFileModel) {
        if (modelObj.includeHidden !== undefined
                && root.primaryFileModel.includeHidden !== undefined) {
            modelObj.includeHidden = !!root.primaryFileModel.includeHidden
        }
        if (modelObj.directoriesFirst !== undefined
                && root.primaryFileModel.directoriesFirst !== undefined) {
            modelObj.directoriesFirst = !!root.primaryFileModel.directoriesFirst
        }
    }
    if (modelObj.currentPath !== undefined) {
        modelObj.currentPath = p
    }
    var list = root.ownedTabModels ? root.ownedTabModels.slice(0) : []
    list.push(modelObj)
    root.ownedTabModels = list
    return modelObj
}

function destroyOwnedTabModel(root, fileManagerModelFactory, modelObj) {
    if (!modelObj) {
        return
    }
    var next = []
    var found = false
    var list = root.ownedTabModels || []
    for (var i = 0; i < list.length; ++i) {
        if (list[i] === modelObj) {
            found = true
            continue
        }
        next.push(list[i])
    }
    if (!found) {
        return
    }
    root.ownedTabModels = next
    if (fileManagerModelFactory && fileManagerModelFactory.destroyModel) {
        fileManagerModelFactory.destroyModel(modelObj)
    }
}

function updateTabPath(root, uiPreferences, indexValue, pathValue) {
    var index = Number(indexValue)
    if (!(index >= 0) || !root.tabModel || index >= root.tabModel.count) {
        return
    }
    var p = String(pathValue || "~")
    root.tabModel.setProperty(index, "path", p)
    var modelObj = tabModelRefAt(root, index)
    if (modelObj && modelObj.currentPath !== undefined && String(
                modelObj.currentPath || "") !== p) {
        modelObj.currentPath = p
    }
    saveTabState(root, uiPreferences)
}

function addTab(root, fileManagerModelFactory, uiPreferences, pathValue, activate) {
    if (!root.tabModel || root.tabModel.count <= 0) {
        root.initSingleTab(root.selectedSidebarPath)
    }
    var p = String(pathValue || "")
    if (p.length === 0) {
        p = String((root.fileModel
                    && root.fileModel.currentPath) ? root.fileModel.currentPath : "~")
    }
    if (p.length === 0) {
        p = "~"
    }
    if (!root.tabModel) {
        return
    }
    var modelObj = createOwnedTabModel(root, fileManagerModelFactory, p)
    if (!modelObj) {
        modelObj = root.fileModel || root.primaryFileModel
        if (modelObj && modelObj.currentPath !== undefined) {
            modelObj.currentPath = p
        }
    }
    root.tabModel.append({
                             "path": p
                         })
    var refs = root.tabModelRefs ? root.tabModelRefs.slice(0) : []
    refs.push(modelObj)
    root.tabModelRefs = refs
    saveTabState(root, uiPreferences)
    if (activate !== false) {
        switchToTab(root, indexOfLastTab(root))
    }
}

function openPathInNewTab(root, fileManagerModelFactory, uiPreferences, pathValue) {
    if (!root.tabModel || root.tabModel.count <= 0) {
        root.initSingleTab(root.selectedSidebarPath)
    }
    var p = String(pathValue || "")
    if (p.length === 0) {
        return
    }
    if (p === "__network__") {
        p = "~"
    }
    if (p === "__trash__") {
        p = root.trashFilesPath
    }
    if (!root.tabModel) {
        return
    }
    var modelObj = createOwnedTabModel(root, fileManagerModelFactory, p)
    if (!modelObj) {
        modelObj = root.fileModel || root.primaryFileModel
        if (modelObj && modelObj.currentPath !== undefined) {
            modelObj.currentPath = p
        }
    }
    var insertIndex = root.tabModel.count
    root.tabModel.insert(insertIndex, {
                             "path": p
                         })
    var refs = root.tabModelRefs ? root.tabModelRefs.slice(0) : []
    refs.splice(insertIndex, 0, modelObj)
    root.tabModelRefs = refs
    root.activeTabIndex = insertIndex
    saveTabState(root, uiPreferences)
    switchToTab(root, insertIndex)
}

function switchToTab(root, indexValue) {
    var index = Number(indexValue)
    if (!(index >= 0) || !root.tabModel || index >= root.tabModel.count) {
        return
    }
    var nextModel = tabModelRefAt(root, index)
    if (!nextModel) {
        nextModel = root.primaryFileModel
    }
    if (nextModel && root.fileModel !== nextModel) {
        root.fileModel = nextModel
    }
    root.activeTabIndex = index
    var tabPath = tabPathAt(root, index)
    root.openPath(tabPath)
}

function closeTab(root, fileManagerModelFactory, uiPreferences, indexValue) {
    var index = Number(indexValue)
    if (!(index >= 0) || !root.tabModel || index >= root.tabModel.count) {
        return
    }
    if (root.tabModel.count <= 1) {
        return
    }
    var modelObj = tabModelRefAt(root, index)
    if (modelObj && modelObj !== root.primaryFileModel) {
        destroyOwnedTabModel(root, fileManagerModelFactory, modelObj)
    }
    var refs = root.tabModelRefs ? root.tabModelRefs.slice(0) : []
    if (index >= 0 && index < refs.length) {
        refs.splice(index, 1)
    }
    root.tabModelRefs = refs
    root.tabModel.remove(index)
    if (root.activeTabIndex >= root.tabModel.count) {
        root.activeTabIndex = root.tabModel.count - 1
    } else if (index <= root.activeTabIndex) {
        root.activeTabIndex = Math.max(0, root.activeTabIndex - 1)
    }
    saveTabState(root, uiPreferences)
    switchToTab(root, root.activeTabIndex)
}

function saveTabState(root, uiPreferences) {
    if (!root.tabStateReady || !uiPreferences || !uiPreferences.setPreference) {
        return
    }
    var paths = []
    for (var i = 0; i < tabCount(root); ++i) {
        paths.push(tabPathAt(root, i))
    }
    if (paths.length <= 0) {
        paths = ["~"]
    }
    var safeIndex = Math.max(0, Math.min(paths.length - 1,
                                         Number(root.activeTabIndex || 0)))
    uiPreferences.setPreference("filemanager.tabs.pathsJson",
                                JSON.stringify(paths))
    uiPreferences.setPreference("filemanager.tabs.activeIndex", safeIndex)
}

function restoreTabState(root, defaultPath) {
    root.initSingleTab(defaultPath)
}

function openContextEntryInNewTab(root, fileManagerModelFactory, uiPreferences) {
    if (!root.contextEntryIsDir) {
        return
    }
    var p = String(root.contextEntryPath || "")
    if (p.length <= 0) {
        return
    }
    openPathInNewTab(root, fileManagerModelFactory, uiPreferences, p)
}

function indexOfLastTab(root) {
    return root.tabModel ? (root.tabModel.count - 1) : -1
}
