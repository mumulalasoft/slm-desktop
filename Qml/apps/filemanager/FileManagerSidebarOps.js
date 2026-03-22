.pragma library

function appendSidebarSection(sidebarModel, label) {
    sidebarModel.append({
                            "rowType": "section",
                            "label": String(label || ""),
                            "path": "",
                            "iconName": "",
                            "device": "",
                            "mounted": true,
                            "browsable": false,
                            "bytesTotal": -1,
                            "bytesAvailable": -1
                        })
}

function storageOrderForKey(root, keyValue) {
    var k = String(keyValue || "")
    if (k.length <= 0) {
        return 999999
    }
    if (k === "/") {
        return 0
    }
    var map = root.storageOrderMap || ({})
    if (map[k] === undefined) {
        map[k] = root.storageOrderNext
        root.storageOrderNext = root.storageOrderNext + 1
        root.storageOrderMap = map
    }
    return Number(map[k])
}

function storageKeyForRow(row) {
    if (!row) {
        return ""
    }
    var p = String(row.path || row.rootPath || "")
    var device = String(row.device || "")
    var label = String(row.label || row.name || "")
    if (p === "/") {
        return "__rootfs__"
    }
    if (device.length > 0) {
        if (device.indexOf("/dev/") === 0) {
            var di = device.lastIndexOf("/")
            return "__dev__" + (di >= 0 ? device.slice(di + 1) : device)
        }
        return "__dev__" + device
    }
    if (p.length > 0) {
        return "__path__" + p
    }
    if (label.length > 0) {
        return "__label__" + label.toLowerCase()
    }
    return ""
}

function appendSidebarItem(sidebarModel, label, path, iconName) {
    sidebarModel.append({
                            "rowType": "item",
                            "label": String(label || ""),
                            "path": String(path || ""),
                            "iconName": String(iconName || "folder-symbolic"),
                            "device": "",
                            "mounted": true,
                            "browsable": true,
                            "bytesTotal": -1,
                            "bytesAvailable": -1
                        })
}

function loadStorageSidebarItems(root, sidebarModel, fileManagerApi) {
    if (!fileManagerApi || !fileManagerApi.storageLocations) {
        return
    }
    var rows = fileManagerApi.storageLocations()
    if (!rows || typeof rows.length === "undefined") {
        return
    }
    var byKey = ({})
    for (var i = 0; i < rows.length; ++i) {
        var row = rows[i]
        var device = String(row.device || "")
        var p = String(row.path || row.rootPath || "")
        var mounted = !!row.mounted || (p.length > 0 && p.indexOf(
                                            "__mount__:") !== 0)
        var fsType = String(row.filesystemType || "").toLowerCase()
        if (p === "/run" || p === "/run/lock" || p === "/boot/efi" || p.indexOf(
                    "/run/user/") === 0 || p.indexOf("/proc") === 0 || p.indexOf(
                    "/sys") === 0 || p.indexOf("/dev") === 0 || fsType === "tmpfs"
                || fsType === "devtmpfs" || fsType === "overlay" || fsType
                === "squashfs" || fsType === "proc" || fsType === "sysfs"
                || fsType === "cgroup2" || fsType === "cgroup") {
            continue
        }
        var key = storageKeyForRow(row)
        if (key.length <= 0) {
            continue
        }
        var iconName = String(row.icon || "drive-harddisk-symbolic")
        var label = String(row.label || row.name || root.basename(p) || "Storage")
        if (p === "/") {
            label = "File System"
        }
        var prev = byKey[key]
        if (!prev) {
            byKey[key] = {
                "key": key,
                "label": label,
                "path": p,
                "iconName": iconName,
                "device": device,
                "mounted": mounted,
                "bytesTotal": Number(
                                  row.bytesTotal !== undefined ? row.bytesTotal : -1),
                "bytesAvailable": Number(
                                      row.bytesAvailable !== undefined ? row.bytesAvailable : (row.bytesFree !== undefined ? row.bytesFree : -1))
            }
        } else {
            if (mounted && !prev.mounted) {
                prev.label = label
                prev.path = p
                prev.iconName = iconName
                prev.device = device
                prev.mounted = true
            }
            var totalNow = Number(
                        row.bytesTotal !== undefined ? row.bytesTotal : -1)
            var availableNow = Number(
                        row.bytesAvailable !== undefined ? row.bytesAvailable : (row.bytesFree !== undefined ? row.bytesFree : -1))
            if (totalNow > 0) {
                prev.bytesTotal = totalNow
            }
            if (availableNow >= 0) {
                prev.bytesAvailable = availableNow
            }
        }
    }

    var entries = []
    var keys = Object.keys(byKey)
    for (var k = 0; k < keys.length; ++k) {
        entries.push(byKey[keys[k]])
    }

    var byLabel = ({})
    for (var m = 0; m < entries.length; ++m) {
        var rowM = entries[m]
        var labelKey = String(rowM.label || "").trim().toLowerCase()
        if (labelKey.length <= 0) {
            continue
        }
        var prevM = byLabel[labelKey]
        if (!prevM || (!!rowM.mounted && !prevM.mounted)) {
            byLabel[labelKey] = rowM
        }
    }
    var dedupEntries = []
    var seenEntry = ({})
    for (var n0 = 0; n0 < entries.length; ++n0) {
        var rowE = entries[n0]
        var lk = String(rowE.label || "").trim().toLowerCase()
        if (lk.length > 0 && byLabel[lk] && byLabel[lk] !== rowE
                && !rowE.mounted) {
            continue
        }
        var sig = String(rowE.key || "") + "|" + String(
                    rowE.path || "") + "|" + String(rowE.device || "")
        if (seenEntry[sig]) {
            continue
        }
        seenEntry[sig] = true
        dedupEntries.push(rowE)
    }
    entries = dedupEntries
    entries.sort(function (a, b) {
        var oa = storageOrderForKey(root, String(a.key || ""))
        var ob = storageOrderForKey(root, String(b.key || ""))
        if (oa !== ob) {
            return oa - ob
        }
        var la = String(a.label || "").toLowerCase()
        var lb = String(b.label || "").toLowerCase()
        if (la < lb)
            return -1
        if (la > lb)
            return 1
        return 0
    })

    for (var n = 0; n < entries.length; ++n) {
        var e = entries[n]
        if (!!e.mounted) {
            if (String(e.path || "").length <= 0) {
                continue
            }
            sidebarModel.append({
                                    "rowType": "storage",
                                    "label": String(e.label || "Storage"),
                                    "path": String(e.path || ""),
                                    "iconName": String(
                                                    e.iconName
                                                    || "drive-harddisk-symbolic"),
                                    "device": String(e.device || ""),
                                    "mounted": true,
                                    "browsable": true,
                                    "bytesTotal": Number(
                                                      e.bytesTotal
                                                      !== undefined ? e.bytesTotal : -1),
                                    "bytesAvailable": Number(
                                                          e.bytesAvailable
                                                          !== undefined ? e.bytesAvailable : -1)
                                })
        } else {
            var dev = String(e.device || "")
            if (dev.length <= 0) {
                continue
            }
            sidebarModel.append({
                                    "rowType": "storage",
                                    "label": String(e.label || "Storage"),
                                    "path": "__mount__:" + encodeURIComponent(
                                                dev),
                                    "iconName": String(
                                                    e.iconName
                                                    || "drive-harddisk-symbolic"),
                                    "device": dev,
                                    "mounted": false,
                                    "browsable": false,
                                    "bytesTotal": Number(
                                                      e.bytesTotal
                                                      !== undefined ? e.bytesTotal : -1),
                                    "bytesAvailable": Number(
                                                          e.bytesAvailable
                                                          !== undefined ? e.bytesAvailable : -1)
                                })
        }
    }
}

function rebuildSidebarItems(root, sidebarModel, fileManagerApi) {
    sidebarModel.clear()
    appendSidebarSection(sidebarModel, "Bookmarks")
    appendSidebarItem(sidebarModel, "Home", "~", "go-home-symbolic")
    appendSidebarItem(sidebarModel, "Recent", "__recent__",
                      "document-open-recent-symbolic")
    appendSidebarItem(sidebarModel, "Documents", "~/Documents", "text-x-generic-symbolic")
    appendSidebarItem(sidebarModel, "Music", "~/Music", "audio-x-generic-symbolic")
    appendSidebarItem(sidebarModel, "Pictures", "~/Pictures", "image-x-generic-symbolic")
    appendSidebarItem(sidebarModel, "Videos", "~/Videos", "video-x-generic-symbolic")
    appendSidebarItem(sidebarModel, "Downloads", "~/Downloads",
                      "folder-download-symbolic")
    appendSidebarItem(sidebarModel, "Desktop", "~/Desktop", "user-desktop-symbolic")
    appendSidebarItem(sidebarModel, "Trash", "~/.local/share/Trash/files",
                      "user-trash-symbolic")
    appendSidebarSection(sidebarModel, "Storage")
    loadStorageSidebarItems(root, sidebarModel, fileManagerApi)
    appendSidebarSection(sidebarModel, "Network")
    appendSidebarItem(sidebarModel, "Entire Network", "__network__",
                      "network-workgroup-symbolic")
}
