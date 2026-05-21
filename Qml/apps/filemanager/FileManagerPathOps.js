.pragma library

function listModel(root) {
    return root.tabModel || root.tabModelRef || null
}

function openPath(root, fileManagerApi, pathValue) {
    if (!root.fileModel) {
        return
    }
    var p = String(pathValue || "")
    if (p.indexOf("__mount__:") === 0) {
        var encodedDevice = p.slice(10)
        var device = decodeURIComponent(encodedDevice || "")
        if (device.length <= 0) {
            return
        }
        if (!fileManagerApi || !fileManagerApi.startMountStorageDevice) {
            root.notifyResult("Open Drive", {
                                  "ok": false,
                                  "error": "mount-api-unavailable"
                              })
            return
        }
        root.pendingMountDevice = device
        var mountRes = fileManagerApi.startMountStorageDevice(device)
        if (!mountRes || !mountRes.ok) {
            root.pendingMountDevice = ""
            root.notifyResult("Open Drive", mountRes)
        }
        return
    }
    if (p.length === 0) {
        p = "~"
    }
    if (p === "__network__") {
        p = "~"
    }
    if (p === "__nearby__") {
        root.openNearbySendSheet(null)
        return
    }
    if (p === "__trash__") {
        p = root.trashFilesPath
    }
    console.log("[fm-path] openPath input=", String(pathValue || ""), "normalized=", p,
                "activeTab=", Number(root.activeTabIndex || 0))
    var beforePath = String(root.fileModel.currentPath || "")
    root.fileModel.currentPath = p
    var afterPath = String(root.fileModel.currentPath || "")
    if (beforePath === afterPath
            && root.fileModel.refresh) {
        root.fileModel.refresh()
    }
    root.selectedSidebarPath = p
    root.updateTabPath(root.activeTabIndex, p)
    root.clearSelection()
}

function initSingleTab(root, fileManagerModelFactory, pathValue) {
    var p = String(pathValue || "~")
    if (p.length === 0) {
        p = "~"
    }
    var prevOwned = root.ownedTabModels || []
    for (var i = 0; i < prevOwned.length; ++i) {
        var stale = prevOwned[i]
        if (stale && fileManagerModelFactory && fileManagerModelFactory.destroyModel) {
            fileManagerModelFactory.destroyModel(stale)
        }
    }
    root.ownedTabModels = []
    root.tabModelRefs = []
    var model = listModel(root)
    if (model) {
        model.clear()
        model.append({
                                 "path": p
                             })
    }
    root.tabModelRefs = [root.primaryFileModel]
    root.activeTabIndex = 0
    root.tabStateReady = true
    if (root.primaryFileModel && root.primaryFileModel.currentPath !== undefined) {
        root.primaryFileModel.currentPath = p
    }
    if (root.fileModel !== root.primaryFileModel) {
        root.fileModel = root.primaryFileModel
    }
    root.saveTabState()
}

function openConnectServerDialog(root, connectServerDialog) {
    if (connectServerDialog) {
        connectServerDialog.typeIndex = 4
        connectServerDialog.host = ""
        connectServerDialog.share = ""
        connectServerDialog.port = 445
        connectServerDialog.folder = "/"
        connectServerDialog.domain = "WORKGROUP"
        connectServerDialog.user = ""
        connectServerDialog.password = ""
        connectServerDialog.error = ""
        connectServerDialog.busy = false
        connectServerDialog.openConnectServer()
    }
}

function submitConnectServer(root, fileManagerApi) {
    var dialog = root.connectServerDialogRef
    if (!dialog) return

    var typeRow = dialog.types[Math.max(0, dialog.typeIndex)] || dialog.types[0]
    var scheme = String(typeRow.scheme || "smb").trim()
    var host = String(dialog.host || "").trim()
    var share = String(dialog.share || "").trim()
    var folder = String(dialog.folder || "").trim()
    var port = Number(dialog.port || 0)
    var domain = String(dialog.domain || "").trim()
    var user = String(dialog.user || "").trim()
    var password = String(dialog.password || "")

    if (host.length <= 0) {
        dialog.error = "Server name or IP is required."
        return
    }
    if (!/^[A-Za-z0-9._:-]+$/.test(host)) {
        dialog.error = "Invalid server name or IP."
        return
    }
    if (!(port > 0 && port <= 65535)) {
        dialog.error = "Port must be between 1 and 65535."
        return
    }
    if (folder.length <= 0) {
        folder = "/"
    }
    if (folder.charAt(0) !== "/") {
        folder = "/" + folder
    }
    if (scheme === "smb" && share.length > 0) {
        if (!/^[^\/\\:]+$/.test(share)) {
            dialog.error = "Share name cannot contain slashes or colons."
            return
        }
        folder = "/" + encodeURIComponent(share) + (folder === "/" ? "" : folder)
    }
    var defaultPort = Number(typeRow.port || 0)
    var authority = host
    if (user.length > 0) {
        var loginName = (scheme === "smb" && domain.length > 0) ? (domain + ";" + user) : user
        var userInfo = encodeURIComponent(loginName)
        if (password.length > 0) {
            userInfo += ":" + encodeURIComponent(password)
        }
        authority = userInfo + "@" + authority
    }
    if (!(defaultPort > 0 && port === defaultPort)) {
        authority = host + ":" + String(port)
        if (user.length > 0) {
            var prefixedLoginName = (scheme === "smb" && domain.length > 0) ? (domain + ";" + user) : user
            var prefixedUserInfo = encodeURIComponent(prefixedLoginName)
            if (password.length > 0) {
                prefixedUserInfo += ":" + encodeURIComponent(password)
            }
            authority = prefixedUserInfo + "@" + host + ":" + String(port)
        }
    }
    var uri = scheme + "://" + authority + folder
    dialog.error = ""
    if (!fileManagerApi || !fileManagerApi.startConnectServer) {
        root.notifyResult("Connect Server", {
                              "ok": false,
                              "error": "connect-server-api-unavailable"
                          })
        return
    }
    dialog.busy = true
    var res = fileManagerApi.startConnectServer(uri)
    if (!res || !res.ok) {
        dialog.busy = false
        root.notifyResult("Connect Server", res)
    }
}
