import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop
import "../../apps/filemanager/FileManagerSelection.js" as FileManagerSelection
import "../contextmenu" as ContextMenuComp

Item {
    id: root

    property var desktopViewController: null
    property var desktopMenuProvider: null
    property var shellApi: null

    readonly property var fileManagerApiRef: (typeof FileManagerApi !== "undefined") ? FileManagerApi : null
    readonly property var modelFactoryRef: (typeof FileManagerModelFactory !== "undefined") ? FileManagerModelFactory : null

    property var fileModel: null
    property string desktopPath: "~/Desktop"

    property string viewMode: "grid"
    property int selectedEntryIndex: -1
    property var selectedEntryIndexes: []
    property int selectionAnchorIndex: -1
    property var clipboardPaths: []
    property bool clipboardCut: false
    property bool showFileExtensions: true
    property int iconSizeMode: 1 // 0=small, 1=medium, 2=large
    property int gridSpacingMode: 1 // 0=tight, 1=normal, 2=wide
    property bool contentLoading: false
    property var desktopEntriesCache: []
    property var thumbnailRequestedPaths: ({})
    property var thumbnailRetryAfterByPath: ({})
    property int desktopThumbnailSize: 192
    property string toolbarSearchText: ""
    property bool trashView: false
    property var contextEntryRow: null
    property string contextEntryPath: ""
    property real dockTopY: -1
    property bool dndActive: false
    property bool dndCopyMode: false
    property int dndSourceIndex: -1
    property string dndSourcePath: ""
    property string dndSourceName: ""
    property string dndSourceIconName: ""
    property string dndSourceIconSource: ""
    property int dndHoverIndex: -1
    property real dndLastX: -1
    property real dndLastY: -1
    property bool syncRepairRunning: false
    property int syncRepairAttempts: 0
    property string pendingRefreshReason: ""
    property bool enforcingDesktopPath: false
    property var desktopViewRef: null
    property double suppressDesktopBackgroundMenuUntilMs: 0
    property double suppressContextMenuDismissUntilMs: 0
    property string activeContextMenuKind: ""
    readonly property real desktopShellBottomMargin: {
        if (dockTopY <= 0 || dockTopY >= height) {
            return 20
        }
        return Math.max(20, (height - dockTopY) + 8)
    }

    signal selectionChanged(var selection)
    signal directoryChanged(string path)

    function _parentDir(pathValue) {
        var p = String(pathValue || "")
        var idx = p.lastIndexOf("/")
        if (idx <= 0) {
            return "~"
        }
        return p.slice(0, idx)
    }

    function _selectionRows() {
        var out = []
        if (!fileModel || !fileModel.entryAt) {
            return out
        }
        var indexes = selectedEntryIndexes ? selectedEntryIndexes : []
        for (var i = 0; i < indexes.length; ++i) {
            var idx = Number(indexes[i])
            if (idx < 0) {
                continue
            }
            var row = fileModel.entryAt(idx)
            if (row && row.ok) {
                out.push(row)
            }
        }
        return out
    }

    function _modelPathSet() {
        var out = ({})
        if (!fileModel || !fileModel.entryAt) {
            return out
        }
        var count = Number(fileModel.count || 0)
        for (var i = 0; i < count; ++i) {
            var row = fileModel.entryAt(i)
            if (!row || !row.ok) {
                continue
            }
            if (root._isIgnoredDesktopMetaPath(String(row.path || ""))) {
                continue
            }
            var p = root._normalizedPath(String(row.path || ""))
            if (p.length > 0) {
                out[p] = true
            }
        }
        return out
    }

    function _apiPathSet() {
        var out = ({})
        if (!fileManagerApiRef || !fileManagerApiRef.listDirectory) {
            return out
        }
        var res = fileManagerApiRef.listDirectory(String(desktopPath || ""), false, true)
        if (!res || !res.ok || !res.entries) {
            return out
        }
        var rows = res.entries || []
        for (var i = 0; i < rows.length; ++i) {
            var row = rows[i] || ({})
            if (root._isIgnoredDesktopMetaPath(String(row.path || ""))) {
                continue
            }
            var p = root._normalizedPath(String(row.path || ""))
            if (p.length > 0) {
                out[p] = true
            }
        }
        return out
    }

    function _isIgnoredDesktopMetaPath(pathValue) {
        var p = String(pathValue || "").trim().toLowerCase()
        if (p.length <= 0) {
            return false
        }
        return p.endsWith("/.desktop_shell_slot_map.json")
                || p.endsWith("/.desktop_shell_shortcut_order")
                || p === ".desktop_shell_slot_map.json"
                || p === ".desktop_shell_shortcut_order"
    }

    function _setsEqual(a, b) {
        for (var k in a) {
            if (!b[k]) {
                return false
            }
        }
        for (var k2 in b) {
            if (!a[k2]) {
                return false
            }
        }
        return true
    }

    function ensureDesktopSync(reason) {
        if (!fileModel || !fileModel.refresh || syncRepairRunning) {
            return
        }
        var modelSet = _modelPathSet()
        var apiSet = _apiPathSet()
        if (_setsEqual(modelSet, apiSet)) {
            syncRepairAttempts = 0
            return
        }
        var missing = []
        var excess = []
        for (var k in apiSet) {
            if (!modelSet[k]) {
                missing.push(k)
            }
        }
        for (var k2 in modelSet) {
            if (!apiSet[k2]) {
                excess.push(k2)
            }
        }
        console.warn("[desktop-sync] mismatch reason=" + String(reason || "")
                     + " missing=" + String(missing.length)
                     + " excess=" + String(excess.length))
        if (missing.length > 0) {
            console.warn("[desktop-sync] missing first: " + missing.slice(0, 8).join(" | "))
        }
        if (excess.length > 0) {
            console.warn("[desktop-sync] excess first: " + excess.slice(0, 8).join(" | "))
        }
        console.warn("[desktop-sync] state modelCount=" + String(Number(fileModel && fileModel.count ? fileModel.count : 0))
                     + " cacheCount=" + String(Number(desktopEntriesCache ? desktopEntriesCache.length : 0))
                     + " apiCount=" + String(Object.keys(apiSet).length)
                     + " " + _slotMapSummary())
        syncRepairRunning = true
        syncRepairAttempts = Number(syncRepairAttempts || 0) + 1
        if (fileModel.searchText !== undefined && String(fileModel.searchText || "").length > 0) {
            fileModel.searchText = ""
        }
        fileModel.currentPath = desktopPath
        fileModel.refresh()
        syncRepairRunning = false
        if (syncRepairAttempts <= 3) {
            syncRepairTimer.restart()
        } else {
            _notifyInfo("Desktop sync mismatch detected. Auto-refresh attempted.")
        }
    }

    function requestDesktopRefresh(reason) {
        pendingRefreshReason = String(reason || "")
        if (_contextMenuActive()) {
            refreshDebounceTimer.restart()
            return
        }
        refreshDebounceTimer.restart()
    }

    function _contextMenuActive() {
        var desktopMenuOpen = !!(desktopCtxMenu && desktopCtxMenu.menuVisible)
        var itemMenuOpen = !!(launcherShortcutMenu && launcherShortcutMenu.visible)
        return desktopMenuOpen || itemMenuOpen
    }

    function _indexForPath(pathValue) {
        var p = _normalizedPath(String(pathValue || ""))
        if (p.length <= 0 || !fileModel || !fileModel.entryAt) {
            return -1
        }
        var total = Number(fileModel.count || 0)
        for (var i = 0; i < total; ++i) {
            var row = fileModel.entryAt(i)
            if (!row || !row.ok) {
                continue
            }
            var rp = _normalizedPath(String(row.path || ""))
            if (rp === p) {
                return i
            }
        }
        return -1
    }

    function _ensureContextSelectionForMenu() {
        var p = String(contextEntryPath || "")
        if (p.length <= 0) {
            return false
        }
        var idx = _indexForPath(p)
        if (idx < 0) {
            return false
        }
        applySelectionRequest(idx, Qt.NoModifier)
        return true
    }

    function enforceDesktopPath(reason) {
        if (!fileModel || enforcingDesktopPath) {
            return
        }
        var currentNorm = _normalizedPath(fileModel.currentPath)
        var desktopNorm = _normalizedPath(desktopPath)
        if (currentNorm === desktopNorm) {
            return
        }
        enforcingDesktopPath = true
        if (fileModel.searchText !== undefined && String(fileModel.searchText || "").length > 0) {
            fileModel.searchText = ""
        }
        fileModel.currentPath = desktopPath
        if (fileModel.refresh) {
            fileModel.refresh()
        }
        enforcingDesktopPath = false
        console.warn("[desktop-sync] enforceDesktopPath reason=" + String(reason || "")
                     + " current=" + String(currentNorm) + " target=" + String(desktopNorm))
    }

    function _syncSelectionToController() {
        var rows = _selectionRows()
        if (desktopViewController && desktopViewController.setSelection) {
            desktopViewController.setSelection(selectedEntryIndexes, rows)
        }
        if (desktopMenuProvider) {
            desktopMenuProvider.selection = rows
            desktopMenuProvider.path = desktopPath
        }
        selectionChanged(rows)
    }

    function _syncItemsToController() {
        if (!fileModel || !fileModel.entryAt) {
            return
        }
        var rows = []
        var count = Number(fileModel.count || 0)
        for (var i = 0; i < count; ++i) {
            var row = fileModel.entryAt(i)
            if (row && row.ok) {
                rows.push(row)
            }
        }
        if (fileManagerApiRef && fileManagerApiRef.listDirectory) {
            var seen = ({})
            for (var s = 0; s < rows.length; ++s) {
                var rp = _normalizedPath(String(rows[s].path || ""))
                if (rp.length > 0) {
                    seen[rp] = true
                }
            }
            var apiRes = fileManagerApiRef.listDirectory(String(desktopPath || ""), false, true)
            if (apiRes && apiRes.ok && apiRes.entries) {
                var apiRows = apiRes.entries || []
                for (var a = 0; a < apiRows.length; ++a) {
                    var raw = apiRows[a] || ({})
                    var p = _normalizedPath(String(raw.path || ""))
                    if (p.length <= 0 || _isIgnoredDesktopMetaPath(p) || seen[p]) {
                        continue
                    }
                    rows.push({
                        "ok": true,
                        "name": String(raw.name || ""),
                        "path": String(raw.path || ""),
                        "thumbnailPath": String(raw.thumbnailPath || ""),
                        "mimeType": String(raw.mimeType || ""),
                        "iconName": String(raw.iconName || ""),
                        "isDir": !!raw.isDir,
                        "networkShared": !!raw.networkShared,
                        "size": Number(raw.size || 0),
                        "dateAdded": String(raw.dateAdded || ""),
                        "lastModified": String(raw.lastModified || "")
                    })
                    seen[p] = true
                }
            }
        }
        desktopEntriesCache = rows
        if (desktopViewController && desktopViewController.setItems) {
            desktopViewController.setItems(rows)
        }
        _requestDesktopThumbnails(rows)
    }

    function _requestDesktopThumbnails(rowsValue) {
        if (!fileManagerApiRef || !fileManagerApiRef.requestThumbnailAsync) {
            return
        }
        var rows = rowsValue ? rowsValue : (desktopEntriesCache ? desktopEntriesCache : [])
        var requested = Object.assign({}, thumbnailRequestedPaths)
        var retryAfter = Object.assign({}, thumbnailRetryAfterByPath)
        var nowMs = Date.now()
        var touched = false
        for (var i = 0; i < rows.length; ++i) {
            var row = rows[i] || ({})
            if (!!row.isDir) {
                continue
            }
            var path = _normalizedPath(String(row.path || ""))
            if (path.length <= 0 || _isDesktopLauncherPath(path)) {
                continue
            }
            var thumb = String(row.thumbnailPath || "")
            if (thumb.length > 0 || requested[path]) {
                continue
            }
            var retryAt = Number(retryAfter[path] || 0)
            if (retryAt > nowMs) {
                continue
            }
            requested[path] = true
            touched = true
            fileManagerApiRef.requestThumbnailAsync(path, Number(desktopThumbnailSize || 192))
        }
        if (touched) {
            thumbnailRequestedPaths = requested
        }
    }

    function _applyDesktopThumbnail(pathValue, thumbPath) {
        var path = _normalizedPath(pathValue)
        var thumb = String(thumbPath || "").trim()
        if (path.length <= 0 || thumb.length <= 0 || !desktopEntriesCache || desktopEntriesCache.length <= 0) {
            return
        }
        var next = desktopEntriesCache.slice(0)
        var changed = false
        for (var i = 0; i < next.length; ++i) {
            var row = next[i] || ({})
            var rowPath = _normalizedPath(String(row.path || ""))
            if (rowPath !== path) {
                continue
            }
            if (String(row.thumbnailPath || "") === thumb) {
                break
            }
            var copy = Object.assign({}, row)
            copy.thumbnailPath = thumb
            next[i] = copy
            changed = true
            break
        }
        if (!changed) {
            return
        }
        desktopEntriesCache = next
        if (desktopViewController && desktopViewController.setItems) {
            desktopViewController.setItems(next)
        }
    }

    function _ensureDesktopDirectory() {
        if (!fileManagerApiRef || !fileManagerApiRef.statPath || !fileManagerApiRef.createDirectory) {
            return
        }
        var stat = fileManagerApiRef.statPath(desktopPath)
        if (!stat || !stat.ok || !stat.exists || !stat.isDir) {
            fileManagerApiRef.createDirectory(desktopPath, true)
        }
        _cleanupLegacyDesktopMetaFiles()
    }

    function _cleanupLegacyDesktopMetaFiles() {
        if (!fileManagerApiRef || !fileManagerApiRef.removePath || !fileManagerApiRef.statPath) {
            return
        }
        var base = String(desktopPath || "").trim()
        if (base.length <= 0) {
            return
        }
        var legacy = [
            base + "/.desktop_shell_slot_map.json",
            base + "/.desktop_shell_shortcut_order"
        ]
        for (var i = 0; i < legacy.length; ++i) {
            var p = String(legacy[i] || "")
            var st = fileManagerApiRef.statPath(p)
            if (st && st.ok && !!st.exists && !st.isDir) {
                fileManagerApiRef.removePath(p, false)
            }
        }
    }

    function _normalizedPath(pathValue) {
        var p = String(pathValue || "").trim()
        if (p.length <= 0) {
            return ""
        }
        if (fileManagerApiRef && fileManagerApiRef.statPath) {
            var st = fileManagerApiRef.statPath(p)
            if (st && st.ok) {
                var abs = String(st.absolutePath || "")
                if (abs.length > 0) {
                    return abs
                }
            }
        }
        return p
    }

    function _parentPath(pathValue) {
        var p = String(pathValue || "").trim()
        if (p.length <= 0) {
            return ""
        }
        while (p.length > 1 && p.charAt(p.length - 1) === "/") {
            p = p.slice(0, p.length - 1)
        }
        var idx = p.lastIndexOf("/")
        if (idx <= 0) {
            return "/"
        }
        return p.slice(0, idx)
    }

    function _shouldRefreshForChangedPath(pathValue) {
        var changedPath = root._normalizedPath(pathValue)
        var desktopPathNorm = root._normalizedPath(root.desktopPath)
        var currentPathNorm = root._normalizedPath(root.fileModel && root.fileModel.currentPath
                                                   ? root.fileModel.currentPath : root.desktopPath)
        if (changedPath === desktopPathNorm || changedPath === currentPathNorm) {
            return true
        }
        var changedParent = root._normalizedPath(root._parentPath(pathValue))
        if (changedParent === desktopPathNorm || changedParent === currentPathNorm) {
            return true
        }
        return false
    }

    function _desktopEntriesByPath() {
        var out = ({})
        if (!fileManagerApiRef || !fileManagerApiRef.listDirectory) {
            return out
        }
        var res = fileManagerApiRef.listDirectory(String(desktopPath || ""), false, true)
        if (!res || !res.ok || !res.entries) {
            return out
        }
        var rows = res.entries || []
        for (var i = 0; i < rows.length; ++i) {
            var row = rows[i] || ({})
            var path = _normalizedPath(String(row.path || ""))
            if (path.length <= 0 || _isIgnoredDesktopMetaPath(path)) {
                continue
            }
            out[path] = row
        }
        return out
    }

    function _createdDesktopPath(beforeMap, afterMap) {
        var out = ""
        for (var path in afterMap) {
            if (!beforeMap[path]) {
                out = path
                break
            }
        }
        return out
    }

    function _coerceAppData(appData) {
        var row = appData ? appData : ({})
        return ({
            "name": String(row.name || ""),
            "desktopFile": String(row.desktopFile || ""),
            "desktopId": String(row.desktopId || ""),
            "executable": String(row.executable || ""),
            "iconName": String(row.iconName || ""),
            "source": String(row.source || "")
        })
    }

    function _writeDesktopLauncher(targetDir, appData) {
        if (!fileManagerApiRef || !fileManagerApiRef.writeTextFile || !fileManagerApiRef.statPath) {
            return ""
        }
        var executable = String(appData.executable || "").trim()
        if (executable.length <= 0) {
            return ""
        }
        var baseName = String(appData.name || "Application").trim()
        if (baseName.length <= 0) {
            baseName = "Application"
        }
        var bad = "\\/:*?\"<>|"
        for (var i = 0; i < bad.length; ++i) {
            baseName = baseName.split(bad.charAt(i)).join("_")
        }
        var candidate = String(targetDir || "~/Desktop") + "/" + baseName + ".desktop"
        var probe = fileManagerApiRef.statPath(candidate)
        if (probe && probe.ok && probe.exists) {
            for (var n = 2; n <= 9999; ++n) {
                candidate = String(targetDir || "~/Desktop") + "/" + baseName + " (" + n + ").desktop"
                probe = fileManagerApiRef.statPath(candidate)
                if (!probe || !probe.ok || !probe.exists) {
                    break
                }
            }
        }
        var body = "[Desktop Entry]\n"
        body += "Type=Application\n"
        body += "Name=" + baseName + "\n"
        body += "Exec=" + executable + "\n"
        if (String(appData.iconName || "").trim().length > 0) {
            body += "Icon=" + String(appData.iconName || "").trim() + "\n"
        }
        body += "Terminal=false\n"
        body += "NoDisplay=false\n"
        var wr = fileManagerApiRef.writeTextFile(candidate, body, false)
        if (wr && wr.ok) {
            return _normalizedPath(candidate)
        }
        return ""
    }

    function addAppEntryToDesktop(appData, preferredCellX, preferredCellY) {
        var data = _coerceAppData(appData)
        var beforeMap = _desktopEntriesByPath()
        var added = false
        var createdPath = ""
        var targetDir = String(desktopPath || "~/Desktop")
        var sourceLabel = String(data.source || "").trim()
        if (sourceLabel.length <= 0) {
            sourceLabel = "unknown"
        }

        console.log("[desktop-add] begin source=" + sourceLabel
                    + " name=" + String(data.name || "")
                    + " desktopFile=" + String(data.desktopFile || "")
                    + " desktopId=" + String(data.desktopId || "")
                    + " exec=" + String(data.executable || "")
                    + " targetCell=(" + String(Number(preferredCellX)) + "," + String(Number(preferredCellY)) + ")")

        if (fileManagerApiRef && fileManagerApiRef.createDirectory) {
            fileManagerApiRef.createDirectory(targetDir, true)
        }

        if (!added && fileManagerApiRef && fileManagerApiRef.copyPaths && data.desktopFile.length > 0) {
            var copyRes = fileManagerApiRef.copyPaths([data.desktopFile], targetDir, false)
            added = !!(copyRes && copyRes.ok)
        }

        if (!added && typeof ShortcutModel !== "undefined" && ShortcutModel) {
            if (data.desktopFile.length > 0 && ShortcutModel.addDesktopShortcut) {
                added = ShortcutModel.addDesktopShortcut(data.desktopFile)
            }
            if (!added && data.desktopId.length > 0 && ShortcutModel.addDesktopShortcutById) {
                added = ShortcutModel.addDesktopShortcutById(data.desktopId)
            }
            if (!added && data.executable.length > 0 && ShortcutModel.addDesktopShortcut) {
                added = ShortcutModel.addDesktopShortcut(data.executable)
            }
            if (!added && data.name.length > 0 && ShortcutModel.addDesktopShortcut) {
                added = ShortcutModel.addDesktopShortcut(data.name)
            }
        }

        if (!added) {
            createdPath = _writeDesktopLauncher(targetDir, data)
            added = createdPath.length > 0
        }

        if (!added) {
            console.warn("[desktop-add] failed source=" + sourceLabel
                         + " name=" + String(data.name || "")
                         + " desktopFile=" + String(data.desktopFile || "")
                         + " desktopId=" + String(data.desktopId || "")
                         + " exec=" + String(data.executable || ""))
            return false
        }

        var afterMap = _desktopEntriesByPath()
        if (createdPath.length <= 0) {
            createdPath = _createdDesktopPath(beforeMap, afterMap)
        }
        if (createdPath.length <= 0 && data.desktopFile.length > 0) {
            var srcName = String(data.desktopFile).split("/").pop()
            var candidate = _normalizedPath(targetDir + "/" + srcName)
            if (candidate.length > 0) {
                createdPath = candidate
            }
        }

        if (fileModel && fileModel.refresh) {
            fileModel.refresh()
        }
        requestDesktopRefresh("add-app-entry")
        console.log("[desktop-add] created source=" + sourceLabel
                    + " createdPath=" + String(createdPath || "")
                    + " beforeCount=" + String(Object.keys(beforeMap).length)
                    + " afterCount=" + String(Object.keys(afterMap).length))
        Qt.callLater(function() {
            root._syncItemsToController()
            if (root.desktopViewRef && root.desktopViewRef.refresh) {
                root.desktopViewRef.refresh()
            }
            if (createdPath.length > 0
                    && root.desktopViewRef
                    && root.desktopViewRef.placePathAtCell
                    && Number.isInteger(Number(preferredCellX))
                    && Number.isInteger(Number(preferredCellY))
                    && Number(preferredCellX) >= 0
                    && Number(preferredCellY) >= 0) {
                root.desktopViewRef.placePathAtCell(createdPath,
                                                    Number(preferredCellX),
                                                    Number(preferredCellY))
                console.log("[desktop-add] placed path=" + String(createdPath || "")
                            + " cell=(" + String(Number(preferredCellX)) + "," + String(Number(preferredCellY)) + ")")
            }
        })
        return true
    }

    function snapDesktopToGrid() {
        if (desktopViewRef && desktopViewRef.snapToGrid) {
            desktopViewRef.snapToGrid()
            _notifyInfo("Snap to grid applied.")
        }
    }

    function cleanUpDesktop() {
        if (desktopViewRef && desktopViewRef.cleanUpDesktop) {
            desktopViewRef.cleanUpDesktop()
            _notifyInfo("Desktop cleaned up.")
        }
    }

    function _slotMapSummary() {
        if (!fileModel || !fileModel.loadSlotMap) {
            return "slotMap=unavailable"
        }
        var map = fileModel.loadSlotMap()
        var count = 0
        var samples = []
        for (var k in map) {
            count += 1
            if (samples.length < 5) {
                samples.push(String(k) + "=>" + String(map[k]))
            }
        }
        return "slotMapCount=" + String(count)
                + (samples.length > 0 ? " sample=" + samples.join(" | ") : "")
    }

    function _initModel() {
        if (fileModel || !modelFactoryRef || !modelFactoryRef.createModel) {
            return
        }
        var modelObj = modelFactoryRef.createModel(root)
        if (!modelObj) {
            return
        }
        fileModel = modelObj
        if (fileModel.useSlotOrder !== undefined) {
            fileModel.useSlotOrder = true
        }
        if (fileModel.searchText !== undefined && String(fileModel.searchText || "").length > 0) {
            fileModel.searchText = ""
        }
        fileModel.currentPath = desktopPath
        if (desktopViewController && desktopViewController.setPath) {
            desktopViewController.setPath(desktopPath)
        }
        if (desktopMenuProvider) {
            desktopMenuProvider.desktopSurface = root
            desktopMenuProvider.path = desktopPath
        }
        directoryChanged(desktopPath)
        Qt.callLater(function() {
            root._syncItemsToController()
            root._syncSelectionToController()
        })
    }

    function _reloadDesktop() {
        thumbnailRequestedPaths = ({})
        thumbnailRetryAfterByPath = ({})
        _ensureDesktopDirectory()
        if (!fileModel) {
            _initModel()
            return
        }
        if (fileModel.searchText !== undefined && String(fileModel.searchText || "").length > 0) {
            fileModel.searchText = ""
        }
        fileModel.currentPath = desktopPath
        if (fileModel.refresh) {
            fileModel.refresh()
        }
        if (desktopMenuProvider) {
            desktopMenuProvider.enabled = true
        }
        Qt.callLater(function() {
            root._syncItemsToController()
            root._syncSelectionToController()
        })
    }

    function openSelection() {
        if (!fileModel || !fileModel.entryAt || selectedEntryIndex < 0) {
            return
        }
        var row = fileModel.entryAt(selectedEntryIndex)
        if (!row || !row.ok) {
            return
        }
        var p = String(row.path || "")
        if (p.length <= 0) {
            return
        }

        if (!!row.isDir) {
            if (fileManagerApiRef && fileManagerApiRef.startOpenPathInFileManager) {
                fileManagerApiRef.startOpenPathInFileManager(p, "desktop-open")
            } else {
                _route("filemanager.open", { "target": p })
            }
        } else if (fileManagerApiRef && fileManagerApiRef.startOpenPath) {
            fileManagerApiRef.startOpenPath(p, "desktop-open")
        } else if (fileManagerApiRef && fileManagerApiRef.openPath) {
            fileManagerApiRef.openPath(p)
        }

        if (fileModel && String(fileModel.currentPath || "") !== String(desktopPath || "")) {
            fileModel.currentPath = desktopPath
        }
    }

    function renameSelection() {
        if (!fileModel || !fileModel.entryAt || selectedEntryIndex < 0) {
            return
        }
        var row = fileModel.entryAt(selectedEntryIndex)
        if (!row || !row.ok) {
            return
        }
        var baseName = String(row.name || "").trim()
        if (baseName.length <= 0) {
            return
        }
        // Strip .desktop suffix so user edits only the stem; applyInlineRename adds it back
        if (_isDesktopLauncherPath(String(row.path || ""))) {
            var lowerBase = baseName.toLowerCase()
            if (lowerBase.length > 8 && lowerBase.lastIndexOf(".desktop") === (lowerBase.length - 8)) {
                baseName = baseName.slice(0, baseName.length - 8)
            }
        }
        if (baseName.length <= 0) {
            return
        }
        if (launcherShortcutMenu && launcherShortcutMenu.visible) {
            launcherShortcutMenu.close()
        }
        if (desktopViewRef && desktopViewRef.startInlineRename) {
            desktopViewRef.startInlineRename(Number(selectedEntryIndex), baseName)
            return
        }
        _notifyInfo("Inline rename unavailable.")
    }

    function applyInlineRename(indexValue, nameValue) {
        var idx = (indexValue != null) ? Number(indexValue) : -1
        if (!fileModel || !fileModel.renameAt || !fileModel.entryAt || idx < 0) {
            return
        }
        var row = fileModel.entryAt(idx)
        if (!row || !row.ok) {
            return
        }
        var currentName = String(row.name || "").trim()
        if (currentName.length <= 0) {
            return
        }
        // Normalize currentName to stem (no .desktop) so comparison is consistent
        var currentPath = String(row.path || "")
        if (_isDesktopLauncherPath(currentPath)) {
            var lc = currentName.toLowerCase()
            if (lc.length > 8 && lc.lastIndexOf(".desktop") === (lc.length - 8)) {
                currentName = currentName.slice(0, currentName.length - 8)
            }
        }
        var nextName = String(nameValue || "").trim()
        if (nextName.length <= 0) {
            _notifyInfo("Name cannot be empty.")
            if (desktopViewRef && desktopViewRef.startInlineRename) {
                desktopViewRef.startInlineRename(idx, currentName)
            }
            return
        }
        if (nextName === "." || nextName === ".." || nextName.indexOf("/") >= 0) {
            _notifyInfo("Invalid file name.")
            if (desktopViewRef && desktopViewRef.startInlineRename) {
                desktopViewRef.startInlineRename(idx, currentName)
            }
            return
        }
        var renamedFileName = nextName
        if (root._isDesktopLauncherPath(currentPath)) {
            var lower = nextName.toLowerCase()
            var hasDesktopSuffix = lower.length > 8
                    && lower.lastIndexOf(".desktop") === (lower.length - 8)
            if (!hasDesktopSuffix) {
                renamedFileName = nextName + ".desktop"
            }
        }
        var currentFileName = currentPath.split("/").pop()
        console.log("[rename] cur='" + currentFileName + "' next='" + renamedFileName
                    + "' curName='" + currentName + "' nextName='" + nextName + "'")
        if (renamedFileName === currentFileName || nextName === currentName) {
            console.log("[rename] no-op")
            return
        }
        var res = fileModel.renameAt(idx, renamedFileName)
        console.log("[rename] renameAt res=" + JSON.stringify(res))
        if (!res || !res.ok) {
            _notifyInfo(String((res && res.error) ? res.error : "rename-failed"))
            if (desktopViewRef && desktopViewRef.startInlineRename) {
                desktopViewRef.startInlineRename(idx, currentName)
            }
            return
        }
        // For .desktop files: update Name= field using GKeyFile via gio
        if (_isDesktopLauncherPath(currentPath) && fileManagerApiRef && fileManagerApiRef.setDesktopFileKey) {
            var parentDir = currentPath.substring(0, currentPath.lastIndexOf("/"))
            var newFilePath = parentDir + "/" + renamedFileName
            var kr = fileManagerApiRef.setDesktopFileKey(newFilePath, "Desktop Entry", "Name", nextName)
            console.log("[rename] setDesktopFileKey res=" + JSON.stringify(kr))
        }
        if (fileModel.refresh) {
            fileModel.refresh()
        }
    }

    function _selectedPaths() {
        var rows = _selectionRows()
        var out = []
        for (var i = 0; i < rows.length; ++i) {
            out.push(String(rows[i].path || ""))
        }
        return out
    }

    function _selectedSingleRow() {
        if (!fileModel || !fileModel.entryAt || selectedEntryIndex < 0) {
            return null
        }
        var row = fileModel.entryAt(selectedEntryIndex)
        return (row && row.ok) ? row : null
    }

    function _isDesktopLauncherRow(row) {
        if (!row) {
            return false
        }
        var p = String(row.path || "").toLowerCase()
        return p.length > 8 && p.lastIndexOf(".desktop") === (p.length - 8)
    }

    function addSelectionToDock() {
        var row = _selectedSingleRow()
        if (!_isDesktopLauncherRow(row)) {
            return
        }
        if (typeof DockModel !== "undefined" && DockModel && DockModel.addDesktopEntry) {
            DockModel.addDesktopEntry(String(row.path || ""))
        }
    }

    function _isDesktopLauncherPath(pathValue) {
        var p = String(pathValue || "").toLowerCase()
        return p.length > 8 && p.lastIndexOf(".desktop") === (p.length - 8)
    }

    function _entryPathAt(indexValue) {
        var idx = Number(indexValue || -1)
        if (idx < 0 || !fileModel || !fileModel.entryAt) {
            return ""
        }
        var row = fileModel.entryAt(idx)
        if (!row || !row.ok) {
            return ""
        }
        return String(row.path || "")
    }

    function _loadSlotByPath() {
        if (!fileModel || !fileModel.loadSlotMap) {
            return ({})
        }
        var loaded = fileModel.loadSlotMap()
        return loaded ? loaded : ({})
    }

    function _ensureSlotForPath(slotByPath, pathValue, fallbackSlot) {
        var p = String(pathValue || "")
        if (p.length <= 0) {
            return
        }
        if (slotByPath[p] !== undefined) {
            return
        }
        slotByPath[p] = Number(fallbackSlot || 0)
    }

    function _persistReorderedSlots(sourcePath, sourceIndex, targetPath, targetIndex) {
        if (!fileModel || !fileModel.saveSlotMap) {
            return
        }
        var src = String(sourcePath || "")
        var dst = String(targetPath || "")
        if (src.length <= 0 || dst.length <= 0 || src === dst) {
            return
        }
        var slotByPath = _loadSlotByPath()
        if (fileModel && fileModel.count !== undefined && fileModel.entryAt) {
            var total = Number(fileModel.count || 0)
            for (var i = 0; i < total; ++i) {
                var p = _entryPathAt(i)
                if (p.length > 0 && slotByPath[p] === undefined) {
                    slotByPath[p] = i
                }
            }
        }
        _ensureSlotForPath(slotByPath, src, Number(sourceIndex || 0))
        _ensureSlotForPath(slotByPath, dst, Number(targetIndex || 0))
        var srcSlot = Number(slotByPath[src] || 0)
        var dstSlot = Number(slotByPath[dst] || 0)
        slotByPath[src] = dstSlot
        slotByPath[dst] = srcSlot
        fileModel.saveSlotMap(slotByPath)
    }

    function _isDockDropZone(localY) {
        var y = Number(localY || -1)
        if (y < 0) {
            return false
        }
        if (dockTopY > 0) {
            return y >= Number(dockTopY)
        }
        return y >= Math.max(0, height - 120)
    }

    function beginDesktopDrag(indexValue, pathValue, nameValue, copyMode, sceneX, sceneY) {
        dndActive = true
        dndCopyMode = !!copyMode
        dndSourceIndex = Number(indexValue || -1)
        dndSourcePath = String(pathValue || "")
        dndSourceName = String(nameValue || "")
        dndSourceIconName = ""
        dndSourceIconSource = ""
        dndHoverIndex = -1
        if (fileModel && fileModel.entryAt && dndSourceIndex >= 0) {
            var row = fileModel.entryAt(dndSourceIndex)
            if (row && row.ok) {
                dndSourceIconName = String(row.iconName || "")
            }
        }
        dndLastX = Number(sceneX || -1)
        dndLastY = Number(sceneY || -1)
    }

    function updateDesktopDrag(sceneX, sceneY, hoverIndex) {
        if (!dndActive) {
            return
        }
        dndLastX = Number(sceneX || dndLastX)
        dndLastY = Number(sceneY || dndLastY)
        dndHoverIndex = Number(hoverIndex || -1)
    }

    function finishDesktopDrag(targetIndex, sceneX, sceneY, fallbackIndex) {
        if (!dndActive) {
            return
        }
        dndLastX = Number(sceneX || dndLastX)
        dndLastY = Number(sceneY || dndLastY)
        var target = Number(targetIndex || -1)
        if (target < 0 && dndHoverIndex >= 0) {
            target = dndHoverIndex
        }
        if (target < 0) {
            target = Number(fallbackIndex || -1)
        }
        var source = String(dndSourcePath || "")
        var droppedInFolder = false
        if (target >= 0 && fileModel && fileModel.entryAt && source.length > 0) {
            var row = fileModel.entryAt(target)
            var targetDir = String((row && row.ok && row.isDir) ? row.path : "")
            if (targetDir.length > 0 && targetDir !== source && fileManagerApiRef) {
                if (dndCopyMode && fileManagerApiRef.startCopyPaths) {
                    fileManagerApiRef.startCopyPaths([source], targetDir, false)
                    droppedInFolder = true
                } else if (!dndCopyMode && fileManagerApiRef.startMovePaths) {
                    fileManagerApiRef.startMovePaths([source], targetDir, false)
                    droppedInFolder = true
                }
            }
        }
        var canPinToDock = _isDesktopLauncherPath(dndSourcePath)
        if (!droppedInFolder && canPinToDock && _isDockDropZone(dndLastY)
                && typeof DockModel !== "undefined" && DockModel && DockModel.addDesktopEntry) {
            DockModel.addDesktopEntry(dndSourcePath)
        }
        if (!droppedInFolder && !_isDockDropZone(dndLastY) && target >= 0 && !dndCopyMode) {
            _persistReorderedSlots(source, dndSourceIndex, _entryPathAt(target), target)
        }
        dndActive = false
        dndCopyMode = false
        dndSourceIndex = -1
        dndSourcePath = ""
        dndSourceName = ""
        dndSourceIconName = ""
        dndSourceIconSource = ""
        dndHoverIndex = -1
        dndLastX = -1
        dndLastY = -1
    }

    function _route(action, payload) {
        if (typeof AppCommandRouter === "undefined" || !AppCommandRouter || !AppCommandRouter.route) {
            return
        }
        AppCommandRouter.route(String(action || ""),
                               payload ? payload : ({}),
                               "desktop-view")
    }

    function _notifyInfo(message) {
        var body = String(message || "").trim()
        if (body.length <= 0) {
            return
        }
        if (typeof NotificationManager !== "undefined"
                && NotificationManager
                && NotificationManager.Notify) {
            NotificationManager.Notify("Desktop", 0,
                                       "dialog-information-symbolic",
                                       "Desktop View", body, [], {}, 2200)
        } else {
            console.log("[desktop-view]", body)
        }
    }

    function hasSelection() {
        return _selectedPaths().length > 0
    }

    function selectedPaths() {
        return _selectedPaths()
    }

    function createNewFile() {
        if (!fileManagerApiRef || !fileManagerApiRef.createEmptyFile) {
            return
        }
        fileManagerApiRef.createEmptyFile(String(desktopPath || "~/Desktop") + "/New File.txt")
        if (fileModel && fileModel.refresh) {
            fileModel.refresh()
        }
    }

    function openSelectionWithAppChooser() {
        var paths = _selectedPaths()
        if (paths.length <= 0) {
            _notifyInfo("No selected item to open.")
            return
        }
        if (fileManagerApiRef && fileManagerApiRef.startOpenPathInFileManager) {
            fileManagerApiRef.startOpenPathInFileManager(String(paths[0] || ""), "desktop-open-with")
            _notifyInfo("Use Open With from File Manager window.")
            return
        }
        openSelection()
    }

    function revealSelectionInHome() {
        var paths = _selectedPaths()
        if (paths.length > 0 && fileManagerApiRef && fileManagerApiRef.startOpenPathInFileManager) {
            fileManagerApiRef.startOpenPathInFileManager(String(paths[0] || ""), "desktop-reveal")
            return
        }
        openHomePath()
    }

    function compressSelection() {
        if (!fileManagerApiRef || !fileManagerApiRef.startCompressArchive) {
            return
        }
        var paths = _selectedPaths()
        if (paths.length <= 0) {
            return
        }
        var stamp = Qt.formatDateTime(new Date(), "yyyyMMdd-HHmmss")
        var archivePath = String(desktopPath || "~/Desktop") + "/Desktop-Selection-" + stamp + ".tar"
        fileManagerApiRef.startCompressArchive(paths, archivePath, "tar")
    }

    function moveSelectionToTrash() {
        if (!fileManagerApiRef || !fileManagerApiRef.startTrashPaths) {
            return
        }
        var paths = _selectedPaths()
        if (paths.length <= 0) {
            return
        }
        fileManagerApiRef.startTrashPaths(paths)
    }

    function deleteSelectionPermanently() {
        if (!fileManagerApiRef || !fileManagerApiRef.startRemovePaths) {
            return
        }
        var paths = _selectedPaths()
        if (paths.length <= 0) {
            return
        }
        fileManagerApiRef.startRemovePaths(paths, true)
    }

    function copySelection(cutMode) {
        var paths = _selectedPaths()
        if (paths.length <= 0) {
            return
        }
        clipboardPaths = paths.slice(0)
        clipboardCut = !!cutMode
    }

    function pasteSelectionToDesktop() {
        if (!fileManagerApiRef) {
            return
        }
        var sources = clipboardPaths || []
        if (!sources || sources.length <= 0) {
            return
        }
        var target = String(desktopPath || "~/Desktop")
        if (clipboardCut) {
            if (fileManagerApiRef.startMovePaths) {
                fileManagerApiRef.startMovePaths(sources, target, false)
                clipboardPaths = []
                clipboardCut = false
            }
            return
        }
        if (fileManagerApiRef.startCopyPaths) {
            fileManagerApiRef.startCopyPaths(sources, target, false)
        }
    }

    function duplicateSelection() {
        if (!fileManagerApiRef || !fileManagerApiRef.startCopyPaths) {
            return
        }
        var paths = _selectedPaths()
        if (paths.length <= 0) {
            return
        }
        fileManagerApiRef.startCopyPaths(paths, String(desktopPath || "~/Desktop"), false)
    }

    function openPropertiesSelection() {
        if (selectedEntryIndex < 0 || !fileModel || !fileModel.entryAt) {
            return
        }
        var row = fileModel.entryAt(selectedEntryIndex)
        if (!row || !row.ok) {
            return
        }
        var p = String(row.path || "")
        if (shellApi && shellApi.detachedFileManagerVisible
                && shellApi.detachedFileManagerWindow
                && shellApi.detachedFileManagerWindow.showPropertiesIfReady
                && shellApi.detachedFileManagerWindow.showPropertiesIfReady(p)) {
            return
        }
        if (shellApi && shellApi.pendingDetachedFileManagerPropertiesPath !== undefined) {
            shellApi.pendingDetachedFileManagerPropertiesPath = p
            shellApi.detachedFileManagerPath = _parentDir(p)
            shellApi.detachedFileManagerVisible = true
            return
        }
        if (fileManagerApiRef && fileManagerApiRef.startOpenPathInFileManager) {
            fileManagerApiRef.startOpenPathInFileManager(p, "desktop-properties")
        }
    }

    function createNewFolder() {
        if (!fileModel || !fileModel.createDirectory) {
            return
        }
        fileModel.createDirectory("New Folder")
    }

    function openTerminalHere() {
        if (typeof AppExecutionGate !== "undefined" && AppExecutionGate && AppExecutionGate.launchTerminalAt) {
            AppExecutionGate.launchTerminalAt(desktopPath)
        }
    }

    function selectAllEntries() {
        if (!fileModel) {
            return
        }
        var out = []
        var count = Number(fileModel.count || 0)
        for (var i = 0; i < count; ++i) {
            out.push(i)
        }
        selectedEntryIndexes = out
        selectedEntryIndex = count > 0 ? Number(out[0]) : -1
        selectionAnchorIndex = selectedEntryIndex
        _syncSelectionToController()
    }

    function clearSelection() {
        selectedEntryIndexes = []
        selectedEntryIndex = -1
        selectionAnchorIndex = -1
        _syncSelectionToController()
    }

    function openHomePath() {
        if (fileModel) {
            fileModel.currentPath = "~"
        }
    }

    function openDesktopPath() {
        if (fileModel) {
            fileModel.currentPath = desktopPath
        }
    }

    function openPathInFileManager(pathValue) {
        var target = String(pathValue || "").trim()
        if (target.length <= 0) {
            return
        }
        _route("filemanager.open", { "target": target })
    }

    function desktopUndo() {
        _notifyInfo("Undo is not available for desktop actions yet.")
    }

    function desktopRedo() {
        _notifyInfo("Redo is not available for desktop actions yet.")
    }

    function toggleShowFileExtensions() {
        showFileExtensions = !showFileExtensions
        _notifyInfo(showFileExtensions ? "Show file extensions: On" : "Show file extensions: Off")
    }

    function cycleIconSizeMode() {
        iconSizeMode = (Number(iconSizeMode || 1) + 1) % 3
        var label = iconSizeMode === 0 ? "Small" : (iconSizeMode === 1 ? "Medium" : "Large")
        _notifyInfo("Icon size: " + label)
    }

    function cycleGridSpacingMode() {
        gridSpacingMode = (Number(gridSpacingMode || 1) + 1) % 3
        var label = gridSpacingMode === 0 ? "Tight" : (gridSpacingMode === 1 ? "Normal" : "Wide")
        _notifyInfo("Grid spacing: " + label)
    }

    function arrangeDesktopIconsBy(sortKey, descending) {
        if (desktopViewRef && desktopViewRef.sortDesktop) {
            var key = String(sortKey || "name")
            if (key === "kind") {
                key = "type"
            }
            desktopViewRef.sortDesktop(key, !!descending)
            var label2 = key === "dateModified" ? "date modified" : key
            _notifyInfo("Icons arranged by " + label2 + ".")
            return
        }
        if (!fileModel || !fileModel.entryAt || !fileModel.saveSlotMap) {
            return
        }
        var previousUseSlotOrder = (fileModel.useSlotOrder !== undefined) ? !!fileModel.useSlotOrder : false
        if (fileModel.useSlotOrder !== undefined) {
            fileModel.useSlotOrder = false
        }
        if (fileModel.setSort) {
            fileModel.setSort(String(sortKey || "name"), !!descending)
        }
        var total = Number(fileModel.count || 0)
        var slotMap = ({})
        for (var i = 0; i < total; ++i) {
            var row = fileModel.entryAt(i)
            if (!row || !row.ok) {
                continue
            }
            var p = String(row.path || "")
            if (p.length > 0) {
                slotMap[p] = i
            }
        }
        var saved = fileModel.saveSlotMap(slotMap)
        if (fileModel.useSlotOrder !== undefined) {
            fileModel.useSlotOrder = previousUseSlotOrder
        }
        if (fileModel.refresh) {
            fileModel.refresh()
        }
        var label = String(sortKey || "name")
        if (label === "dateModified") {
            label = "date modified"
        }
        _notifyInfo(saved ? ("Icons arranged by " + label + ".") : "Failed to arrange icons.")
    }

    function arrangeDesktopIcons() {
        if (desktopViewRef && desktopViewRef.cleanUpDesktop) {
            desktopViewRef.cleanUpDesktop()
            _notifyInfo("Desktop cleaned up.")
            return
        }
        arrangeDesktopIconsBy("name", false)
    }

    function openMountedDevicesPath() {
        if (!fileManagerApiRef || !fileManagerApiRef.storageLocations) {
            openPathInFileManager("/media")
            return
        }
        var rows = fileManagerApiRef.storageLocations() || []
        for (var i = 0; i < rows.length; ++i) {
            var row = rows[i] || ({})
            if (!row.mounted) {
                continue
            }
            var p = String(row.path || row.rootPath || "")
            if (p.length > 0 && p.indexOf("__mount__:") !== 0) {
                openPathInFileManager(p)
                return
            }
        }
        openPathInFileManager("/media")
    }

    function openRecentFoldersPath() {
        if (!fileManagerApiRef || !fileManagerApiRef.recentFiles) {
            openPathInFileManager("__recent__")
            return
        }
        var rows = fileManagerApiRef.recentFiles(80) || []
        var seen = ({})
        for (var i = 0; i < rows.length; ++i) {
            var row = rows[i] || ({})
            var p = String(row.path || "")
            if (p.length <= 0) {
                continue
            }
            var idx = p.lastIndexOf("/")
            if (idx <= 0) {
                continue
            }
            var parent = p.slice(0, idx)
            if (parent.length <= 0 || seen[parent]) {
                continue
            }
            seen[parent] = true
            openPathInFileManager(parent)
            return
        }
        openPathInFileManager("__recent__")
    }

    function applySelectionRequest(indexValue, modifiers) {
        FileManagerSelection.applySelectionRequest(root, Qt, Number(indexValue), Number(modifiers || 0))
        _syncSelectionToController()
    }

    function applySelectionRect(indexesValue, modifiers, anchorIndex, baseIndexes) {
        FileManagerSelection.applySelectionRect(root,
                                                Qt,
                                                indexesValue,
                                                Number(modifiers || 0),
                                                Number(anchorIndex || -1),
                                                baseIndexes ? baseIndexes : [])
        _syncSelectionToController()
    }

    focus: true

    Keys.onDeletePressed: function(event) {
        if (root.selectedEntryIndexes.length > 0) {
            event.accepted = true
            root.moveSelectionToTrash()
        }
    }

    Component.onCompleted: {
        _reloadDesktop()
    }

    onDesktopPathChanged: {
        _reloadDesktop()
    }

    Timer {
        id: backendRetryTimer
        interval: 1200
        repeat: true
        running: !root.fileModel
        onTriggered: {
            if (root.desktopMenuProvider) {
                root.desktopMenuProvider.enabled = false
            }
            root._initModel()
        }
    }

    Connections {
        target: root.fileModel
        ignoreUnknownSignals: true
        function onCurrentPathChanged() {
            var p = String(root.fileModel && root.fileModel.currentPath ? root.fileModel.currentPath : root.desktopPath)
            root.enforceDesktopPath("current-path-changed")
            if (root.desktopViewController && root.desktopViewController.setPath) {
                root.desktopViewController.setPath(p)
            }
            if (root.desktopMenuProvider) {
                root.desktopMenuProvider.path = p
                root.desktopMenuProvider.enabled = true
            }
            root.directoryChanged(p)
        }
        function onCountChanged() {
            root._syncItemsToController()
            root._syncSelectionToController()
            root.ensureDesktopSync("count-changed")
        }
        function onModelReset() {
            root._syncItemsToController()
            root._syncSelectionToController()
        }
        function onRowsInserted() {
            root._syncItemsToController()
            root._syncSelectionToController()
        }
        function onRowsRemoved() {
            root._syncItemsToController()
            root._syncSelectionToController()
        }
        function onDataChanged() {
            root._syncItemsToController()
        }
    }

    Connections {
        target: root.fileManagerApiRef
        ignoreUnknownSignals: true
        function onThumbnailReady(pathValue, size, thumbnailPath, ok, error) {
            var normalizedPath = root._normalizedPath(String(pathValue || ""))
            if (normalizedPath.length > 0) {
                var requested = Object.assign({}, root.thumbnailRequestedPaths)
                delete requested[normalizedPath]
                root.thumbnailRequestedPaths = requested
            }
            if (!ok || String(thumbnailPath || "").length <= 0) {
                if (normalizedPath.length > 0) {
                    var retryAfter = Object.assign({}, root.thumbnailRetryAfterByPath)
                    retryAfter[normalizedPath] = Date.now() + 30000
                    root.thumbnailRetryAfterByPath = retryAfter
                }
                return
            }
            if (normalizedPath.length > 0) {
                var retryMap = Object.assign({}, root.thumbnailRetryAfterByPath)
                delete retryMap[normalizedPath]
                root.thumbnailRetryAfterByPath = retryMap
            }
            root._applyDesktopThumbnail(normalizedPath,
                                        String(thumbnailPath || ""))
        }
        function onDirectoryChanged(pathValue) {
            if (root._shouldRefreshForChangedPath(pathValue)
                    && root.fileModel && root.fileModel.refresh) {
                root.requestDesktopRefresh("directory-changed")
            }
        }
        function onFileChanged(pathValue) {
            if (root._shouldRefreshForChangedPath(pathValue)
                    && root.fileModel && root.fileModel.refresh) {
                root.requestDesktopRefresh("file-changed")
            }
        }
        function onPathChanged(pathValue, kind) {
            if (root._shouldRefreshForChangedPath(pathValue)
                    && root.fileModel && root.fileModel.refresh) {
                root.requestDesktopRefresh("path-changed")
            }
        }
    }

    Timer {
        id: refreshDebounceTimer
        interval: 140
        repeat: false
        onTriggered: {
            if (root._contextMenuActive()) {
                refreshDebounceTimer.restart()
                return
            }
            if (!root.fileModel || !root.fileModel.refresh) {
                return
            }
            root.fileModel.refresh()
            postRefreshSyncTimer.restart()
        }
    }

    Timer {
        id: postRefreshSyncTimer
        interval: 180
        repeat: false
        onTriggered: root.ensureDesktopSync(root.pendingRefreshReason.length > 0 ? root.pendingRefreshReason : "post-refresh")
    }

    Timer {
        id: syncRepairTimer
        interval: 180
        repeat: false
        onTriggered: root.ensureDesktopSync("timer")
    }

    Timer {
        id: periodicDesktopSyncTimer
        interval: 1600
        repeat: true
        running: !!root.fileModel
        onTriggered: {
            if (root._contextMenuActive()) {
                return
            }
            root.enforceDesktopPath("periodic")
            root.ensureDesktopSync("periodic")
        }
    }

    Loader {
        id: contentLoader
        anchors.fill: parent
        active: !!root.fileModel
        sourceComponent: DesktopView {
            id: contentView
            anchors.fill: parent
            fileModel: root.fileModel
            entriesModel: root.desktopEntriesCache
            selectedIndex: root.selectedEntryIndex
            selectedIndexes: root.selectedEntryIndexes
            selectionAnchorIndex: root.selectionAnchorIndex
            showFileExtensions: root.showFileExtensions
            iconSizeMode: root.iconSizeMode
            gridSpacingMode: root.gridSpacingMode
            suppressItemContextMenu: !!(desktopCtxMenu && desktopCtxMenu.menuVisible)
            topPadding: 42
            leftPadding: 20
            rightPadding: 20
            bottomPadding: root.desktopShellBottomMargin

            onSelectedIndexRequested: function(index, modifiers) {
                root.applySelectionRequest(index, modifiers)
            }
            onActivateRequested: function(index) {
                root.applySelectionRequest(index, Qt.NoModifier)
                root.openSelection()
            }
            onBackgroundContextRequested: function(x, y) {
                if (Date.now() < Number(root.suppressDesktopBackgroundMenuUntilMs || 0)) {
                    return
                }
                if (desktopCtxMenu && desktopCtxMenu.menuVisible) {
                    return
                }
                if (launcherShortcutMenu.visible) {
                    return
                }
                root.suppressContextMenuDismissUntilMs = Date.now() + 220
                root.contextEntryPath = ""
                root.clearSelection()
                var p = contentView.mapToItem(root, x, y)
                root.activeContextMenuKind = "desktop"
                desktopCtxMenu.openFolderMenu(p.x, p.y)
            }
            onContextMenuRequested: function(index, x, y) {
                if (root.activeContextMenuKind === "desktop"
                        && desktopCtxMenu
                        && desktopCtxMenu.menuVisible) {
                    return
                }
                root.applySelectionRequest(index, Qt.NoModifier)
                var p = contentView.mapToItem(root, x, y)
                var row = root._selectedSingleRow()
                root.suppressDesktopBackgroundMenuUntilMs = Date.now() + 420
                root.suppressContextMenuDismissUntilMs = Date.now() + 220
                root.contextEntryRow = row
                root.contextEntryPath = String((row && row.path) ? row.path : "")
                root.activeContextMenuKind = "item"
                launcherShortcutMenu.popup(p.x, p.y)
            }
            onPointerPressed: function(x, y, button, onItem) {
                if (!onItem && Number(button) === Qt.LeftButton) {
                    root.clearSelection()
                }
                if (Date.now() < Number(root.suppressContextMenuDismissUntilMs || 0)) {
                    return
                }
                if (launcherShortcutMenu.visible) {
                    launcherShortcutMenu.close()
                }
                if (desktopCtxMenu && desktopCtxMenu.closeMenu) {
                    desktopCtxMenu.closeMenu()
                }
                root.activeContextMenuKind = ""
            }
            onClearSelectionRequested: root.clearSelection()
            onRenameCommitRequested: function(index, name) {
                root.applyInlineRename(index, name)
            }
            onSelectionRectRequested: function(indexes, modifiers, anchorIndex, baseIndexes) {
                root.applySelectionRect(indexes, modifiers, anchorIndex, baseIndexes)
            }
            onDragStartRequested: function(index, path, name, isDir, copyMode, sceneX, sceneY) {
                root.beginDesktopDrag(index, path, name, copyMode, sceneX, sceneY)
            }
            onDragMoveRequested: function(sceneX, sceneY, hoverIndex, copyMode) {
                root.updateDesktopDrag(sceneX, sceneY, hoverIndex)
            }
            onDragEndRequested: function(targetIndex) {
                root.finishDesktopDrag(targetIndex, root.dndLastX, root.dndLastY, -1)
            }
            onExternalDropRequested: function(payload, targetCellX, targetCellY) {
                var appEntryPayload = String(payload && payload.appEntryJson ? payload.appEntryJson : "").trim()
                if (appEntryPayload.length > 0) {
                    try {
                        var appData = JSON.parse(appEntryPayload)
                        if (appData && (appData.desktopFile || appData.desktopId || appData.executable || appData.name)) {
                            root.addAppEntryToDesktop(appData, Number(targetCellX), Number(targetCellY))
                            return
                        }
                    } catch (e0) {
                    }
                }
                var desktopItemPayload = String(payload && payload.desktopItemJson ? payload.desktopItemJson : "").trim()
                if (desktopItemPayload.length > 0) {
                    try {
                        var desktopData = JSON.parse(desktopItemPayload)
                        if (desktopData && (desktopData.desktopFile || desktopData.desktopId || desktopData.executable || desktopData.name)) {
                            root.addAppEntryToDesktop(desktopData, Number(targetCellX), Number(targetCellY))
                            return
                        }
                    } catch (e1) {
                    }
                }
                var textPayload = String(payload && payload.text ? payload.text : "").trim()
                if (textPayload.length > 0) {
                    try {
                        var decoded = JSON.parse(textPayload)
                        if (decoded && (decoded.desktopFile || decoded.desktopId || decoded.executable)) {
                            root.addAppEntryToDesktop(decoded, Number(targetCellX), Number(targetCellY))
                            return
                        }
                    } catch (e) {
                    }
                }
                if (!payload || !payload.urls || payload.urls.length <= 0) {
                    var uriList = String(payload && payload.uriListText ? payload.uriListText : "").trim()
                    if (uriList.length <= 0) {
                        return
                    }
                    var urlRows = uriList.split("\n")
                    payload.urls = []
                    for (var u = 0; u < urlRows.length; ++u) {
                        var line = String(urlRows[u] || "").trim()
                        if (line.length > 0 && line.charAt(0) !== "#") {
                            payload.urls.push(line)
                        }
                    }
                    if (!payload.urls || payload.urls.length <= 0) {
                        return
                    }
                }
                var localPaths = []
                for (var i = 0; i < payload.urls.length; ++i) {
                    var url = String(payload.urls[i] || "")
                    if (url.indexOf("file://") === 0) {
                        var p = decodeURIComponent(url.slice(7))
                        if (p.length > 0) {
                            localPaths.push(p)
                        }
                    }
                }
                if (localPaths.length <= 0 || !root.fileManagerApiRef || !root.fileManagerApiRef.startCopyPaths) {
                    return
                }
                root.fileManagerApiRef.startCopyPaths(localPaths, String(root.desktopPath || "~/Desktop"), false)
            }
        }
        onLoaded: {
            root.desktopViewRef = contentLoader.item
        }
    }

    ContextMenuComp.DesktopContextMenu {
        id: desktopCtxMenu
        anchors.fill: parent
        desktopPath: root.desktopPath
        onMenuVisibleChanged: {
            if (!menuVisible && root.activeContextMenuKind === "desktop") {
                root.activeContextMenuKind = ""
            }
        }

        onNewFolder: root.createNewFolder()
        onNewTextFile: {
            if (root.fileManagerApiRef && root.fileManagerApiRef.createEmptyFile) {
                root.fileManagerApiRef.createEmptyFile(String(root.desktopPath || "~/Desktop") + "/New File.txt")
            }
        }
        onArrangeIcons: root.arrangeDesktopIcons()
        onSnapToGrid: root.snapDesktopToGrid()
        onCleanUpDesktop: root.cleanUpDesktop()
        onArrangeIconsBy: function(sortKey, descending) {
            root.arrangeDesktopIconsBy(sortKey, descending)
        }
        onOpenTerminal: root.openTerminalHere()
    }

    Menu {
        id: launcherShortcutMenu
        modal: false
        focus: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside | Popup.CloseOnPressOutsideParent
        onVisibleChanged: {
            if (!visible && root.activeContextMenuKind === "item") {
                root.activeContextMenuKind = ""
            }
        }
        MenuItem {
            text: "Open"
            onTriggered: {
                root._ensureContextSelectionForMenu()
                root.openSelection()
            }
        }
        MenuItem {
            text: "Rename"
            onTriggered: {
                root._ensureContextSelectionForMenu()
                root.renameSelection()
            }
        }
        MenuItem {
            text: "Add to Dock"
            enabled: root._isDesktopLauncherRow(root.contextEntryRow)
            visible: enabled
            height: visible ? implicitHeight : 0
            onTriggered: root.addSelectionToDock()
        }
        MenuSeparator {}
        MenuItem {
            text: "Move to Trash"
            onTriggered: {
                root._ensureContextSelectionForMenu()
                root.moveSelectionToTrash()
            }
        }
        MenuItem {
            text: "Delete Permanently"
            onTriggered: {
                root._ensureContextSelectionForMenu()
                root.deleteSelectionPermanently()
            }
        }
        MenuSeparator {}
        MenuItem {
            text: "Properties"
            onTriggered: {
                root._ensureContextSelectionForMenu()
                root.openPropertiesSelection()
            }
        }
    }

}
