.pragma library

function contextOpenWithEntry(root, indexValue) {
    if (!root.contextOpenWithApps || indexValue < 0
            || indexValue >= root.contextOpenWithApps.length) {
        return ({})
    }
    return root.contextOpenWithApps[indexValue] || ({})
}

function contextDefaultOpenWithEntry(root) {
    var apps = root.contextOpenWithAllApps || []
    for (var i = 0; i < apps.length; ++i) {
        var row = apps[i] || ({})
        if (!!row.defaultApp) {
            return row
        }
    }
    if (apps.length > 0) {
        return apps[0] || ({})
    }
    return ({})
}

function contextRecommendedOpenWithEntry(root, indexValue) {
    var out = contextRecommendedOpenWithEntries(root)
    var idx = Number(indexValue || 0)
    if (idx < 0 || idx >= out.length) {
        return ({})
    }
    return out[idx] || ({})
}

function contextRecommendedOpenWithEntries(root) {
    var apps = root.contextOpenWithAllApps || []
    var def = contextDefaultOpenWithEntry(root)
    var defId = String((def && def.id) ? def.id : "")
    var out = []
    var seen = ({})
    for (var i = 0; i < apps.length; ++i) {
        var row = apps[i] || ({})
        var id = String((row && row.id) ? row.id : "")
        if (id.length <= 0 || id === defId || seen[id] === true) {
            continue
        }
        seen[id] = true
        out.push(row)
        if (out.length >= 3) {
            break
        }
    }
    return out
}

function contextRecommendedOpenWithCount(root) {
    return contextRecommendedOpenWithEntries(root).length
}

function openWithSortScore(row) {
    var item = row || ({})
    if (!!item.defaultApp) {
        return 0
    }
    if (!!item.pinned) {
        return 1
    }
    return 2
}

function sortOpenWithRows(rows) {
    var src = rows || []
    var list = []
    for (var i = 0; i < src.length; ++i) {
        list.push(src[i] || ({}))
    }
    list.sort(function (a, b) {
        var sa = openWithSortScore(a)
        var sb = openWithSortScore(b)
        if (sa !== sb) {
            return sa - sb
        }
        var an = String(
                    (a && a.name) ? a.name : ((a && a.id) ? a.id : "")
                    ).toLowerCase()
        var bn = String(
                    (b && b.name) ? b.name : ((b && b.id) ? b.id : "")
                    ).toLowerCase()
        if (an < bn) {
            return -1
        }
        if (an > bn) {
            return 1
        }
        return 0
    })
    return list
}

function openWithSecondaryTag(row) {
    var item = row || ({})
    if (!!item.defaultApp) {
        return "Default"
    }
    if (!!item.pinned) {
        return "Pinned"
    }
    return "Recommended"
}

function filteredOpenWithCount(root) {
    var rows = root.contextOpenWithAllApps || []
    var q = String(root.openWithSearchText || "").trim().toLowerCase()
    var count = 0
    for (var i = 0; i < rows.length; ++i) {
        var row = rows[i] || ({})
        var appId = String((row && row.id) ? row.id : "")
        var appName = String((row && row.name) ? row.name : appId)
        if (q.length <= 0 || appName.toLowerCase().indexOf(q) >= 0
                || appId.toLowerCase().indexOf(q) >= 0) {
            count += 1
        }
    }
    return count
}

function refreshContextOpenWithApps(root, fileManagerApi) {
    root.contextOpenWithApps = []
    root.contextOpenWithAllApps = []
    if (root.contextEntryIndex < 0) {
        return
    }
    if (!fileManagerApi || !fileManagerApi.startOpenWithApplications) {
        return
    }
    var p = String(root.contextEntryPath || "")
    if (p.length === 0) {
        return
    }
    root.contextOpenWithPath = p
    var reqId = String(Date.now()) + "-" + String(Math.floor(Math.random() * 1000000))
    root.contextOpenWithRequestId = reqId
    fileManagerApi.startOpenWithApplications(p, 256, reqId)
}

function refreshPropertiesOpenWithApps(root, fileManagerApi, pathValue) {
    root.propertiesOpenWithApps = []
    root.propertiesOpenWithCurrentIndex = -1
    root.propertiesOpenWithRequestId = ""
    root.propertiesOpenWithPath = ""
    var p = String(pathValue || "")
    if (p.length <= 0) {
        return
    }
    if (!fileManagerApi || !fileManagerApi.startOpenWithApplications) {
        return
    }
    root.propertiesOpenWithPath = p
    var reqId = String(Date.now()) + "-" + String(Math.floor(Math.random() * 1000000))
    root.propertiesOpenWithRequestId = reqId
    fileManagerApi.startOpenWithApplications(p, 256, reqId)
}

function applyPropertiesOpenWithSelection(root, fileManagerApi, indexValue) {
    var idx = Number(indexValue)
    var apps = root.propertiesOpenWithApps || []
    if (!(idx >= 0) || idx >= apps.length) {
        return
    }
    var row = apps[idx] || ({})
    var appId = String((row && row.id) ? row.id : "")
    var p = String((root.propertiesEntry && root.propertiesEntry.path)
                   ? root.propertiesEntry.path : "")
    if (appId.length <= 0 || p.length <= 0) {
        return
    }
    root.propertiesOpenWithCurrentIndex = idx
    root.propertiesOpenWithName = String((row && row.name) ? row.name : appId)
    if (!fileManagerApi || !fileManagerApi.startSetDefaultOpenWithApp) {
        return
    }
    var reqId = root.createAsyncRequestId()
    var res = fileManagerApi.startSetDefaultOpenWithApp(p, appId, reqId)
    if (!res || !res.ok) {
        root.notifyResult("Set Default Application", res)
        return
    }
    root.trackOpenAction(reqId, "Set Default Application", true)
}

function openContextEntryInApp(root, fileManagerApi, appIdValue) {
    var appId = String(appIdValue || "")
    var p = String(root.contextEntryPath || "")
    if (appId.length === 0 || p.length === 0) {
        return
    }
    if (!fileManagerApi || !fileManagerApi.startOpenPathWithApp) {
        return
    }
    var reqId = root.createAsyncRequestId()
    var res = fileManagerApi.startOpenPathWithApp(p, appId, reqId)
    if (!res || !res.ok) {
        root.notifyResult(root.contextEntryIsDir ? "Open In" : "Open With", res)
        return
    }
    root.trackOpenAction(reqId, root.contextEntryIsDir ? "Open In" : "Open With", false)
}

function setDefaultContextEntryApp(root, fileManagerApi, appIdValue) {
    var appId = String(appIdValue || "")
    var p = String(root.contextEntryPath || "")
    if (appId.length === 0 || p.length === 0) {
        return
    }
    if (!fileManagerApi || !fileManagerApi.startSetDefaultOpenWithApp) {
        return
    }
    var reqId = root.createAsyncRequestId()
    var res = fileManagerApi.startSetDefaultOpenWithApp(p, appId, reqId)
    if (!res || !res.ok) {
        root.notifyResult("Set Default Application", res)
        return
    }
    root.trackOpenAction(reqId, "Set Default Application", true)
}

function openWithOtherApplication(root, openWithDialog) {
    if (root.contextEntryIndex < 0 || root.contextEntryIsDir) {
        return
    }
    root.openWithDialogMode = "open"
    root.openWithSearchText = ""
    openWithDialog.open()
}

function setDefaultWithOtherApplication(root, openWithDialog) {
    if (root.contextEntryIndex < 0 || root.contextEntryIsDir) {
        return
    }
    root.openWithDialogMode = "setdefault"
    root.openWithSearchText = ""
    openWithDialog.open()
}

function openInOtherApplication(root, openWithDialog) {
    if (root.contextEntryIndex < 0) {
        return
    }
    root.openWithDialogMode = "openin"
    root.openWithSearchText = ""
    openWithDialog.open()
}
