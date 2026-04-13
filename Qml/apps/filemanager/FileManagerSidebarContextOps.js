.pragma library

function fallbackPathFromSidebarLabel(root) {
    var label = String(root.sidebarContextLabel || "").trim().toLowerCase()
    if (label === "home") return "~"
    if (label === "documents") return "~/Documents"
    if (label === "music") return "~/Music"
    if (label === "pictures") return "~/Pictures"
    if (label === "videos") return "~/Videos"
    if (label === "downloads") return "~/Downloads"
    if (label === "desktop") return "~/Desktop"
    if (label === "trash") return root.trashFilesPath
    return ""
}

function openSidebarContextPath(root, pathValue) {
    var p = String(pathValue || root.sidebarContextPath || "")
    if (p.length <= 0) {
        p = fallbackPathFromSidebarLabel(root)
    }
    if (p.length <= 0) {
        return
    }
    root.openPath(p)
}

function openSidebarContextPathInNewTab(root, pathValue) {
    var p = String(pathValue || root.sidebarContextPath || "")
    if (p.length <= 0) {
        p = fallbackPathFromSidebarLabel(root)
    }
    if (p.length <= 0) {
        return
    }
    root.openPathInNewTab(p)
}

function openSidebarContextPathInNewWindow(root, pathValue) {
    var p = String(pathValue || root.sidebarContextPath || "")
    if (p.length <= 0) {
        p = fallbackPathFromSidebarLabel(root)
    }
    if (p.length <= 0) {
        return
    }
    root.openInNewWindowRequested(p)
}

function sidebarContextCanOpenPath(root) {
    var p = String(root.sidebarContextPath || "")
    var rowType = String(root.sidebarContextRowType || "")
    if (rowType === "section" || p.length <= 0) {
        return false
    }
    if (!root.sidebarContextMounted) {
        return p.indexOf("__mount__:") === 0
    }
    if (p === "__recent__" || p === "__network__") {
        return false
    }
    return true
}

function sidebarContextCanOpenInNewWindow(root) {
    if (!sidebarContextCanOpenPath(root)) {
        return false
    }
    var p = String(root.sidebarContextPath || "")
    if (String(root.sidebarContextRowType || "") === "storage"
            && p.indexOf("__mount__:") === 0) {
        return false
    }
    return p.length > 0
}

function sidebarContextCanMount(root) {
    return String(root.sidebarContextRowType || "") === "storage"
            && !root.sidebarContextMounted
            && String(root.sidebarContextDevice || "").length > 0
}

function sidebarContextCanUnmount(root) {
    return String(root.sidebarContextRowType || "") === "storage"
            && root.sidebarContextMounted
            && String(root.sidebarContextDevice || "").length > 0
}

function sidebarContextMount(root, fileManagerApi) {
    if (!sidebarContextCanMount(root)) {
        return
    }
    if (!fileManagerApi || !fileManagerApi.startMountStorageDevice) {
        root.notifyResult("Open Drive", {
                              "ok": false,
                              "error": "mount-api-unavailable"
                          })
        return
    }
    root.pendingMountDevice = String(root.sidebarContextDevice || "")
    var res = fileManagerApi.startMountStorageDevice(root.pendingMountDevice)
    if (!res || !res.ok) {
        root.pendingMountDevice = ""
        root.notifyResult("Open Drive", res)
    }
}

function sidebarContextUnmount(root, fileManagerApi) {
    if (!sidebarContextCanUnmount(root)) {
        return
    }
    if (!fileManagerApi || !fileManagerApi.startUnmountStorageDevice) {
        root.notifyResult("Eject", {
                              "ok": false,
                              "error": "unmount-api-unavailable"
                          })
        return
    }
    var res = fileManagerApi.startUnmountStorageDevice(
                String(root.sidebarContextDevice || ""))
    if (!res || !res.ok) {
        root.notifyResult("Eject", res)
    }
}

function sidebarContextRevealInFileManager(root, fileManagerApi) {
    var p = String(root.sidebarContextPath || "")
    if (p.length <= 0 || p.indexOf("__mount__:") === 0) {
        return
    }
    if (!fileManagerApi || !fileManagerApi.startOpenPathInFileManager) {
        return
    }
    var reqId = root.createAsyncRequestId()
    var res = fileManagerApi.startOpenPathInFileManager(p, reqId)
    if (res && res.ok) {
        root.trackOpenAction(reqId, "Open In File Manager", false)
    } else {
        root.notifyResult("Open In File Manager", res)
    }
}

function sidebarContextCopyPath(root, fileManagerApi) {
    var p = String(root.sidebarContextPath || "")
    if (p.length <= 0 || p.indexOf("__mount__:") === 0) {
        return
    }
    if (fileManagerApi && fileManagerApi.copyTextToClipboard) {
        fileManagerApi.copyTextToClipboard(p)
    }
}

function storageUsageRatio(bytesAvailableValue, bytesTotalValue) {
    var total = Number(bytesTotalValue || -1)
    var available = Number(bytesAvailableValue || -1)
    if (!(total > 0) || available < 0) {
        return -1
    }
    var used = total - available
    if (used < 0) {
        used = 0
    }
    if (used > total) {
        used = total
    }
    return used / total
}

function formatStorageBytes(bytesValue) {
    var n = Number(bytesValue || 0)
    if (!(n >= 0)) {
        return ""
    }
    var units = ["B", "KB", "MB", "GB", "TB", "PB"]
    var u = 0
    while (n >= 1024 && u < units.length - 1) {
        n = n / 1024.0
        u += 1
    }
    var digits = n >= 100 ? 0 : (n >= 10 ? 1 : 2)
    return String(n.toFixed(digits)) + " " + units[u]
}

function storageCapacityText(bytesAvailableValue, bytesTotalValue) {
    var total = Number(bytesTotalValue || -1)
    var available = Number(bytesAvailableValue || -1)
    if (!(total > 0) || !(available >= 0)) {
        return ""
    }
    var freeText = formatStorageBytes(available)
    var totalText = formatStorageBytes(total)
    if (freeText.length <= 0 || totalText.length <= 0) {
        return ""
    }
    return freeText + " free of " + totalText
}

function storageUsageColor(theme, ratioValue) {
    var ratio = Number(ratioValue || 0)
    if (ratio >= 0.8) {
        return theme.color("storageUsageCritical")
    }
    return theme.color("storageUsageInfo")
}

function storageTrackColor(theme, mountedValue) {
    if (!mountedValue) {
        return theme.color("storageTrackUnmounted")
    }
    return theme.color("storageTrackMounted")
}

function toggleStorageMount(root, fileManagerApi, rowPath, rowDevice, mounted) {
    var pathValue = String(rowPath || "")
    var deviceValue = String(rowDevice || "")
    var isMounted = !!mounted
    if (isMounted) {
        if (deviceValue.length <= 0 && pathValue.length > 0) {
            deviceValue = pathValue
        }
        if (deviceValue.length <= 0 || !fileManagerApi
                || !fileManagerApi.startUnmountStorageDevice) {
            return
        }
        var unmountRes = fileManagerApi.startUnmountStorageDevice(deviceValue)
        if (!unmountRes || !unmountRes.ok) {
            root.notifyResult("Eject", unmountRes)
        }
        return
    }

    if (deviceValue.length <= 0 && pathValue.indexOf("__mount__:") === 0) {
        deviceValue = decodeURIComponent(pathValue.slice(10))
    }
    if (deviceValue.length <= 0 || !fileManagerApi
            || !fileManagerApi.startMountStorageDevice) {
        return
    }
    root.pendingMountDevice = deviceValue
    var mountRes = fileManagerApi.startMountStorageDevice(deviceValue)
    if (!mountRes || !mountRes.ok) {
        root.pendingMountDevice = ""
        root.notifyResult("Open Drive", mountRes)
    }
}
