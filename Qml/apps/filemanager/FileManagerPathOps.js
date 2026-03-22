.pragma library

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
            root.notifyResult("Mount Storage", {
                                  "ok": false,
                                  "error": "mount-api-unavailable"
                              })
            return
        }
        root.pendingMountDevice = device
        var mountRes = fileManagerApi.startMountStorageDevice(device)
        if (!mountRes || !mountRes.ok) {
            root.pendingMountDevice = ""
            root.notifyResult("Mount Storage", mountRes)
        }
        return
    }
    if (p.length === 0) {
        p = "~"
    }
    if (p === "__network__") {
        p = "~"
    }
    if (p === "__trash__") {
        p = root.trashFilesPath
    }
    root.fileModel.currentPath = p
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
    if (root.tabModel) {
        root.tabModel.clear()
        root.tabModel.append({
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
    root.connectServerTypeIndex = 4
    root.connectServerHost = ""
    root.connectServerPort = 445
    root.connectServerFolder = "/"
    root.connectServerError = ""
    root.connectServerBusy = false
    if (connectServerDialog && connectServerDialog.openAndFocus) {
        connectServerDialog.openAndFocus()
    } else if (connectServerDialog && connectServerDialog.open) {
        connectServerDialog.open()
    }
}

function submitConnectServer(root, fileManagerApi) {
    var typeRow = root.connectServerTypes[Math.max(0, root.connectServerTypeIndex)]
            || root.connectServerTypes[0]
    var scheme = String(typeRow.scheme || "smb").trim()
    var host = String(root.connectServerHost || "").trim()
    var folder = String(root.connectServerFolder || "").trim()
    var port = Number(root.connectServerPort || 0)

    if (host.length <= 0) {
        root.connectServerError = "Server name or IP is required."
        return
    }
    if (!/^[A-Za-z0-9._:-]+$/.test(host)) {
        root.connectServerError = "Invalid server name or IP."
        return
    }
    if (!(port > 0 && port <= 65535)) {
        root.connectServerError = "Port must be between 1 and 65535."
        return
    }
    if (folder.length <= 0) {
        folder = "/"
    }
    if (folder.charAt(0) !== "/") {
        folder = "/" + folder
    }
    var defaultPort = Number(typeRow.port || 0)
    var authority = host
    if (!(defaultPort > 0 && port === defaultPort)) {
        authority = host + ":" + String(port)
    }
    var uri = scheme + "://" + authority + folder
    root.connectServerError = ""
    if (!fileManagerApi || !fileManagerApi.startConnectServer) {
        root.notifyResult("Connect Server", {
                              "ok": false,
                              "error": "connect-server-api-unavailable"
                          })
        return
    }
    root.connectServerBusy = true
    root.pendingConnectServerUri = uri
    var res = fileManagerApi.startConnectServer(uri)
    if (!res || !res.ok) {
        root.connectServerBusy = false
        root.pendingConnectServerUri = ""
        root.notifyResult("Connect Server", res)
    }
}
