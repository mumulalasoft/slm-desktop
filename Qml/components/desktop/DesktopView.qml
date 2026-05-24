import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

Item {
    id: root

    property var fileModel: null
    property var entriesModel: []
    property int selectedIndex: -1
    property var selectedIndexes: []
    property int selectionAnchorIndex: -1
    property bool showFileExtensions: true
    property int iconSizeMode: 1
    property int gridSpacingMode: 1
    property bool suppressItemContextMenu: false

    property real topPadding: 54
    property real leftPadding: 24
    property real rightPadding: 28
    property real bottomPadding: 20

    property string fillMode: "vertical-first"
    property string anchorMode: "top-right"

    readonly property real workX: leftPadding
    readonly property real workY: topPadding
    readonly property real workWidth: Math.max(1, width - leftPadding - rightPadding)
    readonly property real workHeight: Math.max(1, height - topPadding - bottomPadding)

    readonly property real cellWidth: {
        var base = 104
        if (iconSizeMode === 0) {
            base = 96
        } else if (iconSizeMode === 2) {
            base = 112
        }
        if (gridSpacingMode === 0) {
            return base - 6
        }
        if (gridSpacingMode === 2) {
            return base + 12
        }
        return base
    }

    readonly property real cellHeight: {
        var base = 122
        if (iconSizeMode === 0) {
            base = 112
        } else if (iconSizeMode === 2) {
            base = 132
        }
        if (gridSpacingMode === 0) {
            return base - 8
        }
        if (gridSpacingMode === 2) {
            return base + 14
        }
        return base
    }

    readonly property real tileWidth: Math.max(86, cellWidth - 8)
    readonly property real tileHeight: Math.max(100, cellHeight - 8)

    property var slotByPath: ({})
    property var iconOverrideByPath: ({})
    property int iconOverrideRevision: 0
    property var positionedEntries: []
    property bool debugRenderLogs: false

    property bool dragActive: false
    property string dragPath: ""
    property int dragModelIndex: -1
    property real dragItemX: 0
    property real dragItemY: 0
    property real dragPointerX: 0
    property real dragPointerY: 0
    property real dragHotspotX: 0
    property real dragHotspotY: 0
    property int dragHoverSlot: -1
    property bool dragGestureStarted: false
    property real dragPressSceneX: 0
    property real dragPressSceneY: 0
    property bool selectionRectArmed: false
    property bool selectionRectActive: false
    property bool suppressBackgroundClick: false
    property int editingIndex: -1
    property string editingName: ""
    property real selectionPressX: 0
    property real selectionPressY: 0
    property real selectionCurrentX: 0
    property real selectionCurrentY: 0
    property int selectionPressModifiers: Qt.NoModifier
    property var selectionBaseIndexes: []
    property double suppressRightBackgroundMenuUntilMs: 0
    signal selectedIndexRequested(int index, int modifiers)
    signal activateRequested(int index)
    signal backgroundContextRequested(real x, real y)
    signal contextMenuRequested(int index, real x, real y)
    signal pointerPressed(real x, real y, int button, bool onItem)
    signal clearSelectionRequested()
    signal renameCommitRequested(int index, string name)
    signal renameCanceled()
    signal selectionRectRequested(var indexes, int modifiers, int anchorIndex, var baseIndexes)
    signal dragStartRequested(int index, string path, string name, bool isDir, bool copyMode, real sceneX, real sceneY)
    signal dragMoveRequested(real sceneX, real sceneY, int hoverIndex, bool copyMode)
    signal dragEndRequested(int targetIndex)
    signal externalDropRequested(var payload, int targetCellX, int targetCellY)

    function _isSelected(modelIndex) {
        if (modelIndex < 0) {
            return false
        }
        var arr = selectedIndexes ? selectedIndexes : []
        for (var i = 0; i < arr.length; ++i) {
            if (Number(arr[i]) === modelIndex) {
                return true
            }
        }
        return false
    }

    function _loadSlotMap() {
        if (fileModel && fileModel.loadSlotMap) {
            var loaded = fileModel.loadSlotMap()
            slotByPath = loaded ? loaded : ({})
        } else {
            slotByPath = ({})
        }
    }

    function _saveSlotMap() {
        if (!fileModel || !fileModel.saveSlotMap) {
            return
        }
        fileModel.saveSlotMap(slotByPath)
    }

    function _relayout() {
        var res = placement.computePlacement(entriesModel, slotByPath)
        positionedEntries = res.placed ? res.placed : []
        slotByPath = res.slotByPath ? res.slotByPath : ({})
        saveDebounce.restart()
    }

    function refresh() {
        _loadSlotMap()
        _relayout()
    }

    function cleanUpDesktop() {
        slotByPath = placement.cleanUp(entriesModel)
        _saveSlotMap()
        _relayout()
    }

    function snapToGrid() {
        var occupied = ({})
        var next = ({})
        var rows = positionedEntries ? positionedEntries.slice(0) : []
        rows.sort(function(a, b) {
            var sa = Number(a && a.slot !== undefined ? a.slot : 0)
            var sb = Number(b && b.slot !== undefined ? b.slot : 0)
            if (sa === sb) {
                return Number(a && a.modelIndex !== undefined ? a.modelIndex : 0)
                        - Number(b && b.modelIndex !== undefined ? b.modelIndex : 0)
            }
            return sa - sb
        })
        for (var i = 0; i < rows.length; ++i) {
            var row = rows[i] || ({})
            var path = String(row.path || "")
            if (path.length <= 0) {
                continue
            }
            var preferred = Number(row.slot)
            var slot = placement.nearestFreeSlot(preferred, occupied)
            if (slot < 0) {
                slot = placement.firstFreeSlot(occupied)
            }
            if (slot < 0) {
                continue
            }
            occupied[String(slot)] = path
            next[path] = slot
        }
        slotByPath = next
        _saveSlotMap()
        _relayout()
    }

    function placePathAtCell(pathValue, targetCellX, targetCellY) {
        var path = String(pathValue || "")
        if (path.length <= 0 || !placement.isValidCell(Number(targetCellX), Number(targetCellY))) {
            return false
        }
        slotByPath = placement.movePathToCell(path,
                                              Number(targetCellX),
                                              Number(targetCellY),
                                              entriesModel,
                                              slotByPath)
        _saveSlotMap()
        _relayout()
        return true
    }

    function sortDesktop(sortKey, descending) {
        var src = entriesModel ? entriesModel.slice(0) : []
        var key = String(sortKey || "name")
        src.sort(function(a, b) {
            var aa = a || ({})
            var bb = b || ({})
            var av = ""
            var bv = ""
            if (key === "type") {
                av = String(aa.isDir ? "dir" : (aa.mimeType || "file")).toLowerCase()
                bv = String(bb.isDir ? "dir" : (bb.mimeType || "file")).toLowerCase()
            } else if (key === "dateModified") {
                av = String(aa.lastModified || "")
                bv = String(bb.lastModified || "")
            } else {
                av = String(aa.name || "").toLowerCase()
                bv = String(bb.name || "").toLowerCase()
            }
            if (av === bv) {
                return 0
            }
            var cmp = (av < bv) ? -1 : 1
            return descending ? -cmp : cmp
        })

        var orderedSlots = placement.orderedSlots()
        var next = ({})
        for (var i = 0; i < src.length && i < orderedSlots.length; ++i) {
            var row = src[i] || ({})
            var p = String(row.path || "")
            if (p.length <= 0) {
                continue
            }
            next[p] = orderedSlots[i]
        }
        slotByPath = next
        _saveSlotMap()
        _relayout()
    }

    function _placedByModelIndex(modelIndex) {
        for (var i = 0; i < positionedEntries.length; ++i) {
            var row = positionedEntries[i] || ({})
            if (Number(row.modelIndex) === Number(modelIndex)) {
                return row
            }
        }
        return null
    }

    function _modelIndexAtSlot(slot) {
        for (var i = 0; i < positionedEntries.length; ++i) {
            var row = positionedEntries[i] || ({})
            if (Number(row.slot) === Number(slot)) {
                return Number(row.modelIndex)
            }
        }
        return -1
    }

    function _entryAtModelIndex(modelIndex) {
        var idx = Number(modelIndex)
        if (idx < 0) {
            return null
        }
        var rows = entriesModel ? entriesModel : []
        if (idx >= rows.length) {
            return null
        }
        return rows[idx] || null
    }

    function _normalizedPath(pathValue) {
        var p = String(pathValue || "").trim()
        if (p.length <= 0) {
            return ""
        }
        if (p === "~") {
            return ""
        }
        if (p.indexOf("~/") === 0) {
            return p
        }
        while (p.length > 1 && p.charAt(p.length - 1) === "/") {
            p = p.slice(0, -1)
        }
        return p
    }

    function iconOverrideForPath(pathValue, revision) {
        void revision
        var key = _normalizedPath(pathValue)
        if (key.length <= 0 || !iconOverrideByPath) {
            return ({})
        }
        return iconOverrideByPath[key] || ({})
    }

    function _isLocalFilePath(value) {
        var text = String(value || "").trim()
        return text.length > 0 && text.charAt(0) === "/"
    }

    function _providerLocalSource(value) {
        var raw = String(value || "").trim()
        if (raw.length <= 0) {
            return ""
        }
        if (raw.indexOf("image://themeicon/") === 0) {
            return raw
        }
        if (raw.indexOf("file://") === 0) {
            raw = raw.slice("file://".length)
        }
        if (_isLocalFilePath(raw)) {
            return "image://themeicon/" + encodeURIComponent(raw) + "?v="
                   + ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                      ? ThemeIconController.revision : 0)
        }
        return raw
    }

    function _themeIconSource(name) {
        var n = String(name || "").trim()
        var rev = ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                   ? ThemeIconController.revision : 0)
        if (n.length <= 0) {
            n = "text-x-generic-symbolic"
        }
        if (n.indexOf("image://themeicon/") === 0) {
            return n + (n.indexOf("?") >= 0 ? "&v=" : "?v=") + rev
        }
        return "image://themeicon/" + n + "?v=" + rev
    }

    function _resolvedIconSource(row, override) {
        var data = row || ({})
        var ov = override || ({})
        var source = String(ov.iconSource || data.iconSource || "").trim()
        if (source.length > 0) {
            return _providerLocalSource(source)
        }
        var name = String(ov.iconName || data.iconName || "").trim()
        if (name.length <= 0) {
            return _themeIconSource("text-x-generic-symbolic")
        }
        if (_isLocalFilePath(name)) {
            return _providerLocalSource(name)
        }
        if (name.indexOf("image://themeicon/") === 0) {
            return name
        }
        if (name.indexOf("file://") === 0) {
            return _providerLocalSource(name)
        }
        if (name.indexOf("image://") === 0 || name.indexOf("qrc:/") === 0) {
            return name
        }
        return _themeIconSource(name)
    }

    function setEntryIconOverride(pathValue, iconName, iconSource) {
        var key = _normalizedPath(pathValue)
        if (key.length <= 0) {
            return
        }
        var next = Object.assign({}, iconOverrideByPath || ({}))
        next[key] = {
            "iconName": String(iconName || ""),
            "iconSource": String(iconSource || "")
        }
        iconOverrideByPath = next
        iconOverrideRevision += 1
    }

    function _hasEntryAtCell(cellX, cellY) {
        var slot = placement.cellToSlot(Number(cellX), Number(cellY))
        if (slot < 0) {
            return false
        }
        for (var i = 0; i < positionedEntries.length; ++i) {
            var row = positionedEntries[i] || ({})
            if (Number(row.slot) === slot) {
                return true
            }
        }
        return false
    }

    function _logRenderStats(reason) {
        if (!debugRenderLogs) {
            return
        }
        var count = Number(positionedEntries ? positionedEntries.length : 0)
        var modelCount = Number(entriesModel ? entriesModel.length : 0)
        var cols = Number(placement.columns || 1)
        var rows = Number(placement.rows || 1)
        var capacity = Number(placement.capacity || 1)
        var has00 = _hasEntryAtCell(0, 0)
        var has01 = _hasEntryAtCell(0, 1)
        var has10 = _hasEntryAtCell(1, 0)
        console.log("[desktop-render] reason=" + String(reason || "")
                    + " model=" + String(modelCount)
                    + " placed=" + String(count)
                    + " grid=" + String(cols) + "x" + String(rows)
                    + " capacity=" + String(capacity)
                    + " cell00=" + String(has00)
                    + " cell01=" + String(has01)
                    + " cell10=" + String(has10))
    }

    function _startDrag(modelIndex, localX, localY) {
        var placed = _placedByModelIndex(modelIndex)
        var entry = _entryAtModelIndex(modelIndex)
        if (!placed || !entry) {
            return
        }

        dragActive = true
        dragPath = String(entry.path || "")
        dragModelIndex = modelIndex
        dragHotspotX = Math.max(0, Math.min(tileWidth, localX))
        dragHotspotY = Math.max(0, Math.min(tileHeight, localY))
        dragItemX = Number(placed.x) + ((cellWidth - tileWidth) * 0.5)
        dragItemY = Number(placed.y) + ((cellHeight - tileHeight) * 0.5)
        dragGestureStarted = true

        var scene = mapToItem(null, dragItemX + dragHotspotX, dragItemY + dragHotspotY)
        dragPointerX = scene.x
        dragPointerY = scene.y
        dragHoverSlot = Number(placed.slot)
        dragStartRequested(modelIndex,
                           String(entry.path || ""),
                           String(entry.name || ""),
                           !!entry.isDir,
                           false,
                           scene.x,
                           scene.y)
    }

    function _updateDragWithScene(modelIndex, sceneX, sceneY) {
        if (!dragActive || Number(modelIndex) !== dragModelIndex) {
            return
        }
        dragPointerX = sceneX
        dragPointerY = sceneY

        var itemTopLeft = mapFromItem(null, sceneX - dragHotspotX, sceneY - dragHotspotY)
        dragItemX = itemTopLeft.x
        dragItemY = itemTopLeft.y

        var local = mapFromItem(null, sceneX, sceneY)
        var targetCell = placement.pointToCell(local.x, local.y)
        if (targetCell.valid) {
            dragHoverSlot = placement.cellToSlot(targetCell.cellX, targetCell.cellY)
        } else {
            dragHoverSlot = -1
        }
        dragMoveRequested(sceneX, sceneY, _modelIndexAtSlot(dragHoverSlot), false)
    }

    function _finishDragWithScene(modelIndex, sceneX, sceneY) {
        if (!dragActive || Number(modelIndex) !== dragModelIndex) {
            return
        }
        dragPointerX = sceneX
        dragPointerY = sceneY

        var local = mapFromItem(null, sceneX, sceneY)
        var targetCell = placement.pointToCell(local.x, local.y)

        var finalTargetIndex = -1
        if (targetCell.valid) {
            slotByPath = placement.movePathToCell(dragPath,
                                                  targetCell.cellX,
                                                  targetCell.cellY,
                                                  entriesModel,
                                                  slotByPath)
            _saveSlotMap()
            _relayout()
            var movedSlot = Number(slotByPath[dragPath])
            finalTargetIndex = _modelIndexAtSlot(movedSlot)
        }

        dragEndRequested(finalTargetIndex)

        dragActive = false
        dragPath = ""
        dragModelIndex = -1
        dragHoverSlot = -1
    }

    function _rectBounds(x0, y0, x1, y1) {
        var left = Math.min(Number(x0), Number(x1))
        var top = Math.min(Number(y0), Number(y1))
        var right = Math.max(Number(x0), Number(x1))
        var bottom = Math.max(Number(y0), Number(y1))
        return {
            "left": left,
            "top": top,
            "right": right,
            "bottom": bottom,
            "width": Math.max(0, right - left),
            "height": Math.max(0, bottom - top)
        }
    }

    function _indexesInSelectionRect(bounds) {
        var out = []
        var rows = positionedEntries ? positionedEntries : []
        var b = bounds || ({})
        var left = Number(b.left || 0)
        var right = Number(b.right || 0)
        var top = Number(b.top || 0)
        var bottom = Number(b.bottom || 0)
        for (var i = 0; i < rows.length; ++i) {
            var row = rows[i] || ({})
            var modelIndex = Number(row.modelIndex)
            if (modelIndex < 0) {
                continue
            }
            var itemX = Number(row.x || 0) + ((cellWidth - tileWidth) * 0.5)
            var itemY = Number(row.y || 0) + ((cellHeight - tileHeight) * 0.5)
            var itemRight = itemX + tileWidth
            var itemBottom = itemY + tileHeight
            var intersects = !(itemRight < left || itemX > right || itemBottom < top || itemY > bottom)
            if (intersects) {
                out.push(modelIndex)
            }
        }
        out.sort(function(a, b) { return Number(a) - Number(b) })
        return out
    }

    function _modelIndexAtPoint(localX, localY) {
        var px = Number(localX || 0)
        var py = Number(localY || 0)
        var rows = positionedEntries ? positionedEntries : []
        for (var i = rows.length - 1; i >= 0; --i) {
            var row = rows[i] || ({})
            var modelIndex = Number(row.modelIndex)
            if (modelIndex < 0) {
                continue
            }
            var itemX = Number(row.x || 0) + ((cellWidth - tileWidth) * 0.5)
            var itemY = Number(row.y || 0) + ((cellHeight - tileHeight) * 0.5)

            // Editing item: pass through entire tile so the TextField receives all clicks
            if (modelIndex === root.editingIndex) {
                if (px >= itemX && px <= (itemX + tileWidth)
                        && py >= itemY && py <= (itemY + tileHeight)) {
                    return modelIndex
                }
                continue
            }

            var iconSize = Math.min(tileWidth, Math.max(24, tileHeight - 38))
            var iconX = itemX + ((tileWidth - iconSize) * 0.5)
            var iconY = itemY
            var iconHit = (px >= iconX && px <= (iconX + iconSize)
                           && py >= iconY && py <= (iconY + iconSize))

            var labelTop = itemY + Math.max(0, tileHeight - 36)
            var labelHit = (px >= (itemX + 8) && px <= (itemX + tileWidth - 8)
                            && py >= labelTop && py <= (itemY + tileHeight))

            if (iconHit || labelHit) {
                return modelIndex
            }
        }
        return -1
    }

    function _emitSelectionRect() {
        var bounds = _rectBounds(selectionPressX,
                                 selectionPressY,
                                 selectionCurrentX,
                                 selectionCurrentY)
        var indexes = _indexesInSelectionRect(bounds)
        var anchor = indexes.length > 0 ? Number(indexes[0]) : -1
        selectionRectRequested(indexes,
                               Number(selectionPressModifiers || Qt.NoModifier),
                               anchor,
                               selectionBaseIndexes ? selectionBaseIndexes.slice(0) : [])
    }

    function startInlineRename(indexValue, nameValue) {
        editingIndex = Number(indexValue)
        editingName = String(nameValue || "")
    }

    function stopInlineRename() {
        editingIndex = -1
        editingName = ""
    }

    function _commitInlineRename() {
        if (editingIndex < 0) {
            return
        }
        var idx = Number(editingIndex)
        var nextName = String(editingName || "")
        stopInlineRename()
        renameCommitRequested(idx, nextName)
    }

    onEntriesModelChanged: _relayout()
    onWorkWidthChanged: _relayout()
    onWorkHeightChanged: _relayout()
    onCellWidthChanged: _relayout()
    onCellHeightChanged: _relayout()
    onPositionedEntriesChanged: renderLogDebounce.restart()

    Component.onCompleted: refresh()

    DesktopPlacementEngine {
        id: placement
        workX: root.workX
        workY: root.workY
        workWidth: root.workWidth
        workHeight: root.workHeight
        cellWidth: root.cellWidth
        cellHeight: root.cellHeight
        fillMode: root.fillMode
        anchor: root.anchorMode
    }

    MouseArea {
        id: backgroundArea
        anchors.fill: parent
        z: 1000
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: true
        onPressed: function(mouse) {
            if (root.dragActive) {
                return
            }
            var hitIndex = root._modelIndexAtPoint(mouse.x, mouse.y)
            if (hitIndex >= 0) {
                mouse.accepted = false
                return
            }
            root._commitInlineRename()
            root.pointerPressed(mouse.x, mouse.y, mouse.button, false)
            if (mouse.button === Qt.RightButton) {
                if (Date.now() < Number(root.suppressRightBackgroundMenuUntilMs || 0)) {
                    return
                }
                root.clearSelectionRequested()
                root.backgroundContextRequested(mouse.x, mouse.y)
                root.suppressBackgroundClick = true
                backgroundClickSuppressTimer.restart()
                return
            }
            if (mouse.button !== Qt.LeftButton) {
                return
            }
            root.selectionRectArmed = true
            root.selectionRectActive = false
            root.selectionPressX = mouse.x
            root.selectionPressY = mouse.y
            root.selectionCurrentX = mouse.x
            root.selectionCurrentY = mouse.y
            root.selectionPressModifiers = Number(mouse.modifiers || Qt.NoModifier)
            root.selectionBaseIndexes = root.selectedIndexes ? root.selectedIndexes.slice(0) : []
            // Ctrl = additive rubber-band: keep existing selection visible until rect forms
            if (!(root.selectionPressModifiers & Qt.ControlModifier)) {
                root.clearSelectionRequested()
            }
        }
        onPositionChanged: function(mouse) {
            if (!root.selectionRectArmed || !(mouse.buttons & Qt.LeftButton)) {
                return
            }
            root.selectionCurrentX = mouse.x
            root.selectionCurrentY = mouse.y
            var dx = mouse.x - root.selectionPressX
            var dy = mouse.y - root.selectionPressY
            if (!root.selectionRectActive && ((dx * dx) + (dy * dy)) >= 36) {
                root.selectionRectActive = true
                // Non-additive: ensure clean slate (already cleared on press, but guard
                // against any edge cases where onPressed didn't clear)
                if (!(root.selectionPressModifiers & Qt.ControlModifier)) {
                    root.clearSelectionRequested()
                }
            }
            if (root.selectionRectActive) {
                root._emitSelectionRect()
            }
        }
        onReleased: function(mouse) {
            if (mouse.button !== Qt.LeftButton) {
                return
            }
            if (root.selectionRectActive) {
                root.selectionCurrentX = mouse.x
                root.selectionCurrentY = mouse.y
                root._emitSelectionRect()
                root.suppressBackgroundClick = true
                backgroundClickSuppressTimer.restart()
            }
            root.selectionRectArmed = false
            root.selectionRectActive = false
            root.selectionBaseIndexes = []
        }
        onCanceled: {
            root.selectionRectArmed = false
            root.selectionRectActive = false
            root.selectionBaseIndexes = []
        }
        onClicked: function(mouse) {
            if (root.suppressBackgroundClick) {
                return
            }
            if (mouse.button === Qt.LeftButton) {
                root.clearSelectionRequested()
            }
        }
    }

    Item {
        id: desktopCanvas
        anchors.fill: parent
        clip: true

        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.width: Theme.borderWidthNone
            visible: root.positionedEntries.length <= 0

            Text {
                anchors.centerIn: parent
                text: qsTr("Desktop")
                color: Theme.color("selectedItemText")
                opacity: Theme.opacityFaint
                font.family: Theme.fontFamilyDisplay
                font.pixelSize: Theme.fontSize("hero")
                font.weight: Theme.fontWeight("semibold")
            }
        }

        Repeater {
            id: itemsRepeater
            model: root.positionedEntries

            delegate: Item {
                id: cellItem
                readonly property var placedRow: modelData || ({})
                readonly property int modelIndex: Number(placedRow.modelIndex)
                readonly property var sourceRow: root._entryAtModelIndex(modelIndex)
                readonly property var iconOverride: root.iconOverrideForPath(sourceRow && sourceRow.path ? sourceRow.path : "",
                                                                             root.iconOverrideRevision)
                readonly property bool draggingSelf: root.dragActive
                                                    && root.dragModelIndex === modelIndex
                                                    && root.dragGestureStarted
                readonly property real baseX: Number(placedRow.x || 0) + ((root.cellWidth - root.tileWidth) * 0.5)
                readonly property real baseY: Number(placedRow.y || 0) + ((root.cellHeight - root.tileHeight) * 0.5)

                x: baseX
                y: baseY
                width: root.tileWidth
                height: root.tileHeight
                z: 10
                opacity: draggingSelf ? 0.28 : 1.0

                Behavior on opacity {
                    NumberAnimation {
                        duration: Theme.durationSm
                        easing.type: Theme.easingDefault
                    }
                }

                property bool leftPressed: false
                property bool dragArmed: false
                property bool suppressClick: false
                property real pressSceneX: 0
                property real pressSceneY: 0

                DesktopItem {
                    id: desktopItem
                    anchors.fill: parent
                    displayName: String(sourceRow && sourceRow.name ? sourceRow.name : "")
                    iconName: String(iconOverride && iconOverride.iconName
                                     ? iconOverride.iconName
                                     : (sourceRow && sourceRow.iconName ? sourceRow.iconName : ""))
                    iconSource: root._resolvedIconSource(sourceRow, iconOverride)
                    thumbnailSource: String(sourceRow && sourceRow.thumbnailPath ? sourceRow.thumbnailPath : "")
                    selected: root._isSelected(modelIndex)
                    dragging: false
                    isDir: !!(sourceRow && sourceRow.isDir)
                    networkShared: !!(sourceRow && sourceRow.networkShared)
                    previewCandidate: !isDir
                                      && String(thumbnailSource || "").length > 0
                                      && String(sourceRow && sourceRow.path ? sourceRow.path : "").toLowerCase().slice(-8) !== ".desktop"
                                      && String(sourceRow && sourceRow.mimeType ? sourceRow.mimeType : "").toLowerCase().indexOf("desktop") < 0
                    tileWidth: root.tileWidth
                    tileHeight: root.tileHeight
                    editing: root.editingIndex === modelIndex
                    editText: (root.editingIndex === modelIndex) ? root.editingName : ""

                    onEditValueChanged: function(text) {
                        if (root.editingIndex === modelIndex) {
                            root.editingName = String(text || "")
                        }
                    }
                    onEditCommitted: function(text) {
                        if (root.editingIndex === modelIndex) {
                            root.editingName = String(text || "")
                            root._commitInlineRename()
                        }
                    }
                    onEditCanceled: {
                        if (root.editingIndex === modelIndex) {
                            root.stopInlineRename()
                            root.renameCanceled()
                        }
                    }


                    onPressed: function(px, py, button, buttons, modifiers) {
                        if (root.editingIndex >= 0 && root.editingIndex !== modelIndex) {
                            root._commitInlineRename()
                        }
                        var lp = desktopItem.mapToItem(root, px, py)
                        root.pointerPressed(lp.x, lp.y, button, true)
                        if (button === Qt.RightButton) {
                            if (root.suppressItemContextMenu) {
                                return
                            }
                            root.suppressRightBackgroundMenuUntilMs = Date.now() + 260
                            if (!root._isSelected(modelIndex)) {
                                root.selectedIndexRequested(modelIndex, Qt.NoModifier)
                            }
                            var p = desktopItem.mapToItem(root, px, py)
                            Qt.callLater(function() {
                                root.contextMenuRequested(modelIndex, p.x, p.y)
                            })
                            return
                        }
                        cellItem.leftPressed = (button === Qt.LeftButton)
                        cellItem.dragArmed = false
                        cellItem.suppressClick = false
                        var scene = desktopItem.mapToItem(null, px, py)
                        cellItem.pressSceneX = scene.x
                        cellItem.pressSceneY = scene.y
                    }

                    onMoved: function(px, py, buttons, modifiers) {
                        if (!cellItem.leftPressed) {
                            return
                        }
                        var scene = desktopItem.mapToItem(null, px, py)
                        var dx = scene.x - cellItem.pressSceneX
                        var dy = scene.y - cellItem.pressSceneY
                        if (!cellItem.dragArmed && ((dx * dx) + (dy * dy)) >= 36) {
                            cellItem.dragArmed = true
                            cellItem.suppressClick = true
                            if (!root._isSelected(modelIndex)) {
                                root.selectedIndexRequested(modelIndex, Qt.NoModifier)
                            }
                            root._startDrag(modelIndex, px, py)
                        }
                        if (cellItem.dragArmed) {
                            root._updateDragWithScene(modelIndex, scene.x, scene.y)
                        }
                    }

                    onReleased: function(px, py, button, buttons, modifiers) {
                        if (cellItem.dragArmed) {
                            var scene = desktopItem.mapToItem(null, px, py)
                            root._finishDragWithScene(modelIndex, scene.x, scene.y)
                            clickSuppressTimer.restart()
                        }
                        cellItem.leftPressed = false
                        cellItem.dragArmed = false
                    }

                    onClicked: function(button, modifiers, px, py) {
                        if (root.editingIndex === modelIndex) {
                            return
                        }
                        if (cellItem.suppressClick) {
                            return
                        }
                        if (button === Qt.LeftButton) {
                            root.selectedIndexRequested(modelIndex, modifiers)
                        }
                    }

                    onDoubleClicked: function(button, modifiers, px, py) {
                        if (button !== Qt.LeftButton) {
                            return
                        }
                        root.selectedIndexRequested(modelIndex, Qt.NoModifier)
                        root.activateRequested(modelIndex)
                    }
                }

                Timer {
                    id: clickSuppressTimer
                    interval: 150
                    repeat: false
                    onTriggered: cellItem.suppressClick = false
                }
            }
        }

        DesktopItem {
            id: dragOverlay
            visible: root.dragActive
            z: 1200
            x: root.dragItemX
            y: root.dragItemY
            width: root.tileWidth
            height: root.tileHeight
            interactable: false

            readonly property var _dragRow: root._entryAtModelIndex(root.dragModelIndex)
            displayName: _dragRow ? String(_dragRow.name || "") : ""
            iconName: _dragRow ? String(_dragRow.iconName || "") : ""
            iconSource: _dragRow ? root._resolvedIconSource(_dragRow, ({}) ) : ""
            thumbnailSource: _dragRow ? String(_dragRow.thumbnailPath || "") : ""
            isDir: _dragRow ? !!_dragRow.isDir : false
            networkShared: _dragRow ? !!_dragRow.networkShared : false
            previewCandidate: !isDir && String(thumbnailSource || "").length > 0
            selected: root.dragActive && root._isSelected(root.dragModelIndex)
            dragging: root.dragActive
            tileWidth: root.tileWidth
            tileHeight: root.tileHeight
        }
    }

    Rectangle {
        id: dragTargetHighlight
        visible: root.dragActive && root.dragHoverSlot >= 0
        z: 900
        width: root.tileWidth
        height: root.tileHeight
        radius: Theme.radiusCard
        color: Theme.color("accentSubtle")
        opacity: Theme.opacitySubtle
        border.width: Theme.borderWidthThick
        border.color: Theme.color("focusRingStrong")

        readonly property var cell: placement.slotToCell(root.dragHoverSlot)
        x: Number(root.workX) + Number((cell.valid ? cell.cellX : 0) * root.cellWidth) + ((root.cellWidth - width) * 0.5)
        y: Number(root.workY) + Number((cell.valid ? cell.cellY : 0) * root.cellHeight) + ((root.cellHeight - height) * 0.5)
    }

    Rectangle {
        id: selectionRect
        visible: root.selectionRectActive
        z: 880
        readonly property var bounds: root._rectBounds(root.selectionPressX,
                                                       root.selectionPressY,
                                                       root.selectionCurrentX,
                                                       root.selectionCurrentY)
        x: Number(bounds.left || 0)
        y: Number(bounds.top || 0)
        width: Number(bounds.width || 0)
        height: Number(bounds.height || 0)
        color: Theme.color("selectionRectFill")
        opacity: Theme.opacityGhost
        border.width: Theme.borderWidthThin
        border.color: Theme.color("selectionRectBorder")
        radius: Theme.radiusControl
    }

    DropArea {
        anchors.fill: parent
        onDropped: function(drop) {
            var localPos = root.mapFromItem(null, drop.x, drop.y)
            var cell = placement.pointToCell(localPos.x, localPos.y)
            var appEntryJson = ""
            var desktopItemJson = ""
            var uriListText = ""
            var formats = []
            if (drop.formats && drop.formats.length !== undefined) {
                for (var f = 0; f < drop.formats.length; ++f) {
                    formats.push(String(drop.formats[f] || ""))
                }
            }
            if (drop.getDataAsString) {
                try { appEntryJson = String(drop.getDataAsString("application/x-slm-app-entry") || "") } catch (e0) {}
                try { desktopItemJson = String(drop.getDataAsString("application/x-slm-desktop-item") || "") } catch (e1) {}
                try { uriListText = String(drop.getDataAsString("text/uri-list") || "") } catch (e2) {}
            }
            var payload = {
                "urls": drop.urls ? drop.urls : [],
                "text": String(drop.text || ""),
                "keys": drop.keys ? drop.keys : [],
                "formats": formats,
                "appEntryJson": appEntryJson,
                "desktopItemJson": desktopItemJson,
                "uriListText": uriListText
            }
            root.externalDropRequested(payload,
                                       cell.valid ? cell.cellX : -1,
                                       cell.valid ? cell.cellY : -1)
            drop.acceptProposedAction()
        }
    }

    Timer {
        id: saveDebounce
        interval: 90
        repeat: false
        onTriggered: root._saveSlotMap()
    }

    Timer {
        id: renderLogDebounce
        interval: 110
        repeat: false
        onTriggered: root._logRenderStats("positioned-changed")
    }

    Timer {
        id: backgroundClickSuppressTimer
        interval: 120
        repeat: false
        onTriggered: root.suppressBackgroundClick = false
    }
}
