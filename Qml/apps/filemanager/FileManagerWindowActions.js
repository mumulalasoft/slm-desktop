.pragma library

function titleButtonIcon(root, kind, hovered, pressed) {
    var base = "qrc:/icons/titlebuttons/"
    var active = !!root.windowActive
    if (kind === "close") {
        if (!active) {
            return base + ((hovered || pressed)
                           ? "titlebutton-close-backdrop-active.svg"
                           : "titlebutton-close-backdrop.svg")
        }
        if (pressed) {
            return base + "titlebutton-close-active.svg"
        }
        if (hovered) {
            return base + "titlebutton-close-hover.svg"
        }
        return base + "titlebutton-close.svg"
    }
    if (kind === "minimize") {
        if (!active) {
            return base + ((hovered || pressed)
                           ? "titlebutton-minimize-backdrop-active.svg"
                           : "titlebutton-minimize-backdrop.svg")
        }
        if (pressed) {
            return base + "titlebutton-minimize-active.svg"
        }
        if (hovered) {
            return base + "titlebutton-minimize-hover.svg"
        }
        return base + "titlebutton-minimize.svg"
    }
    if (kind === "maximize") {
        if (!!root.windowMaximized) {
            if (!active) {
                return base + ((hovered || pressed)
                               ? "titlebutton-unmaximize-backdrop-active.svg"
                               : "titlebutton-unmaximize-backdrop.svg")
            }
            if (pressed) {
                return base + "titlebutton-unmaximize-active.svg"
            }
            if (hovered) {
                return base + "titlebutton-unmaximize-hover.svg"
            }
            return base + "titlebutton-unmaximize.svg"
        }
        if (!active) {
            return base + ((hovered || pressed)
                           ? "titlebutton-maximize-backdrop-active.svg"
                           : "titlebutton-maximize-backdrop.svg")
        }
        if (pressed) {
            return base + "titlebutton-maximize-active.svg"
        }
        if (hovered) {
            return base + "titlebutton-maximize-hover.svg"
        }
        return base + "titlebutton-maximize.svg"
    }
    return ""
}

function minimizeWindow(hostWindow) {
    if (hostWindow && hostWindow.showMinimized) {
        hostWindow.showMinimized()
    }
}

function toggleMaximizeWindow(hostWindow, windowMaximized, windowType) {
    if (!hostWindow) {
        return
    }
    if (!!windowMaximized) {
        if (hostWindow.showNormal) {
            hostWindow.showNormal()
        } else {
            hostWindow.visibility = windowType.Windowed
        }
        return
    }
    if (hostWindow.showMaximized) {
        hostWindow.showMaximized()
    } else {
        hostWindow.visibility = windowType.Maximized
    }
}

function notifyResult(notificationManager, title, resultValue) {
    if (!notificationManager || !notificationManager.Notify) {
        return
    }
    var action = String(title || "")
    var ok = !!(resultValue && resultValue.ok)
    if (ok) {
        var deferredBatchAction = (action === "Copy" || action === "Move"
                                   || action === "Move to Trash"
                                   || action === "Delete"
                                   || action === "Delete Permanently"
                                   || action === "Restore")
        if (deferredBatchAction) {
            return
        }
    }
    var body = ok ? "Success" : String(
                        (resultValue && resultValue.error)
                        ? resultValue.error : "Operation failed")
    notificationManager.Notify(
                action, 0,
                ok ? "dialog-information-symbolic" : "dialog-error-symbolic",
                "File Manager", body, [], {}, 2800)
}

function createAsyncRequestId() {
    return String(Date.now()) + "-" + String(Math.floor(Math.random() * 1000000))
}

function trackOpenAction(root, requestId, title, refreshOpenWith) {
    var rid = String(requestId || "")
    if (rid.length === 0) {
        return
    }
    var next = {}
    var keys = Object.keys(root.pendingOpenActions || {})
    for (var i = 0; i < keys.length; ++i) {
        var k = keys[i]
        next[k] = root.pendingOpenActions[k]
    }
    next[rid] = {
        "title": String(title || "Open"),
        "refreshOpenWith": !!refreshOpenWith
    }
    root.pendingOpenActions = next
}

function openTargetViaExecutionGate(root, appCommandRouter, fileManagerApi, pathValue, sourceLabel) {
    var p = String(pathValue || "")
    if (p.length === 0) {
        return ({
                    "ok": false,
                    "error": "invalid-path"
                })
    }
    if (appCommandRouter && appCommandRouter.routeWithResult) {
        var routed = appCommandRouter.routeWithResult(
                    "filemanager.open",
                    {
                        "target": p
                    },
                    String(sourceLabel || "filemanager"))
        return routed
    }
    if (fileManagerApi && fileManagerApi.startOpenPath) {
        var reqId = createAsyncRequestId()
        var launchRes = fileManagerApi.startOpenPath(p, reqId)
        if (launchRes && launchRes.ok) {
            trackOpenAction(root, reqId, "Open", false)
        }
        return launchRes || ({
                                "ok": false,
                                "error": "open-failed"
                            })
    }
    return ({
                "ok": false,
                "error": "open-unavailable"
            })
}

function openContextEntry(root, fileModel, appCommandRouter, fileManagerApi) {
    if (!fileModel || !fileModel.activate || root.contextEntryIndex < 0) {
        return
    }
    root.selectedEntryIndex = root.contextEntryIndex
    var res = fileModel.activate(root.contextEntryIndex)
    if (res && res.ok && res.type === "file") {
        var launchRes = openTargetViaExecutionGate(
                    root, appCommandRouter, fileManagerApi, String(res.path || ""),
                    "filemanager-context")
        if (!launchRes || !launchRes.ok) {
            root.notifyResult("Open", launchRes)
        }
    }
}

function openContextEntryInNewWindow(root) {
    if (!root.contextEntryIsDir) {
        return
    }
    var p = String(root.contextEntryPath || "")
    if (p.length === 0) {
        return
    }
    root.openInNewWindowRequested(p)
}

function openMenuAt(menuRef, px, py) {
    if (!menuRef || menuRef.open === undefined) {
        return
    }
    menuRef.x = px
    menuRef.y = py
    menuRef.open()
}

function moveContextEntryToTrash(root, fileManagerApi) {
    var src = String(root.contextEntryPath || "")
    if (src.length === 0 || !fileManagerApi || !fileManagerApi.startTrashPaths) {
        return
    }
    var res = fileManagerApi.startTrashPaths([src])
    root.notifyResult("Move to Trash", res)
}
