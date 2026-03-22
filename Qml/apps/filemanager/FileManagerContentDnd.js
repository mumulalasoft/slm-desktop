.pragma library

function beginContentDrag(root, contentView, dndSidebarSpringOpenTimer, index, pathValue, nameValue,
                          copyMode, sceneX, sceneY) {
    var srcPath = String(pathValue || "")
    var selected = root.selectedPaths ? root.selectedPaths() : []
    if (!selected || selected.length <= 0
            || (srcPath.length > 0 && selected.indexOf(srcPath) < 0)) {
        selected = srcPath.length > 0 ? [srcPath] : []
    }
    if (!selected || selected.length <= 0) {
        return
    }
    root.dndActive = true
    root.dndCopyMode = !!copyMode
    root.dndSourcePaths = selected.slice(0)
    root.dndSidebarHoverPath = ""
    root.dndSidebarSpringOpenPath = ""
    root.dndGhostLabel = selected.length > 1 ? (String(
                                                    selected.length) + " items") : String(
                                                   nameValue || root.basename(
                                                       srcPath))
    root.dndGhostX = Number(sceneX || 0)
    root.dndGhostY = Number(sceneY || 0)
    if (contentView) {
        contentView.dndActive = true
    }
    if (dndSidebarSpringOpenTimer) {
        dndSidebarSpringOpenTimer.stop()
    }
}

function updateContentDrag(root, sidebarDropTargetAt, dndSidebarSpringOpenTimer, sceneX, sceneY, copyMode) {
    if (!root.dndActive) {
        return
    }
    root.dndCopyMode = !!copyMode
    root.dndGhostX = Number(sceneX || 0)
    root.dndGhostY = Number(sceneY || 0)
    var hit = sidebarDropTargetAt(sceneX, sceneY)
    var nextHover = hit.ok ? String(hit.path || "") : ""
    if (nextHover !== root.dndSidebarHoverPath) {
        root.dndSidebarHoverPath = nextHover
        root.dndSidebarSpringOpenPath = nextHover
        if (dndSidebarSpringOpenTimer) {
            dndSidebarSpringOpenTimer.stop()
            if (nextHover.length > 0 && nextHover !== "__trash__") {
                dndSidebarSpringOpenTimer.start()
            }
        }
    }
}

function finishContentDrag(root, contentView, dndSidebarSpringOpenTimer, fileManagerApi) {
    if (!root.dndActive) {
        return
    }
    var targetPath = String(root.dndSidebarHoverPath || "")
    var sourcePaths = root.dndSourcePaths ? root.dndSourcePaths.slice(0) : []
    var copyMode = !!root.dndCopyMode

    root.dndActive = false
    root.dndCopyMode = false
    root.dndSourcePaths = []
    root.dndSidebarHoverPath = ""
    root.dndSidebarSpringOpenPath = ""
    if (dndSidebarSpringOpenTimer) {
        dndSidebarSpringOpenTimer.stop()
    }
    root.dndGhostLabel = ""
    if (contentView) {
        contentView.dndActive = false
    }

    if (targetPath.length <= 0 || sourcePaths.length <= 0 || !fileManagerApi) {
        return
    }

    var candidates = []
    for (var i = 0; i < sourcePaths.length; ++i) {
        var src = String(sourcePaths[i] || "")
        if (src.length <= 0) {
            continue
        }
        if (!copyMode) {
            if (targetPath === src || root.parentPathOf(src) === targetPath
                    || root.isPathSameOrInside(targetPath, src)) {
                continue
            }
        }
        if (candidates.indexOf(src) < 0) {
            candidates.push(src)
        }
    }
    if (candidates.length <= 0) {
        return
    }

    var res
    if (targetPath === "__trash__") {
        if (copyMode || !fileManagerApi.startTrashPaths) {
            return
        }
        res = fileManagerApi.startTrashPaths(candidates)
        root.notifyResult("Move to Trash", res)
        return
    }

    if (root.startResolveSlmDragDropAction) {
        root.startResolveSlmDragDropAction(candidates, targetPath, copyMode)
        root.clearSelection()
        return
    }

    if (copyMode) {
        if (!fileManagerApi.startCopyPaths) {
            root.notifyResult("Copy", {
                                  "ok": false,
                                  "error": "async-copy-api-unavailable"
                              })
            return
        }
        res = fileManagerApi.startCopyPaths(candidates, targetPath, false)
        root.notifyResult("Copy", res)
        return
    }
    if (!fileManagerApi.startMovePaths) {
        root.notifyResult("Move", {
                              "ok": false,
                              "error": "async-move-api-unavailable"
                          })
        return
    }
    res = fileManagerApi.startMovePaths(candidates, targetPath, false)
    root.notifyResult("Move", res)
    if (res && res.ok) {
        root.clearSelection()
    }
}
