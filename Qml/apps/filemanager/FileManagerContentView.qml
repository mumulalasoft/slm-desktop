import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import "FileManagerUtils.js" as FileManagerUtils

Rectangle {
    id: root
    property var fileModel: null
    property string searchText: ""
    property string viewMode: "grid"
    property int contentScalePercent: 100
    readonly property real contentScale: Math.max(0.75, Math.min(4.0, Number(contentScalePercent || 100) / 100.0))
    property int selectedIndex: -1
    property var selectedIndexes: []
    property int dndHoverIndex: -1
    property bool dndActive: false
    property var hostItem: null
    property bool suppressFlick: false
    property bool rubberSelecting: false
    property real rubberStartX: 0
    property real rubberStartY: 0
    property real rubberCurrentX: 0
    property real rubberCurrentY: 0
    readonly property int gridColumns: gridView.visible ? Math.max(1, gridView.computedColumns) : 1
    readonly property int bodyFontSize: themedFontSize("body", 13)
    readonly property int secondaryFontSize: themedFontSize("caption", 12)
    property var thumbnailCache: ({})
    property var thumbnailPending: ({})
    property var thumbnailRequestQueue: []
    property var thumbnailRequestQueuedMap: ({})
    property int thumbnailGeneration: 0
    property bool thumbnailsEnabled: false
    property bool contentLoading: false
    property bool trashView: false
    property var columnParentEntries: []
    property var columnChildEntries: []
    property int columnsFocusPane: 1 // 0: parent, 1: current, 2: child/preview
    property int columnParentIndex: -1
    property int columnChildIndex: -1
    property var columnsPreviewEntry: null
    property string columnsDisplayPath: ""
    readonly property bool emptyStateVisible: !!fileModel && Number(fileModel.count || 0) <= 0
    readonly property bool deepSearching: !!(fileModel && fileModel.deepSearchRunning)
    readonly property bool searching: String(searchText || "").trim().length > 0
    readonly property string currentPathText: String(fileModel && fileModel.currentPath ? fileModel.currentPath : "")
    readonly property bool recentView: String(currentPathText || "").trim() === "__recent__"
    readonly property string listSortKey: String(fileModel && fileModel.sortKey ? fileModel.sortKey : "dateModified")
    readonly property bool listSortDescending: !!(fileModel && fileModel.sortDescending)

    signal selectedIndexRequested(int index, int modifiers)
    signal contextMenuRequested(int index, real x, real y)
    signal activateRequested(int index)
    signal renameRequested(int index)
    signal clearSelectionRequested()
    signal backgroundContextRequested(real x, real y)
    signal dragStartRequested(int index, string path, string name, bool isDir, bool copyMode, real sceneX, real sceneY)
    signal dragMoveRequested(real sceneX, real sceneY, int hoverIndex, bool copyMode)
    signal dragEndRequested(int targetIndex)
    signal selectionRectRequested(var indexes, int modifiers, int anchorIndex)
    signal goUpRequested()
    signal clearSearchRequested()
    signal createFolderRequested()
    signal openHomeRequested()
    signal clearRecentsRequested()

    color: "transparent"
    border.width: Theme.borderWidthNone
    border.color: "transparent"

    function positionSelection(index, atBeginning) {
        if (index < 0) {
            return
        }
        if (viewMode === "list") {
            listView.positionViewAtIndex(index, atBeginning ? ListView.Beginning : ListView.Contain)
        } else if (viewMode === "columns") {
            columnsCurrentList.positionViewAtIndex(index, atBeginning ? ListView.Beginning : ListView.Contain)
        } else {
            gridView.positionViewAtIndex(index, atBeginning ? GridView.Beginning : GridView.Contain)
        }
    }

    function positionSelectionCenter(index) {
        if (index < 0) {
            return
        }
        if (viewMode === "list") {
            listView.positionViewAtIndex(index, ListView.Center)
        } else if (viewMode === "columns") {
            columnsCurrentList.positionViewAtIndex(index, ListView.Center)
        } else {
            gridView.positionViewAtIndex(index, GridView.Center)
        }
    }

    function isSelected(index) {
        return selectedIndexes && selectedIndexes.indexOf(index) >= 0
    }

    function iconUrl(name) {
        var base = String(name || "")
        var rev = ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                   ? ThemeIconController.revision : 0)
        var join = base.indexOf("?") >= 0 ? "&" : "?"
        return "image://themeicon/" + base + join + "v=" + rev
    }

    function preferredFileIcon(iconName, mimeType, isDir) {
        if (isDir) {
            return "folder"
        }
        var icon = String(iconName || "").trim()
        if (icon.length > 0 && icon !== "text-x-generic" && icon !== "text-x-generic-symbolic") {
            return icon
        }
        var mime = String(mimeType || "").trim()
        if (mime.length > 0 && mime.indexOf("/") > 0) {
            return mime.replace("/", "-")
        }
        return icon.length > 0 ? icon : "text-x-generic-symbolic"
    }

    function emptyStateTitle() {
        if (deepSearching) {
            return "Searching..."
        }
        if (searching) {
            return "No results found"
        }
        if (trashView) {
            return "Trash is empty"
        }
        if (recentView) {
            return "No recent files"
        }
        return "Folder is empty"
    }

    function emptyStateSubtitle() {
        if (deepSearching) {
            return "Please wait while results are collected."
        }
        if (searching) {
            return "Try another keyword or clear the search."
        }
        if (trashView) {
            return "Deleted items will appear here."
        }
        if (recentView) {
            return "Recently opened files will appear here."
        }
        return "Drop files here or create a new item."
    }

    function themedFontSize(token, fallback) {
        if (Theme.fontSize) {
            var value = Number(Theme.fontSize(token))
            if (value > 0) {
                return Math.round(value)
            }
        }
        return Number(fallback || 13)
    }

    function thumbnailKey(pathValue, pixelSize) {
        var raw = Math.max(64, Math.round(Number(pixelSize || 256)))
        var bucket = 128
        if (raw > 128 && raw <= 256) {
            bucket = 256
        } else if (raw > 256 && raw <= 512) {
            bucket = 512
        } else if (raw > 512) {
            bucket = 1024
        }
        return String(pathValue || "") + "|" + String(bucket)
    }

    function thumbnailCacheGet(pathValue, pixelSize) {
        var key = thumbnailKey(pathValue, pixelSize)
        return thumbnailCache[key] ? String(thumbnailCache[key]) : ""
    }

    function thumbnailCacheSet(pathValue, pixelSize, thumbPath) {
        var key = thumbnailKey(pathValue, pixelSize)
        var copy = Object.assign({}, thumbnailCache)
        copy[key] = String(thumbPath || "")
        thumbnailCache = copy
    }

    function thumbnailPendingHas(pathValue, pixelSize) {
        var key = thumbnailKey(pathValue, pixelSize)
        return !!thumbnailPending[key]
    }

    function thumbnailPendingSet(pathValue, pixelSize, active) {
        var key = thumbnailKey(pathValue, pixelSize)
        var copy = Object.assign({}, thumbnailPending)
        if (active) {
            copy[key] = true
        } else {
            delete copy[key]
        }
        thumbnailPending = copy
    }

    function requestThumbnailIfNeeded(pathValue, pixelSize, priorityValue) {
        var p = String(pathValue || "")
        var raw = Math.max(64, Math.round(Number(pixelSize || 256)))
        var s = 128
        if (raw > 128 && raw <= 256) {
            s = 256
        } else if (raw > 256 && raw <= 512) {
            s = 512
        } else if (raw > 512) {
            s = 1024
        }
        var prio = Number(priorityValue || 999999)
        if (p.length === 0) {
            return
        }
        if (thumbnailCacheGet(p, s).length > 0 || thumbnailPendingHas(p, s)) {
            return
        }
        if (typeof FileManagerApi === "undefined" || !FileManagerApi || !FileManagerApi.requestThumbnailAsync) {
            return
        }
        var key = thumbnailKey(p, s)
        if (thumbnailRequestQueuedMap[key]) {
            var existing = thumbnailRequestQueue ? thumbnailRequestQueue.slice(0) : []
            for (var i = 0; i < existing.length; ++i) {
                if (thumbnailKey(existing[i].path, existing[i].size) === key) {
                    existing[i].priority = Math.min(Number(existing[i].priority || 999999), prio)
                    break
                }
            }
            thumbnailRequestQueue = existing
            return
        }
        var queuedMap = Object.assign({}, thumbnailRequestQueuedMap)
        queuedMap[key] = true
        thumbnailRequestQueuedMap = queuedMap
        var queue = thumbnailRequestQueue ? thumbnailRequestQueue.slice(0) : []
        queue.push({ "path": p, "size": s, "priority": prio, "generation": root.thumbnailGeneration })
        thumbnailRequestQueue = queue
        if (!thumbnailRequestTimer.running) {
            thumbnailRequestTimer.start()
        }
    }

    function previewSource(pathValue, nameValue, isDirValue, modelThumbPath, pixelSize, nearViewport, allowThumb, priorityValue) {
        if (!thumbnailsEnabled || isDirValue || !nearViewport || !FileManagerUtils.isPreviewCandidateName(nameValue)) {
            return ""
        }
        var raw = Math.max(64, Math.round(Number(pixelSize || 256)))
        var s = 128
        if (raw > 128 && raw <= 256) {
            s = 256
        } else if (raw > 256 && raw <= 512) {
            s = 512
        } else if (raw > 512) {
            s = 1024
        }
        var modelThumb = String(modelThumbPath || "")
        if (modelThumb.length > 0) {
            return FileManagerUtils.toFileUrl(modelThumb)
        }
        var p = String(pathValue || "")
        var cached = thumbnailCacheGet(p, s)
        if (cached.length > 0) {
            return FileManagerUtils.toFileUrl(cached)
        }
        if (!allowThumb) {
            return ""
        }
        return ""
    }

    function ensureColumnsPreviewThumbnail() {
        if (!thumbnailsEnabled || !columnsPreviewEntry) {
            return
        }
        var entry = columnsPreviewEntry
        if (!entry || !!entry.isDir || !FileManagerUtils.isPreviewCandidateName(String(entry.name || ""))) {
            return
        }
        var pixelSize = Math.max(96, Math.round(Math.min(width, height) * 0.45))
        var modelThumb = String(entry.thumbnailPath || "")
        if (modelThumb.length > 0) {
            thumbnailCacheSet(String(entry.path || ""), pixelSize, modelThumb)
            return
        }
        requestThumbnailIfNeeded(String(entry.path || ""), pixelSize, 0)
    }

    function modelContainsPath(pathValue) {
        if (!fileModel || !fileModel.count || !fileModel.entryAt) {
            return false
        }
        var p = String(pathValue || "")
        var n = Number(fileModel.count || 0)
        for (var i = 0; i < n; ++i) {
            var e = fileModel.entryAt(i)
            if (e && e.ok && String(e.path || "") === p) {
                return true
            }
        }
        return false
    }

    function parentPathOf(pathValue) {
        var p = String(pathValue || "")
        if (p.length <= 1) {
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

    function readDirectoryEntries(pathValue) {
        var p = String(pathValue || "")
        if (p.length <= 0) {
            return []
        }
        if (typeof FileManagerApi === "undefined" || !FileManagerApi || !FileManagerApi.listDirectory) {
            return []
        }
        var res = FileManagerApi.listDirectory(p, false, true)
        if (!res || !res.ok || !res.entries) {
            return []
        }
        return res.entries
    }

    function refreshColumnsData() {
        if (viewMode !== "columns" || !fileModel) {
            return
        }
        var currentPath = String(fileModel.currentPath || "")
        var parentPath = parentPathOf(currentPath)
        columnParentEntries = parentPath.length > 0 ? readDirectoryEntries(parentPath) : []
        columnParentIndex = -1
        for (var i = 0; i < columnParentEntries.length; ++i) {
            var parentRow = columnParentEntries[i]
            if (String(parentRow.path || "") === currentPath) {
                columnParentIndex = i
                break
            }
        }

        var entry = (selectedIndex >= 0 && fileModel.entryAt) ? fileModel.entryAt(selectedIndex) : null
        if (entry && entry.ok && !!entry.isDir) {
            columnChildEntries = readDirectoryEntries(String(entry.path || ""))
            columnChildIndex = -1
            columnsPreviewEntry = null
        } else {
            columnChildEntries = []
            columnChildIndex = -1
            columnsPreviewEntry = (entry && entry.ok) ? entry : null
        }
        updateColumnsDisplayPath()
        ensureColumnsPreviewThumbnail()
    }

    function formatSize(sizeValue) {
        var size = Number(sizeValue || 0)
        if (!(size >= 0)) {
            return "Unknown"
        }
        var units = ["B", "KB", "MB", "GB", "TB"]
        var idx = 0
        while (size >= 1024 && idx < units.length - 1) {
            size = size / 1024.0
            idx += 1
        }
        return (size >= 10 || idx === 0 ? Math.round(size) : Math.round(size * 10) / 10) + " " + units[idx]
    }

    function formatDateCell(isoValue) {
        var raw = String(isoValue || "").trim()
        if (raw.length <= 0) {
            return "-"
        }
        var dt = new Date(raw)
        if (isNaN(dt.getTime())) {
            return raw
        }
        var now = new Date()
        var sameYear = dt.getFullYear() === now.getFullYear()
        return Qt.formatDateTime(dt, sameYear ? "dd MMM HH:mm" : "dd MMM yyyy")
    }

    function kindLabel(mimeType, isDir, suffixValue) {
        if (isDir) {
            return "Folder"
        }
        var mime = String(mimeType || "").trim()
        if (mime.length > 0 && mime.indexOf("/") > 0) {
            var tail = String(mime.split("/")[1] || "").replace(/[-_]/g, " ").trim()
            if (tail.length > 0) {
                return tail.charAt(0).toUpperCase() + tail.slice(1)
            }
        }
        var suffix = String(suffixValue || "").trim()
        if (suffix.length > 0) {
            return suffix.toUpperCase() + " File"
        }
        return "File"
    }

    function defaultSortDescendingForKey(key) {
        var k = String(key || "")
        if (k === "name" || k === "kind") {
            return false
        }
        return true
    }

    function requestListSort(key) {
        if (!fileModel || !fileModel.setSort) {
            return
        }
        var k = String(key || "dateModified")
        var nextDescending = defaultSortDescendingForKey(k)
        if (root.listSortKey === k) {
            nextDescending = !root.listSortDescending
        }
        fileModel.setSort(k, nextDescending)
    }

    function sortIndicatorFor(key) {
        if (String(key || "") !== root.listSortKey) {
            return ""
        }
        return root.listSortDescending ? " ▼" : " ▲"
    }

    function updateColumnsDisplayPath() {
        if (!fileModel) {
            columnsDisplayPath = ""
            return
        }
        var currentPath = String(fileModel.currentPath || "")
        if (viewMode !== "columns") {
            columnsDisplayPath = currentPath
            return
        }
        var path = currentPath
        if (columnsFocusPane === 0) {
            if (columnParentIndex >= 0 && columnParentIndex < columnParentEntries.length) {
                path = String(columnParentEntries[columnParentIndex].path || "")
            } else {
                path = parentPathOf(currentPath)
            }
        } else if (columnsFocusPane === 2) {
            if (columnsPreviewEntry && columnsPreviewEntry.ok) {
                path = String(columnsPreviewEntry.path || currentPath)
            } else if (columnChildIndex >= 0 && columnChildIndex < columnChildEntries.length) {
                path = String(columnChildEntries[columnChildIndex].path || currentPath)
            }
        }
        columnsDisplayPath = path.length > 0 ? path : currentPath
    }

    function columnsSelectParent(index, modifiers) {
        if (index < 0 || index >= columnParentEntries.length || !fileModel) {
            return false
        }
        var row = columnParentEntries[index]
        if (!row || !row.isDir) {
            return false
        }
        columnParentIndex = index
        columnsFocusPane = 0
        fileModel.currentPath = String(row.path || "")
        selectedIndex = -1
        selectedIndexRequested(-1, Number(modifiers || 0))
        return true
    }

    function columnsSelectChild(index, openDir) {
        if (index < 0 || index >= columnChildEntries.length || !fileModel) {
            return false
        }
        var row = columnChildEntries[index]
        if (!row) {
            return false
        }
        columnChildIndex = index
        columnsFocusPane = 2
        if (!!openDir && !!row.isDir) {
            fileModel.currentPath = String(row.path || "")
            selectedIndex = -1
            selectedIndexRequested(-1, 0)
        }
        updateColumnsDisplayPath()
        return true
    }

    function columnsHandleKey(key, modifiers) {
        if (viewMode !== "columns" || !fileModel) {
            return false
        }

        if (key === Qt.Key_Tab) {
            columnsFocusPane = (columnsFocusPane + 1) % 3
            updateColumnsDisplayPath()
            return true
        }

        if (key === Qt.Key_Left) {
            if (columnsFocusPane === 2) {
                columnsFocusPane = 1
                updateColumnsDisplayPath()
                return true
            }
            if (columnsFocusPane === 1) {
                columnsFocusPane = 0
                updateColumnsDisplayPath()
                return true
            }
            if (columnsFocusPane === 0 && fileModel.goUp) {
                fileModel.goUp()
                selectedIndex = -1
                selectedIndexRequested(-1, Number(modifiers || 0))
                updateColumnsDisplayPath()
                return true
            }
            return false
        }

        if (key === Qt.Key_Right) {
            if (columnsFocusPane === 0) {
                if (columnParentIndex >= 0) {
                    return columnsSelectParent(columnParentIndex, modifiers)
                }
                columnsFocusPane = 1
                updateColumnsDisplayPath()
                return true
            }
            if (columnsFocusPane === 1) {
                if (selectedIndex >= 0 && fileModel.entryAt) {
                    var selected = fileModel.entryAt(selectedIndex)
                    if (selected && selected.ok && selected.isDir) {
                        columnsFocusPane = 2
                        updateColumnsDisplayPath()
                        return true
                    }
                    if (selected && selected.ok && !selected.isDir) {
                        columnsFocusPane = 2
                        columnsPreviewEntry = selected
                        updateColumnsDisplayPath()
                        return true
                    }
                }
                return false
            }
            if (columnsFocusPane === 2) {
                return columnsSelectChild(columnChildIndex, true)
            }
            return false
        }

        if (key === Qt.Key_Up || key === Qt.Key_Down) {
            var step = key === Qt.Key_Down ? 1 : -1
            if (columnsFocusPane === 0) {
                var nextParent = Math.max(0, Math.min(columnParentEntries.length - 1, columnParentIndex + step))
                return columnsSelectParent(nextParent, modifiers)
            }
            if (columnsFocusPane === 1) {
                if (!fileModel.count || fileModel.count <= 0) {
                    return true
                }
                var nextCurrent = selectedIndex < 0 ? 0 : selectedIndex + step
                nextCurrent = Math.max(0, Math.min(Number(fileModel.count || 0) - 1, nextCurrent))
                selectedIndexRequested(nextCurrent, Number(modifiers || 0))
                columnsCurrentList.positionViewAtIndex(nextCurrent, ListView.Contain)
                columnsFocusPane = 1
                updateColumnsDisplayPath()
                return true
            }
            if (columnsFocusPane === 2 && !columnsPreviewEntry) {
                if (!columnChildEntries || columnChildEntries.length <= 0) {
                    return true
                }
                var nextChild = columnChildIndex < 0 ? 0 : columnChildIndex + step
                nextChild = Math.max(0, Math.min(columnChildEntries.length - 1, nextChild))
                columnChildIndex = nextChild
                columnsChildList.positionViewAtIndex(nextChild, ListView.Contain)
                updateColumnsDisplayPath()
                return true
            }
            return true
        }

        if (key === Qt.Key_Return || key === Qt.Key_Enter) {
            if (columnsFocusPane === 0) {
                return columnsSelectParent(columnParentIndex, modifiers)
            }
            if (columnsFocusPane === 1) {
                if (selectedIndex >= 0) {
                    activateRequested(selectedIndex)
                    return true
                }
                return false
            }
            if (columnsFocusPane === 2 && !columnsPreviewEntry) {
                return columnsSelectChild(columnChildIndex, true)
            }
            return true
        }

        return false
    }

    function activeView() {
        if (viewMode === "list") {
            return listView
        }
        if (viewMode === "columns") {
            return columnsCurrentList
        }
        return gridView
    }

    function visibleCount() {
        return fileModel ? Math.max(0, Number(fileModel.count || 0)) : 0
    }

    function rowHeightForListLike() {
        if (viewMode === "list") {
            return Math.max(24, Math.round(32 * root.contentScale) + Number(listView.spacing || 0))
        }
        return 32
    }

    function pageStepForCurrentView() {
        if (viewMode === "grid") {
            var rows = Math.max(1, Math.floor(Math.max(1, gridView.height) / Math.max(1, gridView.cellHeight)))
            return Math.max(1, rows * Math.max(1, root.gridColumns))
        }
        var rowsLike = Math.max(1, Math.floor(Math.max(1, root.height) / rowHeightForListLike()))
        return Math.max(1, rowsLike)
    }

    function listBucketKey(isoDateTime) {
        var bucket = FileManagerUtils.dayBucket(String(isoDateTime || ""))
        if (bucket === "today") return "today"
        if (bucket === "yesterday") return "yesterday"
        return "older"
    }

    function listBucketTitle(isoDateTime) {
        var key = listBucketKey(isoDateTime)
        if (key === "today") return "Today"
        if (key === "yesterday") return "Yesterday"
        return "Older"
    }

    function listShouldShowSectionHeader(indexValue, isoDateTime) {
        var idx = Number(indexValue || 0)
        if (idx <= 0 || !fileModel || !fileModel.entryAt) {
            return true
        }
        var prev = fileModel.entryAt(idx - 1)
        var prevIso = prev && prev.ok ? String(prev.lastModified || "") : ""
        return listBucketKey(prevIso) !== listBucketKey(isoDateTime)
    }

    function clampIndex(indexValue) {
        var count = visibleCount()
        if (count <= 0) {
            return -1
        }
        return Math.max(0, Math.min(count - 1, Number(indexValue || 0)))
    }

    function handleNavigationKey(key, modifiers) {
        if (viewMode === "columns") {
            return columnsHandleKey(key, modifiers)
        }
        var count = visibleCount()
        if (count <= 0) {
            return false
        }
        var current = Number(selectedIndex || -1)
        var hasSelection = current >= 0 && current < count
        var next = hasSelection ? current : 0
        var handled = true

        if (viewMode === "list") {
            if (key === Qt.Key_Right && hasSelection) {
                activateRequested(current)
                return true
            }
            if (key === Qt.Key_Left && !(modifiers & Qt.AltModifier) && !(modifiers & Qt.ControlModifier) && !(modifiers & Qt.MetaModifier)) {
                goUpRequested()
                return true
            }
        }

        switch (key) {
        case Qt.Key_Up:
            next = hasSelection ? (current - (viewMode === "grid" ? root.gridColumns : 1)) : 0
            break
        case Qt.Key_Down:
            next = hasSelection ? (current + (viewMode === "grid" ? root.gridColumns : 1)) : 0
            break
        case Qt.Key_Left:
            if (viewMode === "grid") {
                next = hasSelection ? (current - 1) : 0
            } else {
                handled = false
            }
            break
        case Qt.Key_Right:
            if (viewMode === "grid") {
                next = hasSelection ? (current + 1) : 0
            } else {
                handled = false
            }
            break
        case Qt.Key_Home:
            next = 0
            break
        case Qt.Key_End:
            next = count - 1
            break
        case Qt.Key_PageUp:
            next = hasSelection ? (current - pageStepForCurrentView()) : 0
            break
        case Qt.Key_PageDown:
            next = hasSelection ? (current + pageStepForCurrentView()) : (count - 1)
            break
        default:
            handled = false
            break
        }

        if (!handled) {
            return false
        }

        next = clampIndex(next)
        if (next < 0) {
            return true
        }
        selectedIndexRequested(next, Number(modifiers || 0))
        positionSelection(next, false)
        return true
    }

    function indexAtPoint(x, y) {
        var v = activeView()
        if (!v || !v.contentItem || !v.indexAt) {
            return -1
        }
        var p = root.mapToItem(v.contentItem, x, y)
        return v.indexAt(p.x, p.y)
    }

    function currentRubberRect() {
        var x1 = Math.min(rubberStartX, rubberCurrentX)
        var y1 = Math.min(rubberStartY, rubberCurrentY)
        var x2 = Math.max(rubberStartX, rubberCurrentX)
        var y2 = Math.max(rubberStartY, rubberCurrentY)
        return { "x": x1, "y": y1, "w": Math.max(1, x2 - x1), "h": Math.max(1, y2 - y1) }
    }

    function rectsIntersect(a, b) {
        return a.x < (b.x + b.w) && (a.x + a.w) > b.x &&
               a.y < (b.y + b.h) && (a.y + a.h) > b.y
    }

    function collectRubberSelection() {
        var model = fileModel
        var v = activeView()
        if (!model || !v || !v.itemAtIndex) {
            return []
        }
        var rect = currentRubberRect()
        var out = []
        var count = Number(model.count || 0)
        for (var i = 0; i < count; ++i) {
            var item = v.itemAtIndex(i)
            if (!item) {
                continue
            }
            var p = item.mapToItem(root, 0, 0)
            var itemRect = { "x": p.x, "y": p.y, "w": item.width, "h": item.height }
            if (rectsIntersect(rect, itemRect)) {
                out.push(i)
            }
        }
        return out
    }

    Connections {
        target: (typeof FileManagerApi !== "undefined") ? FileManagerApi : null
        ignoreUnknownSignals: true
        function onThumbnailReady(path, size, thumbnailPath, ok, error) {
            var p = String(path || "")
            var s = Math.max(64, Math.round(Number(size || 256)))
            root.thumbnailPendingSet(p, s, false)
            if (!root.modelContainsPath(p)) {
                return
            }
            if (ok && String(thumbnailPath || "").length > 0) {
                root.thumbnailCacheSet(p, s, String(thumbnailPath || ""))
            }
        }
    }

    Connections {
        target: root.fileModel
        ignoreUnknownSignals: true
        function onCurrentPathChanged() {
            root.thumbnailGeneration = root.thumbnailGeneration + 1
            root.thumbnailCache = ({})
            root.thumbnailPending = ({})
            root.thumbnailRequestQueue = []
            root.thumbnailRequestQueuedMap = ({})
            thumbnailRequestTimer.stop()
            root.refreshColumnsData()
        }
    }

    onSelectedIndexChanged: refreshColumnsData()
    onViewModeChanged: refreshColumnsData()
    onColumnsPreviewEntryChanged: ensureColumnsPreviewThumbnail()
    onColumnsFocusPaneChanged: updateColumnsDisplayPath()
    onColumnParentIndexChanged: updateColumnsDisplayPath()
    onColumnChildIndexChanged: updateColumnsDisplayPath()

    Timer {
        id: thumbnailRequestTimer
        interval: 80
        repeat: true
        running: false
        onTriggered: {
            if (!root.thumbnailsEnabled) {
                stop()
                root.thumbnailRequestQueue = []
                root.thumbnailRequestQueuedMap = ({})
                return
            }
            var processed = 0
            var queue = root.thumbnailRequestQueue ? root.thumbnailRequestQueue.slice(0) : []
            queue.sort(function(a, b) {
                var pa = Number(a.priority || 999999)
                var pb = Number(b.priority || 999999)
                return pa - pb
            })
            var queuedMap = Object.assign({}, root.thumbnailRequestQueuedMap)
            while (queue.length > 0 && processed < 6) {
                var job = queue.shift()
                if (Number(job.generation || 0) !== root.thumbnailGeneration) {
                    continue
                }
                var p = String(job.path || "")
                var s = Math.max(64, Math.round(Number(job.size || 256)))
                var key = root.thumbnailKey(p, s)
                delete queuedMap[key]
                root.thumbnailPendingSet(p, s, true)
                var res = FileManagerApi.requestThumbnailAsync(p, s)
                if (!(res && res.ok)) {
                    root.thumbnailPendingSet(p, s, false)
                }
                processed += 1
            }
            root.thumbnailRequestQueue = queue
            root.thumbnailRequestQueuedMap = queuedMap
            if (queue.length <= 0) {
                stop()
            }
        }
    }

    function updateRubberSelection(modifiers) {
        var indexes = collectRubberSelection()
        var anchor = indexes.length > 0 ? Number(indexes[0]) : -1
        selectionRectRequested(indexes, Number(modifiers || 0), anchor)
    }

    MouseArea {
        id: backgroundMouse
        anchors.fill: parent
        acceptedButtons: Qt.RightButton | Qt.LeftButton
        propagateComposedEvents: true
        property bool dragSelecting: false
        property bool pendingRubber: false
        property real pressX: 0
        property real pressY: 0
        property int pressModifiers: 0
        onPressed: function(mouse) {
            if (mouse.button === Qt.LeftButton) {
                var hitIndex = root.indexAtPoint(mouse.x, mouse.y)
                if (hitIndex >= 0) {
                    pendingRubber = false
                    dragSelecting = false
                    mouse.accepted = false
                    return
                }
                pendingRubber = true
                dragSelecting = false
                pressX = mouse.x
                pressY = mouse.y
                pressModifiers = mouse.modifiers
                mouse.accepted = true
                return
            }
            if (mouse.button === Qt.RightButton) {
                pendingRubber = false
                dragSelecting = false
                root.rubberSelecting = false
                root.suppressFlick = false
                root.clearSelectionRequested()
                root.backgroundContextRequested(mouse.x, mouse.y)
                mouse.accepted = true
            }
        }
        onPositionChanged: function(mouse) {
            if (!(mouse.buttons & Qt.LeftButton)) {
                return
            }
            if (pendingRubber && !dragSelecting) {
                var dx = mouse.x - pressX
                var dy = mouse.y - pressY
                if ((dx * dx + dy * dy) >= 49) {
                    pendingRubber = false
                    dragSelecting = true
                    root.rubberSelecting = true
                    root.suppressFlick = true
                    root.rubberStartX = pressX
                    root.rubberStartY = pressY
                    root.rubberCurrentX = mouse.x
                    root.rubberCurrentY = mouse.y
                    if ((pressModifiers & Qt.ControlModifier) === 0) {
                        root.clearSelectionRequested()
                    }
                    root.updateRubberSelection(pressModifiers)
                }
            }
            if (!dragSelecting) {
                return
            }
            root.rubberCurrentX = mouse.x
            root.rubberCurrentY = mouse.y
            root.updateRubberSelection(mouse.modifiers)
            mouse.accepted = true
        }
        onReleased: function(mouse) {
            if (pendingRubber) {
                pendingRubber = false
                dragSelecting = false
                if ((pressModifiers & Qt.ControlModifier) === 0) {
                    root.clearSelectionRequested()
                }
                mouse.accepted = true
                return
            }
            if (!dragSelecting) {
                return
            }
            root.rubberCurrentX = mouse.x
            root.rubberCurrentY = mouse.y
            root.updateRubberSelection(mouse.modifiers)
            dragSelecting = false
            root.rubberSelecting = false
            root.suppressFlick = false
            mouse.accepted = true
        }
        onCanceled: {
            pendingRubber = false
            dragSelecting = false
            root.rubberSelecting = false
            root.suppressFlick = false
        }
    }

    Rectangle {
        visible: root.rubberSelecting
        z: 99
        x: root.currentRubberRect().x
        y: root.currentRubberRect().y
        width: root.currentRubberRect().w
        height: root.currentRubberRect().h
        color: Theme.color("fileManagerRubberBand")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("selectedItem")
    }

    Item {
        anchors.fill: parent

        GridView {
            id: gridView
            anchors.fill: parent
            anchors.leftMargin: 0
            anchors.rightMargin: 0
            anchors.topMargin: 0
            anchors.bottomMargin: 0
            model: fileModel
            visible: root.viewMode === "grid"
            clip: true
            property int minCellWidth: Math.round(132 * root.contentScale)
            property int targetCellWidth: Math.round(156 * root.contentScale)
            property int computedColumns: Math.max(1, Math.floor(width / targetCellWidth))
            cellWidth: Math.max(minCellWidth, Math.floor(width / computedColumns))
            cellHeight: cellWidth + Math.round(30 * root.contentScale)
            boundsBehavior: Flickable.StopAtBounds
            boundsMovement: Flickable.StopAtBounds
            flickableDirection: Flickable.VerticalFlick
            maximumFlickVelocity: 5800
            flickDeceleration: 3800
            reuseItems: true
            cacheBuffer: 800
            interactive: visible && !root.suppressFlick && contentHeight > height
            highlightMoveDuration: 80
            add: Transition {
                NumberAnimation { properties: "x,y"; duration: 145; easing.type: Easing.OutBack; easing.overshoot: 1.15 }
                NumberAnimation { properties: "opacity"; from: 0; to: 1; duration: 130; easing.type: Easing.OutCubic }
                NumberAnimation { properties: "scale"; from: 0.84; to: 1.0; duration: 145; easing.type: Easing.OutBack; easing.overshoot: 1.12 }
            }
            addDisplaced: Transition {
                NumberAnimation { properties: "x,y"; duration: 135; easing.type: Easing.OutCubic }
            }
            remove: Transition {
                NumberAnimation { properties: "opacity"; from: 1; to: 0; duration: 125; easing.type: Easing.InCubic }
                NumberAnimation { properties: "scale"; from: 1.0; to: 0.78; duration: 125; easing.type: Easing.InCubic }
            }
            removeDisplaced: Transition {
                NumberAnimation { properties: "x,y"; duration: 135; easing.type: Easing.OutCubic }
            }

            delegate: Item {
                id: rowItem
                required property int index
                required property string name
                required property string path
                required property string thumbnailPath
                required property string mimeType
                required property string iconName
                required property bool isDir
                required property int size
                required property string lastModified
                readonly property bool previewCandidate: (!isDir && FileManagerUtils.isPreviewCandidateName(name))
                readonly property real rowCenterY: y + (height * 0.5)
                readonly property real viewportCenterY: gridView.contentY + (gridView.height * 0.5)
                readonly property bool nearViewport: Math.abs(rowCenterY - viewportCenterY) <= (gridView.height * 1.35)
                readonly property bool allowThumb: !gridView.moving && !gridView.flicking
                readonly property real thumbSide: Math.max(72 * root.contentScale, Math.min(width * 0.78, height - (34 * root.contentScale)))

                width: gridView.cellWidth - 12
                height: gridView.cellHeight - 8

                function ensureGridThumbnail() {
                    if (!root.thumbnailsEnabled || isDir || !previewCandidate || !nearViewport || !allowThumb) {
                        return
                    }
                    if (String(thumbnailPath || "").length > 0) {
                        root.thumbnailCacheSet(path, thumbSide, thumbnailPath)
                        return
                    }
                    root.requestThumbnailIfNeeded(path, thumbSide, Math.abs(rowCenterY - viewportCenterY))
                }

                onThumbnailPathChanged: ensureGridThumbnail()
                onNearViewportChanged: ensureGridThumbnail()
                onAllowThumbChanged: ensureGridThumbnail()
                onThumbSideChanged: ensureGridThumbnail()
                Component.onCompleted: ensureGridThumbnail()

                Column {
                    anchors.centerIn: parent
                    spacing: 3
                    width: parent.width

                    Item {
                        width: rowItem.thumbSide
                        height: rowItem.thumbSide
                        anchors.horizontalCenter: parent.horizontalCenter

                        Image {
                            id: gridThumbImage
                            anchors.fill: parent
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            cache: true
                            opacity: status === Image.Ready ? 1.0 : 0.0
                            Behavior on opacity {
                                NumberAnimation { duration: 130; easing.type: Easing.OutCubic }
                            }
                            source: root.previewSource(path, name, isDir, thumbnailPath, rowItem.thumbSide, nearViewport, allowThumb,
                                                       Math.abs(rowCenterY - viewportCenterY))
                        }
                        Image {
                            anchors.fill: parent
                            visible: !(gridThumbImage.status === Image.Ready)
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                            cache: true
                            source: root.iconUrl(root.preferredFileIcon(iconName, mimeType, isDir))
                        }
                    }

                    Item {
                        id: gridLabelWrap
                        width: parent.width - 4
                        height: Math.max(16, gridNameText.paintedHeight)
                        anchors.horizontalCenter: parent.horizontalCenter

                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.topMargin: -2
                            width: Math.min(parent.width - 8, gridNameText.paintedWidth + 12)
                            height: Math.max(18, gridNameText.paintedHeight + 4)
                            radius: Theme.radiusMdPlus
                            visible: root.isSelected(index)
                            color: Theme.color("selectedItem")
                        }

                        Text {
                            id: gridNameText
                            text: name
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            color: root.isSelected(index) ? Theme.color("selectedItemText") : Theme.color("textPrimary")
                            font.pixelSize: root.bodyFontSize
                            font.weight: Theme.fontWeight("normal")
                            wrapMode: Text.Wrap
                            lineHeightMode: Text.ProportionalHeight
                            lineHeight: Theme.lineHeight("tight")
                            maximumLineCount: 2
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignTop
                        }
                    }

                }

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: function(m) {
                        var canDeferredRename =
                                m.button === Qt.LeftButton &&
                                m.modifiers === Qt.NoModifier &&
                                root.selectedIndex === index &&
                                root.selectedIndexes &&
                                root.selectedIndexes.length === 1
                        if (m.button === Qt.RightButton) {
                            if (!root.isSelected(index)) {
                                root.selectedIndexRequested(index, Qt.NoModifier)
                            }
                            renameClickTimer.stop()
                            var p = rowItem.mapToItem(root, m.x, m.y)
                            root.contextMenuRequested(index, p.x, p.y)
                            return
                        }
                        root.selectedIndexRequested(index, m.modifiers)
                        if (canDeferredRename) {
                            renameClickTimer.restart()
                        } else {
                            renameClickTimer.stop()
                        }
                    }
                    onPressed: function(m) {
                        dragStartX = m.x
                        dragStartY = m.y
                        dragStarted = false
                    }
                    property real dragStartX: 0
                    property real dragStartY: 0
                    property bool dragStarted: false
                    onPositionChanged: function(m) {
                        if (!(m.buttons & Qt.LeftButton)) {
                            return
                        }
                        var dx = m.x - dragStartX
                        var dy = m.y - dragStartY
                        var dist2 = dx * dx + dy * dy
                        if (!dragStarted && dist2 > 81) {
                            renameClickTimer.stop()
                            dragStarted = true
                            var scenePos = rowItem.mapToItem(root.hostItem ? root.hostItem : root, m.x, m.y)
                            var contentPos = rowItem.mapToItem(gridView.contentItem, m.x, m.y)
                            var copyMode = !!(m.modifiers & Qt.ControlModifier)
                            var hoverIdx = gridView.indexAt(contentPos.x, contentPos.y)
                            root.dragStartRequested(index, path, name, isDir, copyMode, scenePos.x, scenePos.y)
                            root.dragMoveRequested(scenePos.x, scenePos.y, hoverIdx, copyMode)
                            return
                        }
                        if (dragStarted && root.dndActive) {
                            var scenePos2 = rowItem.mapToItem(root.hostItem ? root.hostItem : root, m.x, m.y)
                            var contentPos2 = rowItem.mapToItem(gridView.contentItem, m.x, m.y)
                            var copyMode2 = !!(m.modifiers & Qt.ControlModifier)
                            var hoverIdx2 = gridView.indexAt(contentPos2.x, contentPos2.y)
                            root.dragMoveRequested(scenePos2.x, scenePos2.y, hoverIdx2, copyMode2)
                        }
                    }
                    onReleased: function(m) {
                        if (dragStarted && root.dndActive) {
                            var contentPos = rowItem.mapToItem(gridView.contentItem, m.x, m.y)
                            var targetIdx = gridView.indexAt(contentPos.x, contentPos.y)
                            root.dragEndRequested(targetIdx)
                        }
                        dragStarted = false
                    }
                    onDoubleClicked: {
                        renameClickTimer.stop()
                        if (root.dndActive) {
                            return
                        }
                        root.selectedIndexRequested(index, 0)
                        root.activateRequested(index)
                    }
                }

                Timer {
                    id: renameClickTimer
                    interval: 540
                    repeat: false
                    onTriggered: root.renameRequested(index)
                }
            }
        }

        Item {
            id: listModePane
            anchors.fill: parent
            visible: root.viewMode === "list"
            readonly property int headerHeight: 28
            readonly property int rowHeight: Math.round(30 * root.contentScale)
            readonly property int hPadding: 9
            readonly property int colSpacing: 8
            readonly property real colNameWidth: Math.max(210, Math.round(width * 0.38))
            readonly property real colSizeWidth: 90
            readonly property real colKindWidth: Math.max(120, Math.round(width * 0.16))
            readonly property real colDateAddedWidth: Math.max(140, Math.round(width * 0.18))
            readonly property real colDateModifiedWidth: Math.max(150, width - (hPadding * 2) - colNameWidth - colSizeWidth - colKindWidth - colDateAddedWidth - (colSpacing * 4))

            Rectangle {
                id: listHeader
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: listModePane.headerHeight
                color: "transparent"

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: listModePane.hPadding
                    anchors.rightMargin: listModePane.hPadding
                    spacing: listModePane.colSpacing

                    Text {
                        width: listModePane.colNameWidth
                        text: "Name" + root.sortIndicatorFor("name")
                        font.pixelSize: root.secondaryFontSize
                        font.weight: Theme.fontWeight("semibold")
                        color: Theme.color("textSecondary")
                        verticalAlignment: Text.AlignVCenter
                    }
                    Text {
                        width: listModePane.colSizeWidth
                        text: "Size" + root.sortIndicatorFor("size")
                        font.pixelSize: root.secondaryFontSize
                        font.weight: Theme.fontWeight("semibold")
                        color: Theme.color("textSecondary")
                        verticalAlignment: Text.AlignVCenter
                    }
                    Text {
                        width: listModePane.colKindWidth
                        text: "Kind" + root.sortIndicatorFor("kind")
                        font.pixelSize: root.secondaryFontSize
                        font.weight: Theme.fontWeight("semibold")
                        color: Theme.color("textSecondary")
                        verticalAlignment: Text.AlignVCenter
                    }
                    Text {
                        width: listModePane.colDateAddedWidth
                        text: "Date Added" + root.sortIndicatorFor("dateAdded")
                        font.pixelSize: root.secondaryFontSize
                        font.weight: Theme.fontWeight("semibold")
                        color: Theme.color("textSecondary")
                        verticalAlignment: Text.AlignVCenter
                    }
                    Text {
                        width: listModePane.colDateModifiedWidth
                        text: "Date Modified" + root.sortIndicatorFor("dateModified")
                        font.pixelSize: root.secondaryFontSize
                        font.weight: Theme.fontWeight("semibold")
                        color: Theme.color("textSecondary")
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: listModePane.hPadding
                    anchors.rightMargin: listModePane.hPadding
                    spacing: listModePane.colSpacing
                    z: 2

                    MouseArea { width: listModePane.colNameWidth; height: parent.height; onClicked: root.requestListSort("name") }
                    MouseArea { width: listModePane.colSizeWidth; height: parent.height; onClicked: root.requestListSort("size") }
                    MouseArea { width: listModePane.colKindWidth; height: parent.height; onClicked: root.requestListSort("kind") }
                    MouseArea { width: listModePane.colDateAddedWidth; height: parent.height; onClicked: root.requestListSort("dateAdded") }
                    MouseArea { width: listModePane.colDateModifiedWidth; height: parent.height; onClicked: root.requestListSort("dateModified") }
                }
            }

            ListView {
                id: listView
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: listHeader.bottom
                anchors.bottom: parent.bottom
                model: fileModel
                clip: true
                spacing: 0
                boundsBehavior: Flickable.StopAtBounds
                boundsMovement: Flickable.StopAtBounds
                flickableDirection: Flickable.VerticalFlick
                maximumFlickVelocity: 5800
                flickDeceleration: 3800
                reuseItems: true
                cacheBuffer: 800
                interactive: visible && !root.suppressFlick && contentHeight > height

                delegate: Item {
                    id: rowSimpleItem
                    required property int index
                    required property string name
                    required property string path
                    required property string thumbnailPath
                    required property string mimeType
                    required property string iconName
                    required property bool isDir
                    required property int size
                    required property string suffix
                    required property string dateAdded
                    required property string lastModified
                    readonly property bool previewCandidate: (!isDir && FileManagerUtils.isPreviewCandidateName(name))
                    readonly property real iconSide: Math.max(16, Math.round(18 * root.contentScale))
                    width: listView.width
                    height: listModePane.rowHeight

                    function ensureListThumbnail() {
                        if (!root.thumbnailsEnabled || !previewCandidate) {
                            return
                        }
                        if (String(thumbnailPath || "").length > 0) {
                            root.thumbnailCacheSet(path, iconSide * 2, thumbnailPath)
                            return
                        }
                        root.requestThumbnailIfNeeded(path, iconSide * 2, Math.abs(y - listView.contentY))
                    }

                    onThumbnailPathChanged: ensureListThumbnail()
                    onIconSideChanged: ensureListThumbnail()
                    onVisibleChanged: ensureListThumbnail()
                    Component.onCompleted: ensureListThumbnail()

                    Rectangle {
                        anchors.fill: parent
                        radius: Theme.radiusMd
                        border.width: Theme.borderWidthNone
                        border.color: "transparent"
                        color: root.isSelected(index)
                               ? Theme.color("selectedItem")
                               : (rowSimpleMouse.containsMouse
                                  ? Theme.color("hoverItem")
                                  : ((index % 2) === 1 ? Theme.color("fileManagerRowAlt") : "transparent"))
                        Behavior on color {
                            ColorAnimation { duration: 100; easing.type: Easing.OutCubic }
                        }
                    }

                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: listModePane.hPadding
                        anchors.rightMargin: listModePane.hPadding
                        spacing: listModePane.colSpacing

                        Row {
                            width: listModePane.colNameWidth
                            height: parent.height
                            spacing: 7

                            Item {
                                width: rowSimpleItem.iconSide
                                height: rowSimpleItem.iconSide
                                anchors.verticalCenter: parent.verticalCenter

                                Image {
                                    id: listThumbImage
                                    anchors.fill: parent
                                    source: root.previewSource(path, name, isDir, thumbnailPath, rowSimpleItem.iconSide * 2,
                                                               true, !(listView.moving || listView.flicking), Math.abs(rowSimpleItem.y - listView.contentY))
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    cache: true
                                    opacity: status === Image.Ready ? 1.0 : 0.0
                                    Behavior on opacity {
                                        NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
                                    }
                                }

                                Image {
                                    anchors.fill: parent
                                    visible: !(listThumbImage.status === Image.Ready)
                                    source: root.iconUrl(root.preferredFileIcon(iconName, mimeType, isDir))
                                    fillMode: Image.PreserveAspectFit
                                    asynchronous: true
                                    cache: true
                                }
                            }

                            Text {
                                width: listModePane.colNameWidth - rowSimpleItem.iconSide - 7
                                anchors.verticalCenter: parent.verticalCenter
                                text: name
                                color: root.isSelected(index) ? Theme.color("selectedItemText") : Theme.color("textPrimary")
                                font.pixelSize: root.bodyFontSize
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        Text {
                            width: listModePane.colSizeWidth
                            anchors.verticalCenter: parent.verticalCenter
                            text: isDir ? "--" : root.formatSize(size)
                            color: root.isSelected(index) ? Theme.color("selectedItemText") : Theme.color("textSecondary")
                            font.pixelSize: root.secondaryFontSize
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }

                        Text {
                            width: listModePane.colKindWidth
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.kindLabel(mimeType, isDir, suffix)
                            color: root.isSelected(index) ? Theme.color("selectedItemText") : Theme.color("textSecondary")
                            font.pixelSize: root.secondaryFontSize
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }

                        Text {
                            width: listModePane.colDateAddedWidth
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.formatDateCell(dateAdded)
                            color: root.isSelected(index) ? Theme.color("selectedItemText") : Theme.color("textSecondary")
                            font.pixelSize: root.secondaryFontSize
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }

                        Text {
                            width: listModePane.colDateModifiedWidth
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.formatDateCell(lastModified)
                            color: root.isSelected(index) ? Theme.color("selectedItemText") : Theme.color("textSecondary")
                            font.pixelSize: root.secondaryFontSize
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea {
                        id: rowSimpleMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: function(m) {
                            var canDeferredRename =
                                    m.button === Qt.LeftButton &&
                                    m.modifiers === Qt.NoModifier &&
                                    root.selectedIndex === index &&
                                    root.selectedIndexes &&
                                    root.selectedIndexes.length === 1
                            if (m.button === Qt.RightButton) {
                                if (!root.isSelected(index)) {
                                    root.selectedIndexRequested(index, Qt.NoModifier)
                                }
                                renameClickTimerListSimple.stop()
                                var p = rowSimpleItem.mapToItem(root, m.x, m.y)
                                root.contextMenuRequested(index, p.x, p.y)
                                return
                            }
                            root.selectedIndexRequested(index, m.modifiers)
                            if (canDeferredRename) {
                                renameClickTimerListSimple.restart()
                            } else {
                                renameClickTimerListSimple.stop()
                            }
                        }
                        onPressed: function(m) {
                            dragStartX = m.x
                            dragStartY = m.y
                            dragStarted = false
                        }
                        property real dragStartX: 0
                        property real dragStartY: 0
                        property bool dragStarted: false
                        onPositionChanged: function(m) {
                            if (!(m.buttons & Qt.LeftButton)) {
                                return
                            }
                            var dx = m.x - dragStartX
                            var dy = m.y - dragStartY
                            var dist2 = dx * dx + dy * dy
                            if (!dragStarted && dist2 > 81) {
                                renameClickTimerListSimple.stop()
                                dragStarted = true
                                var scenePos = rowSimpleItem.mapToItem(root.hostItem ? root.hostItem : root, m.x, m.y)
                                var contentPos = rowSimpleItem.mapToItem(listView.contentItem, m.x, m.y)
                                var copyMode = !!(m.modifiers & Qt.ControlModifier)
                                var hoverIdx = listView.indexAt(contentPos.x, contentPos.y)
                                root.dragStartRequested(index, path, name, isDir, copyMode, scenePos.x, scenePos.y)
                                root.dragMoveRequested(scenePos.x, scenePos.y, hoverIdx, copyMode)
                                return
                            }
                            if (dragStarted && root.dndActive) {
                                var scenePos2 = rowSimpleItem.mapToItem(root.hostItem ? root.hostItem : root, m.x, m.y)
                                var contentPos2 = rowSimpleItem.mapToItem(listView.contentItem, m.x, m.y)
                                var copyMode2 = !!(m.modifiers & Qt.ControlModifier)
                                var hoverIdx2 = listView.indexAt(contentPos2.x, contentPos2.y)
                                root.dragMoveRequested(scenePos2.x, scenePos2.y, hoverIdx2, copyMode2)
                            }
                        }
                        onReleased: function(m) {
                            if (dragStarted && root.dndActive) {
                                var contentPos = rowSimpleItem.mapToItem(listView.contentItem, m.x, m.y)
                                var targetIdx = listView.indexAt(contentPos.x, contentPos.y)
                                root.dragEndRequested(targetIdx)
                            }
                            dragStarted = false
                        }
                        onDoubleClicked: {
                            renameClickTimerListSimple.stop()
                            root.selectedIndexRequested(index, 0)
                            root.activateRequested(index)
                        }
                    }

                    Timer {
                        id: renameClickTimerListSimple
                        interval: 540
                        repeat: false
                        onTriggered: root.renameRequested(index)
                    }
                }
            }
        }

        Item {
            id: columnsMode
            visible: root.viewMode === "columns"
            anchors.fill: parent
            anchors.leftMargin: 0
            anchors.rightMargin: 0
            anchors.topMargin: 0
            anchors.bottomMargin: 0

            RowLayout {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "transparent"
                    border.width: Theme.borderWidthNone
                    border.color: "transparent"

                    ListView {
                        id: columnsParentList
                        anchors.fill: parent
                        clip: true
                        model: root.columnParentEntries
                        delegate: Item {
                            required property int index
                            required property var modelData
                            readonly property string name: String((modelData && modelData.name) ? modelData.name : "")
                            readonly property string path: String((modelData && modelData.path) ? modelData.path : "")
                            readonly property bool isDir: !!(modelData && modelData.isDir)
                            readonly property string mimeType: String((modelData && modelData.mimeType) ? modelData.mimeType : "")
                            readonly property string iconName: String((modelData && modelData.iconName) ? modelData.iconName : "")
                            readonly property bool selected: root.columnParentIndex === index && root.columnsFocusPane === 0
                            width: columnsParentList.width
                            height: 32

                            Rectangle {
                                anchors.fill: parent
                                border.width: Theme.borderWidthNone
                                border.color: "transparent"
                                color: selected ? Theme.color("selectedItem")
                                                : (parentMouse.containsMouse ? Theme.color("hoverItem") : "transparent")
                                Behavior on color {
                                    ColorAnimation { duration: 100; easing.type: Easing.OutCubic }
                                }
                            }

                            Row {
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 7
                                spacing: 6
                                Image {
                                    width: 16
                                    height: 16
                                    source: root.iconUrl(root.preferredFileIcon(iconName, mimeType, isDir))
                                    fillMode: Image.PreserveAspectFit
                                }
                                Text {
                                    text: name
                                    color: selected ? Theme.color("selectedItemText") : Theme.color("textPrimary")
                                    font.pixelSize: root.bodyFontSize
                                    elide: Text.ElideRight
                                    width: columnsParentList.width - (isDir ? 58 : 40)
                                }

                                Image {
                                    visible: isDir
                                    width: 11
                                    height: 11
                                    source: root.iconUrl("go-next-symbolic")
                                    fillMode: Image.PreserveAspectFit
                                    asynchronous: true
                                    cache: true
                                    opacity: selected ? 0.95 : 0.7
                                    Behavior on opacity {
                                        NumberAnimation { duration: 100; easing.type: Easing.OutCubic }
                                    }
                                }
                            }

                            MouseArea {
                                id: parentMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    root.columnsSelectParent(index, 0)
                                }
                            }
                        }
                    }
                }

                Item { Layout.preferredWidth: 0; Layout.fillHeight: true }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "transparent"
                    border.width: Theme.borderWidthNone
                    border.color: "transparent"

                    ListView {
                        id: columnsCurrentList
                        anchors.fill: parent
                        clip: true
                        model: fileModel
                        delegate: Item {
                            required property int index
                            required property string name
                            required property string path
                            required property string thumbnailPath
                            required property string mimeType
                            required property bool isDir
                            required property string iconName
                            readonly property bool previewCandidate: (!isDir && FileManagerUtils.isPreviewCandidateName(name))
                            width: columnsCurrentList.width
                            height: 32

                function ensureColumnsRowThumbnail() {
                                if (!root.thumbnailsEnabled || !previewCandidate) {
                                    return
                                }
                                if (String(thumbnailPath || "").length > 0) {
                                    root.thumbnailCacheSet(path, 48, thumbnailPath)
                                    return
                                }
                                root.requestThumbnailIfNeeded(path, 48, Math.abs(y - columnsCurrentList.contentY))
                            }

                            onThumbnailPathChanged: ensureColumnsRowThumbnail()
                            onVisibleChanged: ensureColumnsRowThumbnail()
                            Component.onCompleted: ensureColumnsRowThumbnail()

                            Rectangle {
                                anchors.fill: parent
                                border.width: Theme.borderWidthNone
                                border.color: "transparent"
                                color: root.isSelected(index) ? Theme.color("selectedItem")
                                                               : (currentMouse.containsMouse ? Theme.color("hoverItem") : "transparent")
                                Behavior on color {
                                    ColorAnimation { duration: 100; easing.type: Easing.OutCubic }
                                }
                            }
                            Row {
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 7
                                spacing: 6
                                Item {
                                    width: 16
                                    height: 16
                                    Image {
                                        id: columnsRowThumb
                                        anchors.fill: parent
                                        source: root.previewSource(path, name, isDir, thumbnailPath, 48, true,
                                                                   !(columnsCurrentList.moving || columnsCurrentList.flicking),
                                                                   Math.abs(parent.y - columnsCurrentList.contentY))
                                        fillMode: Image.PreserveAspectCrop
                                        asynchronous: true
                                        cache: true
                                        opacity: status === Image.Ready ? 1.0 : 0.0
                                        Behavior on opacity {
                                            NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
                                        }
                                    }
                                    Image {
                                        anchors.fill: parent
                                        visible: !(columnsRowThumb.status === Image.Ready)
                                        source: root.iconUrl(root.preferredFileIcon(iconName, mimeType, isDir))
                                        fillMode: Image.PreserveAspectFit
                                        asynchronous: true
                                        cache: true
                                    }
                                }
                                Text {
                                    text: name
                                    color: root.isSelected(index) ? Theme.color("selectedItemText") : Theme.color("textPrimary")
                                    font.pixelSize: root.bodyFontSize
                                    elide: Text.ElideRight
                                    width: columnsCurrentList.width - (isDir ? 60 : 42)
                                }

                                Image {
                                    visible: isDir
                                    width: 11
                                    height: 11
                                    source: root.iconUrl("go-next-symbolic")
                                    fillMode: Image.PreserveAspectFit
                                    asynchronous: true
                                    cache: true
                                    opacity: root.isSelected(index) ? 0.95 : 0.7
                                    Behavior on opacity {
                                        NumberAnimation { duration: 100; easing.type: Easing.OutCubic }
                                    }
                                }
                            }

                            MouseArea {
                                id: currentMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                onClicked: function(m) {
                                    var canDeferredRename =
                                            m.button === Qt.LeftButton &&
                                            m.modifiers === Qt.NoModifier &&
                                            root.selectedIndex === index &&
                                            root.selectedIndexes &&
                                            root.selectedIndexes.length === 1
                                    root.columnsFocusPane = 1
                                    if (m.button === Qt.RightButton) {
                                        if (!root.isSelected(index)) {
                                            root.selectedIndexRequested(index, Qt.NoModifier)
                                        }
                                        renameClickTimerColumns.stop()
                                        var p = parent.mapToItem(root, m.x, m.y)
                                        root.contextMenuRequested(index, p.x, p.y)
                                        return
                                    }
                                    root.selectedIndexRequested(index, m.modifiers)
                                    if (canDeferredRename) {
                                        renameClickTimerColumns.restart()
                                    } else {
                                        renameClickTimerColumns.stop()
                                    }
                                }
                                onPressed: function(m) {
                                    dragStartX = m.x
                                    dragStartY = m.y
                                    dragStarted = false
                                }
                                property real dragStartX: 0
                                property real dragStartY: 0
                                property bool dragStarted: false
                                onPositionChanged: function(m) {
                                    if (!(m.buttons & Qt.LeftButton)) {
                                        return
                                    }
                                    var dx = m.x - dragStartX
                                    var dy = m.y - dragStartY
                                    var dist2 = dx * dx + dy * dy
                                    if (!dragStarted && dist2 > 81) {
                                        renameClickTimerColumns.stop()
                                        dragStarted = true
                                        var scenePos = parent.mapToItem(root.hostItem ? root.hostItem : root, m.x, m.y)
                                        var contentPos = parent.mapToItem(columnsCurrentList.contentItem, m.x, m.y)
                                        var copyMode = !!(m.modifiers & Qt.ControlModifier)
                                        var hoverIdx = columnsCurrentList.indexAt(contentPos.x, contentPos.y)
                                        root.dragStartRequested(index, path, name, isDir, copyMode, scenePos.x, scenePos.y)
                                        root.dragMoveRequested(scenePos.x, scenePos.y, hoverIdx, copyMode)
                                        return
                                    }
                                    if (dragStarted && root.dndActive) {
                                        var scenePos2 = parent.mapToItem(root.hostItem ? root.hostItem : root, m.x, m.y)
                                        var contentPos2 = parent.mapToItem(columnsCurrentList.contentItem, m.x, m.y)
                                        var copyMode2 = !!(m.modifiers & Qt.ControlModifier)
                                        var hoverIdx2 = columnsCurrentList.indexAt(contentPos2.x, contentPos2.y)
                                        root.dragMoveRequested(scenePos2.x, scenePos2.y, hoverIdx2, copyMode2)
                                    }
                                }
                                onReleased: function(m) {
                                    if (dragStarted && root.dndActive) {
                                        var contentPos = parent.mapToItem(columnsCurrentList.contentItem, m.x, m.y)
                                        var targetIdx = columnsCurrentList.indexAt(contentPos.x, contentPos.y)
                                        root.dragEndRequested(targetIdx)
                                    }
                                    dragStarted = false
                                }
                                onDoubleClicked: {
                                    renameClickTimerColumns.stop()
                                    root.selectedIndexRequested(index, 0)
                                    root.activateRequested(index)
                                }
                            }

                            Timer {
                                id: renameClickTimerColumns
                                interval: 540
                                repeat: false
                                onTriggered: root.renameRequested(index)
                            }
                        }
                    }
                }

                Item { Layout.preferredWidth: 0; Layout.fillHeight: true }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "transparent"
                    border.width: Theme.borderWidthNone
                    border.color: "transparent"

                    ListView {
                        id: columnsChildList
                        visible: !root.columnsPreviewEntry
                        anchors.fill: parent
                        clip: true
                        model: root.columnChildEntries
                        delegate: Item {
                            required property int index
                            required property var modelData
                            readonly property string name: String((modelData && modelData.name) ? modelData.name : "")
                            readonly property string path: String((modelData && modelData.path) ? modelData.path : "")
                            readonly property bool isDir: !!(modelData && modelData.isDir)
                            readonly property string mimeType: String((modelData && modelData.mimeType) ? modelData.mimeType : "")
                            readonly property string iconName: String((modelData && modelData.iconName) ? modelData.iconName : "")
                            width: columnsChildList.width
                            height: 32

                            Rectangle {
                                anchors.fill: parent
                                border.width: Theme.borderWidthNone
                                border.color: "transparent"
                                color: (root.columnChildIndex === index && root.columnsFocusPane === 2)
                                       ? Theme.color("selectedItem")
                                       : (childMouse.containsMouse ? Theme.color("hoverItem") : "transparent")
                                Behavior on color {
                                    ColorAnimation { duration: 100; easing.type: Easing.OutCubic }
                                }
                            }

                            Row {
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 7
                                spacing: 6
                                Image {
                                    width: 16
                                    height: 16
                                    source: root.iconUrl(root.preferredFileIcon(iconName, mimeType, isDir))
                                    fillMode: Image.PreserveAspectFit
                                }
                                Text {
                                    text: name
                                    color: (root.columnChildIndex === index && root.columnsFocusPane === 2)
                                           ? Theme.color("selectedItemText")
                                           : Theme.color("textPrimary")
                                    font.pixelSize: root.bodyFontSize
                                    elide: Text.ElideRight
                                    width: columnsChildList.width - (isDir ? 60 : 42)
                                }

                                Image {
                                    visible: isDir
                                    width: 11
                                    height: 11
                                    source: root.iconUrl("go-next-symbolic")
                                    fillMode: Image.PreserveAspectFit
                                    asynchronous: true
                                    cache: true
                                    opacity: (root.columnChildIndex === index && root.columnsFocusPane === 2) ? 0.95 : 0.7
                                    Behavior on opacity {
                                        NumberAnimation { duration: 100; easing.type: Easing.OutCubic }
                                    }
                                }
                            }

                            MouseArea {
                                id: childMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    root.columnsSelectChild(index, false)
                                }
                                onDoubleClicked: {
                                    root.columnsSelectChild(index, true)
                                }
                            }
                        }
                    }

                    Item {
                        visible: !!root.columnsPreviewEntry
                        anchors.fill: parent

                        Column {
                            anchors.fill: parent
                            anchors.margins: 20
                            spacing: 10

                            Item {
                                width: Math.min(parent.width, parent.height * 0.55)
                                height: width
                                anchors.horizontalCenter: parent.horizontalCenter

                                Image {
                                    id: columnsPreviewImage
                                    anchors.fill: parent
                                    fillMode: Image.PreserveAspectFit
                                    asynchronous: true
                                    cache: true
                                    opacity: status === Image.Ready ? 1.0 : 0.0
                                    Behavior on opacity {
                                        NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
                                    }
                                    source: root.previewSource(String(root.columnsPreviewEntry ? root.columnsPreviewEntry.path : ""),
                                                               String(root.columnsPreviewEntry ? root.columnsPreviewEntry.name : ""),
                                                               false,
                                                               String(root.columnsPreviewEntry ? root.columnsPreviewEntry.thumbnailPath : ""),
                                                               width,
                                                               true,
                                                               true,
                                                               0)
                                }
                                Image {
                                    anchors.fill: parent
                                    visible: !(columnsPreviewImage.status === Image.Ready)
                                    fillMode: Image.PreserveAspectFit
                                    asynchronous: true
                                    cache: true
                                    source: root.iconUrl(root.preferredFileIcon(
                                                             String(root.columnsPreviewEntry && root.columnsPreviewEntry.iconName
                                                                    ? root.columnsPreviewEntry.iconName : ""),
                                                             String(root.columnsPreviewEntry && root.columnsPreviewEntry.mimeType
                                                                    ? root.columnsPreviewEntry.mimeType : ""),
                                                             !!(root.columnsPreviewEntry && root.columnsPreviewEntry.isDir)))
                                }
                            }

                            Text {
                                text: String(root.columnsPreviewEntry ? root.columnsPreviewEntry.name : "")
                                color: Theme.color("textPrimary")
                                font.pixelSize: root.bodyFontSize + 1
                                font.weight: Theme.fontWeight("bold")
                                wrapMode: Text.Wrap
                                lineHeightMode: Text.ProportionalHeight
                                lineHeight: Theme.lineHeight("tight")
                                maximumLineCount: 2
                                elide: Text.ElideRight
                                width: parent.width
                                horizontalAlignment: Text.AlignHCenter
                            }

                            Rectangle {
                                width: parent.width
                                radius: Theme.radiusControl
                                color: Theme.color("fileManagerSearchBg")
                                border.width: Theme.borderWidthThin
                                border.color: Theme.color("fileManagerControlBorder")
                                opacity: Theme.opacityGhost
                                implicitHeight: metaGrid.implicitHeight + 14

                                GridLayout {
                                    id: metaGrid
                                    anchors.fill: parent
                                    anchors.margins: 7
                                    columns: 2
                                    columnSpacing: 8
                                    rowSpacing: 4

                                    Text {
                                        text: "Size"
                                        color: Theme.color("textSecondary")
                                        font.pixelSize: root.secondaryFontSize
                                    }
                                    Text {
                                        text: root.formatSize(Number(root.columnsPreviewEntry ? root.columnsPreviewEntry.size : 0))
                                        color: Theme.color("textPrimary")
                                        font.pixelSize: root.secondaryFontSize
                                        horizontalAlignment: Text.AlignRight
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    Text {
                                        text: "Modified"
                                        color: Theme.color("textSecondary")
                                        font.pixelSize: root.secondaryFontSize
                                    }
                                    Text {
                                        text: {
                                            var raw = String(root.columnsPreviewEntry ? root.columnsPreviewEntry.lastModified : "")
                                            return raw.length > 0 ? raw : "Unknown"
                                        }
                                        color: Theme.color("textPrimary")
                                        font.pixelSize: root.secondaryFontSize
                                        horizontalAlignment: Text.AlignRight
                                        wrapMode: Text.WrapAnywhere
                                        lineHeightMode: Text.ProportionalHeight
                                        lineHeight: Theme.lineHeight("normal")
                                        Layout.fillWidth: true
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Item {
        anchors.fill: parent
        visible: root.emptyStateVisible && !(root.contentLoading && root.viewMode === "list" && !root.deepSearching)
        z: 120

        Column {
            anchors.centerIn: parent
            width: Math.min(parent.width - 32, 420)
            spacing: 10

            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 46
                height: 46
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                cache: true
                opacity: Theme.opacityMuted
                source: root.iconUrl(root.searching ? "system-search-symbolic" : "folder-open-symbolic")
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                color: Theme.color("textPrimary")
                font.pixelSize: root.bodyFontSize + 2
                font.weight: Theme.fontWeight("semibold")
                text: root.emptyStateTitle()
                wrapMode: Text.Wrap
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                color: Theme.color("textSecondary")
                font.pixelSize: root.secondaryFontSize
                text: root.emptyStateSubtitle()
                wrapMode: Text.Wrap
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8
                visible: !root.deepSearching && !root.contentLoading

                Button {
                    visible: root.searching
                    text: "Clear Search"
                    onClicked: root.clearSearchRequested()
                }

                Button {
                    visible: !root.searching && !root.trashView && !root.recentView
                    text: "New Folder"
                    onClicked: root.createFolderRequested()
                }

                Button {
                    visible: root.recentView || root.trashView
                    text: "Open Home"
                    onClicked: root.openHomeRequested()
                }

                Button {
                    visible: root.recentView
                    text: "Clear Recents"
                    onClicked: root.clearRecentsRequested()
                }
            }
        }
    }

    Item {
        anchors.fill: parent
        visible: root.contentLoading && root.viewMode === "list" && root.emptyStateVisible && !root.deepSearching
        z: 121

        Column {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 10
            spacing: 8

            Repeater {
                model: 8
                delegate: Rectangle {
                    width: parent.width
                    height: 22
                    radius: Theme.radiusControl
                    color: Theme.color("fileManagerControlBg")
                    border.width: Theme.borderWidthNone
                    opacity: Theme.opacityFaint

                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        spacing: 8

                        Rectangle {
                            width: 14
                            height: 14
                            radius: Theme.radiusSm
                            color: Theme.color("fileManagerControlBorder")
                            opacity: Theme.opacitySeparator
                        }

                        Rectangle {
                            width: Math.max(96, parent.parent.width * (0.22 + (index % 3) * 0.12))
                            height: 10
                            radius: Theme.radiusSm
                            color: Theme.color("fileManagerControlBorder")
                            opacity: Theme.opacityMuted
                        }
                    }

                    SequentialAnimation on opacity {
                        loops: Animation.Infinite
                        running: parent.visible
                        NumberAnimation { from: 0.35; to: 0.62; duration: 580; easing.type: Easing.InOutQuad }
                        NumberAnimation { from: 0.62; to: 0.35; duration: 580; easing.type: Easing.InOutQuad }
                    }
                }
            }
        }
    }

    Rectangle {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 8
        anchors.rightMargin: 12
        height: 24
        width: loadingRow.implicitWidth + 14
        radius: Theme.radiusPill
        color: Theme.color("fileManagerControlBg")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("fileManagerControlBorder")
        visible: root.deepSearching
        z: 125

        Row {
            id: loadingRow
            anchors.centerIn: parent
            spacing: 6

            BusyIndicator {
                running: parent.parent.visible
                width: 14
                height: 14
            }

            Text {
                text: "Searching"
                color: Theme.color("textSecondary")
                font.pixelSize: root.secondaryFontSize
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
}
