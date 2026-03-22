.pragma library

function isPrimaryModifier(modifiers, qtObject) {
    var m = Number(modifiers || 0)
    return (m & qtObject.ControlModifier) || (m & qtObject.MetaModifier)
}

function isTextEntryFocusItem(item) {
    if (!item) {
        return false
    }
    if (item.hasOwnProperty("cursorPosition") || item.hasOwnProperty(
                "selectedText") || item.hasOwnProperty("echoMode")) {
        return true
    }
    var typeName = item.toString ? String(item.toString()) : ""
    return typeName.indexOf("TextField") >= 0
            || typeName.indexOf("TextInput") >= 0
            || typeName.indexOf("TextArea") >= 0
            || typeName.indexOf("SpinBox") >= 0
            || typeName.indexOf("ComboBox") >= 0
}

function isEditingTextInput(root) {
    if (!root) {
        return false
    }
    // Window attached property can be unavailable during early creation.
    var focusItem = null
    if (root.hostWindow && root.hostWindow.activeFocusItem !== undefined) {
        focusItem = root.hostWindow.activeFocusItem
    } else if (root.Window && root.Window.activeFocusItem !== undefined) {
        focusItem = root.Window.activeFocusItem
    }
    return isTextEntryFocusItem(focusItem)
}

function clearSelection(root) {
    root.selectedEntryIndex = -1
    root.selectedEntryIndexes = []
    root.selectionAnchorIndex = -1
}

function setSingleSelection(root, indexValue) {
    var idx = Number(indexValue)
    if (idx < 0) {
        clearSelection(root)
        return
    }
    root.selectedEntryIndex = idx
    root.selectedEntryIndexes = [idx]
    root.selectionAnchorIndex = idx
}

function normalizeSelection(root, indexesValue) {
    var seen = ({})
    var out = []
    var arr = indexesValue || []
    for (var i = 0; i < arr.length; ++i) {
        var idx = Number(arr[i])
        if (!(idx >= 0)) {
            continue
        }
        var key = String(idx)
        if (seen[key]) {
            continue
        }
        seen[key] = true
        out.push(idx)
    }
    out.sort(function (a, b) {
        return a - b
    })
    root.selectedEntryIndexes = out
    if (out.length <= 0) {
        root.selectedEntryIndex = -1
        root.selectionAnchorIndex = -1
        return
    }
    if (out.indexOf(root.selectedEntryIndex) < 0) {
        root.selectedEntryIndex = out[out.length - 1]
    }
    if (root.selectionAnchorIndex < 0) {
        root.selectionAnchorIndex = root.selectedEntryIndex
    }
}

function applySelectionRequest(root, qtObject, indexValue, modifiers) {
    var idx = Number(indexValue)
    if (idx < 0) {
        clearSelection(root)
        return
    }
    var mod = Number(modifiers || 0)
    var usePrimary = isPrimaryModifier(mod, qtObject)
    var useShift = !!(mod & qtObject.ShiftModifier)
    if (useShift) {
        var anchor = root.selectionAnchorIndex >= 0
                ? root.selectionAnchorIndex
                : (root.selectedEntryIndex >= 0 ? root.selectedEntryIndex : idx)
        var start = Math.min(anchor, idx)
        var end = Math.max(anchor, idx)
        var range = []
        for (var r = start; r <= end; ++r) {
            range.push(r)
        }
        if (usePrimary) {
            normalizeSelection(root, (root.selectedEntryIndexes || []).concat(range))
        } else {
            normalizeSelection(root, range)
        }
        root.selectedEntryIndex = idx
        return
    }
    if (usePrimary) {
        var next = (root.selectedEntryIndexes || []).slice(0)
        var pos = next.indexOf(idx)
        if (pos >= 0) {
            next.splice(pos, 1)
            normalizeSelection(root, next)
        } else {
            next.push(idx)
            normalizeSelection(root, next)
            root.selectedEntryIndex = idx
            root.selectionAnchorIndex = idx
        }
        return
    }
    setSingleSelection(root, idx)
}

function selectAllVisibleEntries(root) {
    if (!root.fileModel || root.fileModel.count === undefined) {
        return
    }
    var n = Number(root.fileModel.count || 0)
    if (n <= 0) {
        clearSelection(root)
        return
    }
    var all = []
    for (var i = 0; i < n; ++i) {
        all.push(i)
    }
    root.selectedEntryIndexes = all
    root.selectedEntryIndex = all[all.length - 1]
    root.selectionAnchorIndex = 0
}

function applySelectionRect(root, qtObject, indexesValue, modifiers, anchorIndex) {
    var indexes = indexesValue || []
    var mod = Number(modifiers || 0)
    if (isPrimaryModifier(mod, qtObject)) {
        normalizeSelection(root, (root.selectedEntryIndexes || []).concat(indexes))
    } else {
        normalizeSelection(root, indexes)
    }
    if (indexes.length > 0) {
        root.selectedEntryIndex = Number(indexes[indexes.length - 1])
        var anchor = Number(anchorIndex)
        if (anchor >= 0) {
            root.selectionAnchorIndex = anchor
        }
    }
}

function handleTypeToSelect(root, textValue, quickTypeClearTimer, contentView) {
    if (!root.fileModel || !root.fileModel.entryAt
            || root.fileModel.count === undefined) {
        return false
    }
    var t = String(textValue || "")
    if (t.length <= 0) {
        return false
    }
    quickTypeClearTimer.restart()
    root.quickTypeBuffer = String(root.quickTypeBuffer || "") + t.toLowerCase()

    var count = Number(root.fileModel.count || 0)
    if (count <= 0) {
        return false
    }
    var start = root.selectedEntryIndex >= 0 ? (root.selectedEntryIndex + 1) : 0
    var idx = -1
    for (var i = 0; i < count; ++i) {
        var probe = (start + i) % count
        var row = root.fileModel.entryAt(probe)
        var name = row && row.ok ? String(row.name || "") : ""
        if (name.toLowerCase().indexOf(root.quickTypeBuffer) === 0) {
            idx = probe
            break
        }
    }
    if (idx < 0 && root.quickTypeBuffer.length > 1) {
        root.quickTypeBuffer = root.quickTypeBuffer.charAt(
                    root.quickTypeBuffer.length - 1)
        for (var j = 0; j < count; ++j) {
            var probe2 = (start + j) % count
            var row2 = root.fileModel.entryAt(probe2)
            var name2 = row2 && row2.ok ? String(row2.name || "") : ""
            if (name2.toLowerCase().indexOf(root.quickTypeBuffer) === 0) {
                idx = probe2
                break
            }
        }
    }
    if (idx < 0) {
        return false
    }
    setSingleSelection(root, idx)
    if (contentView && contentView.positionSelection) {
        contentView.positionSelection(idx, false)
    }
    return true
}
