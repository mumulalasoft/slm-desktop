.pragma library

function begin(root, index, pathValue, nameValue, isDirValue, copyMode, sceneX, sceneY) {
    root.dndActive = true
    root.dndSourceIndex = index
    root.dndSourcePath = String(pathValue || "")
    root.dndSourceName = String(nameValue || "")
    root.dndSourceIsDir = !!isDirValue
    root.dndCopyMode = !!copyMode
    root.dndHoverIndex = -1
    root.dndGhostX = sceneX + 10
    root.dndGhostY = sceneY + 10
}

function update(root, sceneX, sceneY, hoverIndex, copyMode, fileModel) {
    if (!root.dndActive) {
        return
    }
    root.dndCopyMode = !!copyMode
    root.dndGhostX = sceneX + 10
    root.dndGhostY = sceneY + 10

    var idx = hoverIndex
    if (idx >= 0 && idx !== root.dndSourceIndex) {
        var item = fileModel && fileModel.entryAt ? fileModel.entryAt(idx) : {}
        root.dndHoverIndex = (item && item.ok && item.isDir) ? idx : -1
    } else {
        root.dndHoverIndex = -1
    }
}

function finish(root, targetIndex, fileModel, fileManagerApi) {
    if (!root.dndActive) {
        return
    }

    var idx = targetIndex
    var sourcePath = root.dndSourcePath
    var sourceName = root.dndSourceName
    var copyMode = root.dndCopyMode

    root.dndActive = false
    root.dndSourceIndex = -1
    root.dndSourcePath = ""
    root.dndSourceName = ""
    root.dndSourceIsDir = false
    root.dndCopyMode = false
    root.dndHoverIndex = -1

    if (idx < 0 || !sourcePath || !fileModel || !fileModel.entryAt || !fileManagerApi) {
        return
    }

    var targetItem = fileModel.entryAt(idx)
    if (!targetItem || !targetItem.ok || !targetItem.isDir) {
        return
    }

    var targetDir = String(targetItem.path || "")
    var srcParent = sourcePath.slice(0, Math.max(0, sourcePath.lastIndexOf("/")))
    if (targetDir === srcParent || targetDir === sourcePath) {
        return
    }

    var result
    if (root.startResolveSlmDragDropAction) {
        root.startResolveSlmDragDropAction([sourcePath], targetDir, copyMode)
        if (root.selectedIndex !== undefined) {
            root.selectedIndex = -1
        }
        return
    }

    if (copyMode) {
        if (!fileManagerApi.startCopyPaths) {
            result = { "ok": false, "error": "async-copy-api-unavailable" }
        } else {
            result = fileManagerApi.startCopyPaths([sourcePath], targetDir, false)
        }
    } else {
        if (!fileManagerApi.startMovePaths) {
            result = { "ok": false, "error": "async-move-api-unavailable" }
        } else {
            result = fileManagerApi.startMovePaths([sourcePath], targetDir, false)
        }
    }
    root.notifyResult(copyMode ? "Copy" : "Move", result)
    if (result && result.ok) {
        root.selectedIndex = -1
    }
}
