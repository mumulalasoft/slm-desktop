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

function isStorageAction(actionValue) {
    var actionName = String(actionValue || "")
    return actionName === "Open Drive" || actionName === "Eject"
}

function storageStateMessage(storageKey) {
    var k = String(storageKey || "").toLowerCase()
    if (k.length <= 0) {
        return ""
    }
    if (k.indexOf("lock") >= 0 || k.indexOf("encrypted") >= 0
            || k.indexOf("drive-locked") >= 0) {
        return "Drive terkunci. Buka kunci lalu coba lagi."
    }
    if (k.indexOf("busy") >= 0 || k.indexOf("in-use") >= 0
            || k.indexOf("in use") >= 0) {
        return "Drive sedang digunakan. Tutup file yang masih dipakai lalu coba lagi."
    }
    if (k.indexOf("unsupported") >= 0 || k.indexOf("unknown-fs") >= 0
            || k.indexOf("unknown filesystem") >= 0) {
        return "Drive tidak dikenali."
    }
    if (k.indexOf("permission") >= 0 || k.indexOf("denied") >= 0) {
        return "Akses ke drive ditolak."
    }
    if (k.indexOf("timeout") >= 0 || k.indexOf("timed out") >= 0) {
        return "Drive tidak merespons tepat waktu."
    }
    if (k === "mount-not-found" || k === "volume-not-available") {
        return "Drive tidak ditemukan."
    }
    if (k === "mount-path-unavailable") {
        return "Drive berhasil di-mount, tetapi folder mount tidak ditemukan."
    }
    if (k === "daemon-unavailable"
            || k === "mount-api-unavailable"
            || k === "unmount-api-unavailable"
            || k === "eject-api-unavailable") {
        return "Layanan drive tidak tersedia."
    }
    if (k === "daemon-timeout") {
        return "Layanan drive tidak merespons tepat waktu."
    }
    if (k === "operation-cancelled" || k === "cancelled" || k === "canceled") {
        return "Operasi dibatalkan."
    }
    return ""
}

function sanitizeStorageRaw(raw) {
    var text = String(raw || "").trim()
    if (text.length <= 0) {
        return ""
    }
    text = text.replace(/\/dev\/[A-Za-z0-9._-]+/g, "drive")
    text = text.replace(/\b(part)?uuid[:= ]*[A-Fa-f0-9-]{6,}\b/g, "id")
    text = text.replace(/org\.freedesktop\.[A-Za-z0-9._-]+/g, "")
    text = text.replace(/GDBus\.Error:[^:]+:/g, "")
    text = text.replace(/\s+/g, " ").trim()
    return text
}

function nonTechnicalStorageError(actionValue, payload) {
    var actionName = String(actionValue || "")
    var rawError = String((payload && payload.error) ? payload.error : "")
    var code = String((payload && payload.errorCode) ? payload.errorCode : "")
    var key = (code.length > 0 ? code : rawError).toLowerCase()

    var mapped = storageStateMessage(key)
    if (mapped.length > 0) {
        return mapped
    }
    var safe = sanitizeStorageRaw(rawError)
    if (safe.length > 0) {
        if (actionName === "Open Drive") {
            return "Tidak bisa membuka drive: " + safe
        }
        return "Tidak bisa mengeluarkan drive: " + safe
    }
    return actionName === "Open Drive"
            ? "Tidak bisa membuka drive."
            : "Tidak bisa mengeluarkan drive."
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
    function normalizedError(actionValue, payload) {
        var actionName = String(actionValue || "")
        var rawError = String((payload && payload.error) ? payload.error : "")
        var code = String((payload && payload.errorCode) ? payload.errorCode : "")
        var key = (code.length > 0 ? code : rawError).toLowerCase()

        if (actionName === "Extract") {
            if (key === "err_not_found" || key === "not-found") return "Archive file was not found."
            if (key === "err_unsupported_or_corrupt") return "Archive is unsupported or corrupted."
            if (key === "err_timeout" || key === "archive-job-timeout") return "Extraction timed out."
            if (key === "err_cancelled") return "Extraction was cancelled."
            if (key === "err_resource_limit") return "Extraction was blocked by safety limits."
            if (key === "err_name_conflict") return "Destination folder already exists."
            if (key === "extract-tool-unavailable" || key === "err_extract_tool_unavailable")
                return "No archive extractor tool is installed."
            if (key === "extract-failed" || key === "err_extract_failed" || key === "archive-job-failed")
                return "Could not extract this archive."
            if (key === "archive-service-unavailable" || key === "archive-api-unavailable") return "Archive service is unavailable."
            if (key === "async-extract-api-unavailable") return "Extract API is unavailable."
            if (key === "missing-destination") return "Destination folder is required."
            if (rawError.length > 0) return "Extract failed: " + rawError
            return "Extract failed."
        }

        if (isStorageAction(actionName)) {
            return nonTechnicalStorageError(actionName, payload)
        }

        if (rawError.length > 0) {
            return rawError
        }
        return "Operation failed"
    }

    var body = ok ? "Success" : normalizedError(action, resultValue || ({}))
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
    if (res && res.ok && res.type === "archive") {
        if (!fileManagerApi || !fileManagerApi.startExtractArchive) {
            root.notifyResult("Extract", {
                                  "ok": false,
                                  "error": "archive-api-unavailable"
                              })
            return
        }
        var extractRes = fileManagerApi.startExtractArchive(String(res.path || ""), "")
        if (!extractRes || !extractRes.ok) {
            root.notifyResult("Extract", extractRes || {
                                  "ok": false,
                                  "error": "extract-failed"
                              })
        }
        return
    }
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
