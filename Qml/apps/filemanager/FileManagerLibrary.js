.pragma library
.import "FileManagerUtils.js" as FileManagerUtils

function refreshLibraryData(root, fileManagerApi, recentModel, favoriteModel) {
    if (!fileManagerApi) {
        recentModel.clear()
        favoriteModel.clear()
        return
    }

    recentModel.clear()
    if (fileManagerApi.recentFiles) {
        var recents = fileManagerApi.recentFiles(12)
        for (var i = 0; i < recents.length; ++i) {
            var r = recents[i]
            recentModel.append({
                "path": String(r.path || ""),
                "name": FileManagerUtils.basename(String(r.path || "")),
                "lastOpened": String(r.lastOpened || "")
            })
        }
    }

    favoriteModel.clear()
    if (fileManagerApi.favoriteFiles) {
        var favorites = fileManagerApi.favoriteFiles(24)
        for (var j = 0; j < favorites.length; ++j) {
            var f = favorites[j]
            favoriteModel.append({
                "path": String(f.path || ""),
                "name": FileManagerUtils.basename(String(f.path || "")),
                "thumbnailPath": String(f.thumbnailPath || "")
            })
        }
    }
}

function syncSelectedFavoriteState(root, fileManagerApi) {
    var item = root.selectedEntryInfo ? root.selectedEntryInfo() : {}
    if (!item || !item.ok || !item.path || !fileManagerApi || !fileManagerApi.isFavoriteFile) {
        root.selectedFavorite = false
        return
    }
    root.selectedFavorite = !!fileManagerApi.isFavoriteFile(String(item.path))
}

function setSelectedFavorite(root, fileManagerApi, favorite) {
    var item = root.selectedEntryInfo ? root.selectedEntryInfo() : {}
    if (!item || !item.ok || !item.path || !fileManagerApi || !fileManagerApi.setFavoriteFile) {
        return
    }

    var p = String(item.path || "")
    var thumb = (!item.isDir && FileManagerUtils.isImageName(FileManagerUtils.basename(p))) ? p : ""
    var result = fileManagerApi.setFavoriteFile(p, !!favorite, thumb)
    if (root.notifyResult) {
        root.notifyResult(favorite ? "Add Favorite" : "Remove Favorite", result)
    }
    if (result && result.ok) {
        root.selectedFavorite = !!favorite
        if (root.refreshLibraryData) {
            root.refreshLibraryData()
        }
    }
}

function openLibraryPath(root, fileModel, fileManagerApi, appCommandRouter, pathValue) {
    var p = String(pathValue || "")
    if (!p || !fileModel) {
        return
    }

    var isDirHint = (root && root.resolveLibraryEntryIsDir) ? !!root.resolveLibraryEntryIsDir(p) : false
    if (isDirHint) {
        if (root.openPath) {
            root.openPath(p)
        }
        return
    }

    if (appCommandRouter && appCommandRouter.routeWithResult) {
        var routed = appCommandRouter.routeWithResult("filemanager.open",
                                                     { "target": p },
                                                     "file-manager-library")
        if (root && root.notifyResult && (!routed || !routed.ok)) {
            root.notifyResult("Open", routed)
        }
        if (routed && routed.ok) {
            return
        }
    }

    if (fileManagerApi) {
        if (fileManagerApi.startOpenPath) {
            var reqId = (root && root.createAsyncRequestId)
                    ? root.createAsyncRequestId()
                    : (String(Date.now()) + "-" + String(Math.floor(Math.random() * 1000000)))
            var openAsyncRes = fileManagerApi.startOpenPath(p, reqId)
            if (root && root.notifyResult && (!openAsyncRes || !openAsyncRes.ok)) {
                root.notifyResult("Open", openAsyncRes)
                return
            }
            if (root && root.trackOpenAction) {
                root.trackOpenAction(reqId, "Open", false)
            }
            return
        }
    }

    if (appCommandRouter && appCommandRouter.route) {
        appCommandRouter.route("filemanager.open",
                               { "target": p },
                               "file-manager")
    }
}
