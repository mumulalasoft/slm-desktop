.pragma library

function recordNavigationPath(root, pathValue) {
    var nextPath = String(pathValue || "")
    if (nextPath.length <= 0) {
        return
    }
    if (root.navigationInternalChange) {
        root.navigationInternalChange = false
        root.navigationLastPath = nextPath
        return
    }
    var prevPath = String(root.navigationLastPath || "")
    if (prevPath.length > 0 && prevPath !== nextPath) {
        var back = root.navigationBackStack ? root.navigationBackStack.slice(0) : []
        back.push(prevPath)
        if (back.length > 200) {
            back.shift()
        }
        root.navigationBackStack = back
        root.navigationForwardStack = []
    }
    root.navigationLastPath = nextPath
}

function navigateBack(root) {
    if (!root.fileModel || !root.canNavigateBack) {
        return
    }
    var back = root.navigationBackStack.slice(0)
    var targetPath = String(back.pop() || "")
    if (targetPath.length <= 0) {
        root.navigationBackStack = back
        return
    }
    var currentPath = String(root.fileModel.currentPath || "")
    var forward = root.navigationForwardStack ? root.navigationForwardStack.slice(0) : []
    if (currentPath.length > 0) {
        forward.push(currentPath)
    }
    root.navigationBackStack = back
    root.navigationForwardStack = forward
    root.navigationInternalChange = true
    root.fileModel.currentPath = targetPath
    root.clearSelection()
}

function navigateForward(root) {
    if (!root.fileModel || !root.canNavigateForward) {
        return
    }
    var forward = root.navigationForwardStack.slice(0)
    var targetPath = String(forward.pop() || "")
    if (targetPath.length <= 0) {
        root.navigationForwardStack = forward
        return
    }
    var currentPath = String(root.fileModel.currentPath || "")
    var back = root.navigationBackStack ? root.navigationBackStack.slice(0) : []
    if (currentPath.length > 0) {
        back.push(currentPath)
    }
    root.navigationForwardStack = forward
    root.navigationBackStack = back
    root.navigationInternalChange = true
    root.fileModel.currentPath = targetPath
    root.clearSelection()
}
