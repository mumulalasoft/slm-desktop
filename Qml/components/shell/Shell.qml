import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

Item {
    id: root

    property string wallpaperSource: "qrc:/images/wallpaper.jpeg"
    property var shortcutsModel: (typeof ShortcutModel !== "undefined") ? ShortcutModel : null
    property bool inputEnabled: true
    property bool contextMenuOnly: false
    property real dockTopY: -1
    property int maxShortcuts: 24
    property int totalSlots: Math.max(modelCount(), gridColumns() * visibleRows())
    property int cellWidth: 104
    property int cellHeight: 122
    property int tileWidth: 96
    property int tileHeight: 108
    property bool slotsLoaded: false
    readonly property real shellBottomMargin: {
        if (dockTopY <= 0 || dockTopY >= height) {
            return 20
        }
        // Keep the shortcut area ending just above the dock top edge.
        return Math.max(20, (height - dockTopY) + 8)
    }

    property bool dragInProgress: false
    property bool dragActive: false
    property int dragFromSlot: -1
    property int dragToSlot: -1
    property real dragPointerX: 0
    property real dragPointerY: 0
    property string dragEntryName: ""
    property string dragEntryIconName: ""
    property string dragEntryIconSource: ""
    property var dragEntryData: null

    property var slotToModelIndex: []
    property var modelIndexToSlot: ({})
    property var slotByKey: ({})
    property var selectedModelMap: ({})
    property int selectionAnchorIndex: -1
    property bool selectionDragActive: false
    property bool selectionAdditive: false
    property bool selectionDragMoved: false
    property real selectionStartX: 0
    property real selectionStartY: 0
    property real selectionCurrentX: 0
    property real selectionCurrentY: 0

    signal appLaunched(var appData)
    signal addToDockRequested(var appData)
    signal dockDropHover(bool active, real globalX, string iconPath)
    signal dockDropCommit(string desktopFile, real globalX, string iconPath)
    signal dockDropClear()
    signal shellContextMenuRequested(real x, real y)

    function isSelected(modelIndex) {
        if (modelIndex === undefined || modelIndex < 0) {
            return false
        }
        return selectedModelMap[modelIndex] === true
    }

    function clearSelection() {
        selectedModelMap = ({})
        selectionAnchorIndex = -1
    }

    function setSelected(modelIndex, enabled) {
        if (modelIndex === undefined || modelIndex < 0) {
            return
        }
        var next = ({})
        for (var key in selectedModelMap) {
            if (selectedModelMap[key] === true) {
                next[key] = true
            }
        }
        if (enabled) {
            next[modelIndex] = true
        } else {
            delete next[modelIndex]
        }
        selectedModelMap = next
    }

    function selectSingle(modelIndex) {
        if (modelIndex === undefined || modelIndex < 0) {
            clearSelection()
            return
        }
        var next = ({})
        next[modelIndex] = true
        selectedModelMap = next
        selectionAnchorIndex = modelIndex
    }

    function toggleSelection(modelIndex) {
        if (modelIndex === undefined || modelIndex < 0) {
            return
        }
        setSelected(modelIndex, !isSelected(modelIndex))
        selectionAnchorIndex = modelIndex
    }

    function selectRangeTo(modelIndex, additive) {
        if (modelIndex === undefined || modelIndex < 0) {
            return
        }
        var anchor = selectionAnchorIndex
        if (anchor < 0) {
            anchor = modelIndex
        }
        var fromIdx = Math.min(anchor, modelIndex)
        var toIdx = Math.max(anchor, modelIndex)

        var next = ({})
        if (additive) {
            for (var key in selectedModelMap) {
                if (selectedModelMap[key] === true) {
                    next[key] = true
                }
            }
        }
        for (var i = fromIdx; i <= toIdx; ++i) {
            next[i] = true
        }
        selectedModelMap = next
    }

    function normalizeSelection() {
        var count = modelCount()
        var next = ({})
        for (var key in selectedModelMap) {
            var idx = parseInt(key)
            if (!isNaN(idx) && idx >= 0 && idx < count && selectedModelMap[key] === true) {
                next[idx] = true
            }
        }
        selectedModelMap = next
        if (selectionAnchorIndex >= count) {
            selectionAnchorIndex = count - 1
        }
    }

    function beginSelectionDrag(localX, localY, additive) {
        selectionDragActive = true
        selectionAdditive = additive
        selectionDragMoved = false
        selectionStartX = localX
        selectionStartY = localY
        selectionCurrentX = localX
        selectionCurrentY = localY
        if (!selectionAdditive) {
            clearSelection()
        }
    }

    function updateSelectionDrag(localX, localY) {
        if (!selectionDragActive) {
            return
        }
        if (Math.abs(localX - selectionStartX) > 3 || Math.abs(localY - selectionStartY) > 3) {
            selectionDragMoved = true
        }
        selectionCurrentX = localX
        selectionCurrentY = localY
    }

    function finishSelectionDrag() {
        if (!selectionDragActive) {
            return
        }
        var minX = Math.min(selectionStartX, selectionCurrentX)
        var maxX = Math.max(selectionStartX, selectionCurrentX)
        var minY = Math.min(selectionStartY, selectionCurrentY)
        var maxY = Math.max(selectionStartY, selectionCurrentY)
        var next = ({})
        if (selectionAdditive) {
            for (var existing in selectedModelMap) {
                if (selectedModelMap[existing] === true) {
                    next[existing] = true
                }
            }
        }
        for (var slot = 0; slot < totalSlots; ++slot) {
            var modelIndex = (slot < slotToModelIndex.length && slotToModelIndex[slot] !== undefined)
                             ? slotToModelIndex[slot] : -1
            if (modelIndex < 0) {
                continue
            }
            var itemX = slotX(slot) + (cellWidth - tileWidth) * 0.5
            var itemY = slotY(slot) + (cellHeight - tileHeight) * 0.5
            var itemR = itemX + tileWidth
            var itemB = itemY + tileHeight
            var intersects = !(itemR < minX || itemX > maxX || itemB < minY || itemY > maxY)
            if (intersects) {
                next[modelIndex] = true
            }
        }
        selectedModelMap = next
        selectionDragActive = false
    }

    function launchEntry(modelIndex, entryMap, sourceLabel) {
        if (modelIndex < 0 || !entryMap) {
            return
        }
        if (typeof CursorController !== "undefined" && CursorController &&
            CursorController.startBusy) {
            CursorController.startBusy(1300)
        }
        if (typeof AppCommandRouter !== "undefined" && AppCommandRouter &&
            AppCommandRouter.route) {
            AppCommandRouter.route("app.entry", entryMap, "shell-shortcut-" + sourceLabel)
        } else if (typeof AppExecutionGate !== "undefined" && AppExecutionGate &&
                   AppExecutionGate.launchEntryMap) {
            AppExecutionGate.launchEntryMap(entryMap, "shell-shortcut-" + sourceLabel)
        } else {
            console.warn("Shell launch ignored: no router/gate available")
        }
    }

    function arrangeShortcuts() {
        if (!shortcutsModel || !shortcutsModel.arrangeShortcuts) {
            return
        }
        if (shortcutsModel.arrangeShortcuts()) {
            slotByKey = ({})
            persistSlotMap()
            rebuildSlots()
        }
    }

    function modelCount() {
        if (!shortcutsModel || !shortcutsModel.shortcutCount) {
            return 0
        }
        return shortcutsModel.shortcutCount()
    }

    function gridColumns() {
        return Math.max(1, Math.floor(shortcutFlick.width / cellWidth))
    }

    function visibleRows() {
        // Use full rows only, so no shortcut row can spill into dock area.
        return Math.max(1, Math.floor(shortcutFlick.height / cellHeight))
    }

    function slotX(slot) {
        return (slot % gridColumns()) * cellWidth
    }

    function slotY(slot) {
        return Math.floor(slot / gridColumns()) * cellHeight
    }

    function entryKey(entry, fallbackIndex) {
        if (!entry) {
            return "idx:" + fallbackIndex
        }
        if (entry.sourcePath && entry.sourcePath.length > 0) {
            return "src:" + entry.sourcePath
        }
        if (entry.desktopFile && entry.desktopFile.length > 0) {
            return "desktop:" + entry.desktopFile
        }
        if (entry.target && entry.target.length > 0) {
            return "target:" + entry.target
        }
        return "idx:" + fallbackIndex
    }

    function entryAt(modelIndex) {
        if (!shortcutsModel || !shortcutsModel.get || modelIndex < 0 || modelIndex >= modelCount()) {
            return null
        }
        return shortcutsModel.get(modelIndex)
    }

    function firstFreeSlot(slots) {
        for (var i = 0; i < totalSlots; ++i) {
            if (slots[i] === -1 || slots[i] === undefined) {
                return i
            }
        }
        return -1
    }

    function rebuildSlots() {
        var count = modelCount()
        var nextSlots = []
        for (var i = 0; i < totalSlots; ++i) {
            nextSlots.push(-1)
        }

        var used = ({})
        for (var m = 0; m < count; ++m) {
            var entry = entryAt(m)
            var key = entryKey(entry, m)
            var preferred = slotByKey[key]
            if (preferred !== undefined && preferred >= 0 && preferred < totalSlots && !used[preferred]) {
                nextSlots[preferred] = m
                used[preferred] = true
            }
        }

        for (var j = 0; j < count; ++j) {
            var alreadyPlaced = false
            for (var s = 0; s < totalSlots; ++s) {
                if (nextSlots[s] === j) {
                    alreadyPlaced = true
                    break
                }
            }
            if (alreadyPlaced) {
                continue
            }
            var free = firstFreeSlot(nextSlots)
            if (free < 0) {
                break
            }
            nextSlots[free] = j
            var e = entryAt(j)
            var k = entryKey(e, j)
            slotByKey[k] = free
        }

        var nextModelToSlot = ({})
        for (var slot = 0; slot < totalSlots; ++slot) {
            if (nextSlots[slot] >= 0) {
                nextModelToSlot[nextSlots[slot]] = slot
            }
        }

        slotToModelIndex = nextSlots
        modelIndexToSlot = nextModelToSlot
        persistSlotMap()
        normalizeSelection()

        if (dragActive && dragFromSlot >= 0) {
            var movedIndex = slotToModelIndex[dragFromSlot]
            if (movedIndex === undefined || movedIndex < 0) {
                clearDragState()
            }
        }
    }

    function loadPersistedSlotMap() {
        if (slotsLoaded) {
            return
        }
        slotsLoaded = true
        if (!shortcutsModel || !shortcutsModel.loadSlotMap) {
            return
        }
        var loaded = shortcutsModel.loadSlotMap()
        if (loaded) {
            slotByKey = loaded
        }
    }

    function persistSlotMap() {
        if (!slotsLoaded || !shortcutsModel || !shortcutsModel.saveSlotMap) {
            return
        }
        shortcutsModel.saveSlotMap(slotByKey)
    }

    function nearestSlotIndexFromLocal(localX, localY) {
        var nearest = 0
        var nearestDist = Number.MAX_VALUE
        for (var slot = 0; slot < totalSlots; ++slot) {
            var cx = slotX(slot) + (cellWidth * 0.5)
            var cy = slotY(slot) + (cellHeight * 0.5)
            var dx = cx - localX
            var dy = cy - localY
            var d2 = dx * dx + dy * dy
            if (d2 < nearestDist) {
                nearestDist = d2
                nearest = slot
            }
        }
        return nearest
    }

    function nearestSlotIndex(globalX, globalY) {
        var local = shortcutContent.mapFromItem(root, globalX, globalY)
        return nearestSlotIndexFromLocal(local.x, local.y)
    }

    function beginDrag(fromSlot, entry, pointerX, pointerY) {
        if (fromSlot < 0 || fromSlot >= totalSlots) {
            return
        }
        dragActive = true
        dragInProgress = true
        dragFromSlot = fromSlot
        dragToSlot = fromSlot
        dragPointerX = pointerX
        dragPointerY = pointerY
        dragEntryName = entry && entry.name ? entry.name : "App"
        dragEntryIconName = entry && entry.iconName ? entry.iconName : ""
        dragEntryIconSource = entry && entry.iconSource ? entry.iconSource : ""
        dragEntryData = entry
    }

    function updateDrag(pointerX, pointerY) {
        if (!dragActive) {
            return
        }
        dragPointerX = pointerX
        dragPointerY = pointerY
        dragToSlot = nearestSlotIndex(pointerX, pointerY)
    }

    function moveEntryToSlot(fromSlot, toSlot) {
        if (fromSlot < 0 || toSlot < 0 || fromSlot >= totalSlots || toSlot >= totalSlots || fromSlot === toSlot) {
            return
        }
        var next = slotToModelIndex.slice(0)
        var fromModel = next[fromSlot]
        if (fromModel === undefined || fromModel < 0) {
            return
        }
        var toModel = next[toSlot]
        next[toSlot] = fromModel
        next[fromSlot] = (toModel === undefined) ? -1 : toModel

        var fromEntry = entryAt(fromModel)
        var fromKey = entryKey(fromEntry, fromModel)
        slotByKey[fromKey] = toSlot

        if (toModel !== undefined && toModel >= 0) {
            var toEntry = entryAt(toModel)
            var toKey = entryKey(toEntry, toModel)
            slotByKey[toKey] = fromSlot
        }

        var nextModelToSlot = ({})
        for (var i = 0; i < totalSlots; ++i) {
            if (next[i] !== undefined && next[i] >= 0) {
                nextModelToSlot[next[i]] = i
            }
        }
        slotToModelIndex = next
        modelIndexToSlot = nextModelToSlot
        persistSlotMap()
    }

    function clearDragState() {
        dragActive = false
        dragInProgress = false
        dragFromSlot = -1
        dragToSlot = -1
        dragEntryName = ""
        dragEntryIconName = ""
        dragEntryIconSource = ""
        dragEntryData = null
    }

    function refreshShortcuts() {
        if (shortcutsModel && shortcutsModel.refresh) {
            shortcutsModel.refresh()
        } else {
            rebuildSlots()
        }
    }

    onVisibleChanged: {
        if (visible) {
            refreshShortcuts()
        }
    }

    onWidthChanged: rebuildSlots()
    onHeightChanged: rebuildSlots()
    onTotalSlotsChanged: rebuildSlots()

    Component.onCompleted: {
        refreshShortcuts()
        loadPersistedSlotMap()
        rebuildSlots()
    }

    Connections {
        target: root.shortcutsModel
        function onModelReset() {
            root.slotsLoaded = false
            root.loadPersistedSlotMap()
            root.rebuildSlots()
        }
        function onRowsInserted() { root.rebuildSlots() }
        function onRowsRemoved() { root.rebuildSlots() }
    }

    DesktopBackground {
        anchors.fill: parent
        imageSource: root.wallpaperSource
    }

    Flickable {
        id: shortcutFlick
        anchors.fill: parent
        anchors.topMargin: 42
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        anchors.bottomMargin: root.shellBottomMargin
        contentWidth: width
        contentHeight: shortcutContent.height
        clip: true
        interactive: !root.dragInProgress

        Item {
            id: shortcutContent
            width: shortcutFlick.width
            height: Math.max(shortcutFlick.height,
                             Math.ceil(totalSlots / gridColumns()) * cellHeight)

            MouseArea {
                id: shellContextArea
                anchors.fill: parent
                acceptedButtons: root.contextMenuOnly ? Qt.RightButton : (Qt.LeftButton | Qt.RightButton)
                z: -1
                enabled: root.inputEnabled
                property bool leftPressed: false
                onClicked: function(mouse) {
                    if (mouse.button === Qt.LeftButton) {
                        if (!root.selectionDragMoved) {
                            root.clearSelection()
                        }
                        root.selectionDragMoved = false
                    }
                }
                onPressed: function(mouse) {
                    if (mouse.button === Qt.RightButton) {
                        var p = shellContextArea.mapToItem(root, mouse.x, mouse.y)
                        root.shellContextMenuRequested(p.x, p.y)
                        return
                    }
                    if (root.contextMenuOnly) {
                        leftPressed = false
                        return
                    }
                    leftPressed = (mouse.button === Qt.LeftButton)
                    if (!leftPressed) {
                        return
                    }
                    var additive = ((mouse.modifiers & Qt.ControlModifier) !== 0) ||
                                   ((mouse.modifiers & Qt.MetaModifier) !== 0)
                    root.beginSelectionDrag(mouse.x, mouse.y, additive)
                }
                onPositionChanged: function(mouse) {
                    if (!leftPressed || !shellContextArea.pressed) {
                        return
                    }
                    root.updateSelectionDrag(mouse.x, mouse.y)
                }
                onReleased: function(mouse) {
                    if (leftPressed) {
                        root.updateSelectionDrag(mouse.x, mouse.y)
                        root.finishSelectionDrag()
                    }
                    leftPressed = false
                }
                onCanceled: {
                    leftPressed = false
                    root.selectionDragActive = false
                    root.selectionDragMoved = false
                }
            }

            Repeater {
                id: slotRepeater
                model: root.totalSlots

                delegate: ShellShortcutTile {
                    shellRoot: root
                    inputEnabled: root.inputEnabled
                    contextMenuOnly: root.contextMenuOnly
                    slotIndex: index
                    cellWidth: root.cellWidth
                    cellHeight: root.cellHeight
                    tileWidth: root.tileWidth
                    tileHeight: root.tileHeight
                    x: root.slotX(index)
                    y: root.slotY(index)
                    modelIndex: (index < root.slotToModelIndex.length && root.slotToModelIndex[index] !== undefined)
                                ? root.slotToModelIndex[index] : -1
                    entryMap: (modelIndex >= 0) ? root.entryAt(modelIndex) : null
                    hasEntry: modelIndex >= 0 && entryMap !== null
                    draggingThis: root.dragActive && root.dragFromSlot === index
                    selected: root.isSelected(modelIndex)
                    dragActive: root.dragActive
                    dragToSlot: root.dragToSlot
                }
            }

            ShellSelectionRect {
                visible: root.selectionDragActive
                z: 300
                startX: root.selectionStartX
                startY: root.selectionStartY
                endX: root.selectionCurrentX
                endY: root.selectionCurrentY
            }
        }
    }

    ShellDragGhost {
        id: dragGhost
        visible: root.dragActive
        z: 400
        pointerX: root.dragPointerX
        pointerY: root.dragPointerY
        entryName: root.dragEntryName
        entryIconName: root.dragEntryIconName
        entryIconSource: root.dragEntryIconSource
        tileWidth: root.tileWidth
        tileHeight: root.tileHeight
    }
}
