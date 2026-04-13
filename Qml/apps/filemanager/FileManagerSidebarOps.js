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
                            "bytesAvailable": -1,
                            "depth": 0
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
                            "bytesAvailable": -1,
                            "depth": 0
                        })
}

function loadStorageSidebarItems(root, sidebarModel, fileManagerApi, rowsOverride) {
    if (!fileManagerApi || !fileManagerApi.storageLocations) {
        root.storageScanInProgress = true
        sidebarModel.append({
                                "rowType": "storage-status",
                                "label": "Memindai drive...",
                                "path": "",
                                "iconName": "drive-harddisk-symbolic",
                                "device": "",
                                "mounted": true,
                                "browsable": false,
                                "bytesTotal": -1,
                                "bytesAvailable": -1,
                                "depth": 0
                            })
        return
    }
    root.storageScanInProgress = true
    var rows = rowsOverride
    if (!rows || typeof rows.length === "undefined") {
        rows = fileManagerApi.storageLocations()
    }
    if (!rows || typeof rows.length === "undefined") {
        root.storageScanInProgress = true
        sidebarModel.append({
                                "rowType": "storage-status",
                                "label": "Memindai drive...",
                                "path": "",
                                "iconName": "drive-harddisk-symbolic",
                                "device": "",
                                "mounted": true,
                                "browsable": false,
                                "bytesTotal": -1,
                                "bytesAvailable": -1,
                                "depth": 0
                            })
        return
    }
    root.storageSnapshotReady = true
    root.storageScanInProgress = false
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
                "groupKey": String(row.deviceGroupKey || key),
                "groupLabel": String(row.deviceGroupLabel || label || "Drive"),
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
            if (String(prev.groupLabel || "").length <= 0) {
                prev.groupLabel = String(row.deviceGroupLabel || label || "Drive")
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

    if (entries.length <= 0) {
        root.storageScanInProgress = false
        sidebarModel.append({
                                "rowType": "storage-status",
                                "label": root.storageScanInProgress
                                         ? "Memindai drive..."
                                         : "Tidak ada drive terdeteksi",
                                "path": "",
                                "iconName": "drive-harddisk-symbolic",
                                "device": "",
                                "mounted": true,
                                "browsable": false,
                                "bytesTotal": -1,
                                "bytesAvailable": -1,
                                "depth": 0
                            })
        return
    }

    var groupMap = ({})
    var groupOrder = []
    for (var n = 0; n < entries.length; ++n) {
        var e = entries[n]
        var gk = String(e.groupKey || e.key || "")
        if (gk.length <= 0) {
            gk = "__group__" + String(n)
        }
        if (!groupMap[gk]) {
            groupMap[gk] = {
                "label": String(e.groupLabel || e.label || "Drive"),
                "entries": []
            }
            groupOrder.push(gk)
        }
        groupMap[gk].entries.push(e)
    }

    for (var gi = 0; gi < groupOrder.length; ++gi) {
        var groupKey = groupOrder[gi]
        var group = groupMap[groupKey]
        var groupEntries = group.entries || []
        if (groupEntries.length <= 0) {
            continue
        }

        var showGroupHeader = groupEntries.length > 1
        if (showGroupHeader) {
            sidebarModel.append({
                                    "rowType": "storage-group",
                                    "label": String(group.label || "Drive"),
                                    "path": "",
                                    "iconName": "drive-harddisk-symbolic",
                                    "device": "",
                                    "mounted": true,
                                    "browsable": false,
                                    "bytesTotal": -1,
                                    "bytesAvailable": -1,
                                    "depth": 0
                                })
        }

        for (var si = 0; si < groupEntries.length; ++si) {
            var entry = groupEntries[si]
            var rowPath = ""
            var rowDevice = String(entry.device || "")
            var rowMounted = !!entry.mounted
            if (rowMounted) {
                rowPath = String(entry.path || "")
                if (rowPath.length <= 0) {
                    continue
                }
            } else {
                if (rowDevice.length <= 0) {
                    continue
                }
                rowPath = "__mount__:" + encodeURIComponent(rowDevice)
            }

            sidebarModel.append({
                                    "rowType": "storage",
                                    "label": String(entry.label || "Drive"),
                                    "path": rowPath,
                                    "iconName": String(
                                                    entry.iconName
                                                    || "drive-harddisk-symbolic"),
                                    "device": rowDevice,
                                    "mounted": rowMounted,
                                    "browsable": rowMounted,
                                    "bytesTotal": Number(
                                                      entry.bytesTotal
                                                      !== undefined ? entry.bytesTotal : -1),
                                    "bytesAvailable": Number(
                                                          entry.bytesAvailable
                                                          !== undefined ? entry.bytesAvailable : -1),
                                    "depth": showGroupHeader ? 1 : 0
                                })
        }
    }
    root.storageScanInProgress = false
}

function rebuildSidebarItems(root, sidebarModel, fileManagerApi, rowsOverride) {
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
    appendSidebarSection(sidebarModel, "Devices")
    loadStorageSidebarItems(root, sidebarModel, fileManagerApi, rowsOverride)
    appendSidebarSection(sidebarModel, "Network")
    appendSidebarItem(sidebarModel, "Entire Network", "__network__",
                      "network-workgroup-symbolic")
}
