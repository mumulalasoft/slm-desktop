import QtQuick 2.15

QtObject {
    id: root

    property real workX: 0
    property real workY: 0
    property real workWidth: 1
    property real workHeight: 1
    property real cellWidth: 104
    property real cellHeight: 122
    property string fillMode: "vertical-first" // vertical-first | horizontal-first
    property string anchor: "top-left" // top-left | top-right | bottom-left | bottom-right

    readonly property int columns: Math.max(1, Math.floor(Math.max(1, workWidth) / Math.max(1, cellWidth)))
    readonly property int rows: Math.max(1, Math.floor(Math.max(1, workHeight) / Math.max(1, cellHeight)))
    readonly property int capacity: Math.max(1, columns * rows)

    function isValidCell(cellX, cellY) {
        return Number.isInteger(cellX)
                && Number.isInteger(cellY)
                && cellX >= 0
                && cellY >= 0
                && cellX < columns
                && cellY < rows
    }

    function cellToSlot(cellX, cellY) {
        if (!isValidCell(cellX, cellY)) {
            return -1
        }
        return (cellY * columns) + cellX
    }

    function slotToCell(slot) {
        var s = Number(slot)
        if (!Number.isInteger(s) || s < 0) {
            return ({ "valid": false, "cellX": -1, "cellY": -1, "slot": -1 })
        }
        var cellX = s % columns
        var cellY = Math.floor(s / columns)
        var ok = isValidCell(cellX, cellY)
        return ({ "valid": ok, "cellX": cellX, "cellY": cellY, "slot": ok ? s : -1 })
    }

    function slotToPixel(slot) {
        var cell = slotToCell(slot)
        if (!cell.valid) {
            return ({ "x": workX, "y": workY })
        }
        return ({
            "x": workX + (cell.cellX * cellWidth),
            "y": workY + (cell.cellY * cellHeight)
        })
    }

    function pointToCell(localX, localY) {
        var rx = Number(localX) - Number(workX)
        var ry = Number(localY) - Number(workY)
        var cellX = Math.floor(rx / Math.max(1, cellWidth))
        var cellY = Math.floor(ry / Math.max(1, cellHeight))
        return ({
            "cellX": cellX,
            "cellY": cellY,
            "valid": isValidCell(cellX, cellY)
        })
    }

    function orderedSlots() {
        var out = []
        var leftToRight = (anchor === "top-left" || anchor === "bottom-left")
        var topToBottom = (anchor === "top-left" || anchor === "top-right")

        var xs = []
        var ys = []

        for (var x = 0; x < columns; ++x) {
            xs.push(leftToRight ? x : (columns - 1 - x))
        }
        for (var y = 0; y < rows; ++y) {
            ys.push(topToBottom ? y : (rows - 1 - y))
        }

        if (fillMode === "horizontal-first") {
            for (var yi = 0; yi < ys.length; ++yi) {
                for (var xi = 0; xi < xs.length; ++xi) {
                    out.push(cellToSlot(xs[xi], ys[yi]))
                }
            }
        } else {
            for (var xi2 = 0; xi2 < xs.length; ++xi2) {
                for (var yi2 = 0; yi2 < ys.length; ++yi2) {
                    out.push(cellToSlot(xs[xi2], ys[yi2]))
                }
            }
        }

        return out
    }

    function _isSlotFree(slot, occupied) {
        return occupied[String(slot)] === undefined
    }

    function firstFreeSlot(occupied) {
        var slots = orderedSlots()
        for (var i = 0; i < slots.length; ++i) {
            var s = slots[i]
            if (s >= 0 && _isSlotFree(s, occupied)) {
                return s
            }
        }
        return -1
    }

    function nearestFreeSlot(preferredSlot, occupied) {
        var prefCell = slotToCell(preferredSlot)
        if (prefCell.valid && _isSlotFree(preferredSlot, occupied)) {
            return preferredSlot
        }

        var slots = orderedSlots()
        if (!prefCell.valid) {
            for (var i = 0; i < slots.length; ++i) {
                var s = slots[i]
                if (s >= 0 && _isSlotFree(s, occupied)) {
                    return s
                }
            }
            return -1
        }

        var bestSlot = -1
        var bestDist = Number.MAX_VALUE
        for (var j = 0; j < slots.length; ++j) {
            var candidate = slots[j]
            if (candidate < 0 || !_isSlotFree(candidate, occupied)) {
                continue
            }
            var cc = slotToCell(candidate)
            if (!cc.valid) {
                continue
            }
            var dx = cc.cellX - prefCell.cellX
            var dy = cc.cellY - prefCell.cellY
            var d2 = (dx * dx) + (dy * dy)
            if (d2 < bestDist) {
                bestDist = d2
                bestSlot = candidate
            }
        }
        return bestSlot
    }

    function computePlacement(entries, slotByPathIn) {
        var src = entries ? entries : []
        var mapIn = slotByPathIn ? slotByPathIn : ({})
        var occupied = ({})
        var nextSlotByPath = ({})
        var placed = []

        var pending = []
        for (var i = 0; i < src.length; ++i) {
            var row = src[i] || ({})
            var path = String(row.path || "")
            if (path.length <= 0) {
                continue
            }
            var preferred = Number(mapIn[path])
            if (Number.isInteger(preferred) && preferred >= 0) {
                var cell = slotToCell(preferred)
                if (cell.valid && _isSlotFree(preferred, occupied)) {
                    occupied[String(preferred)] = path
                    nextSlotByPath[path] = preferred
                    placed.push({
                        "modelIndex": i,
                        "path": path,
                        "slot": preferred,
                        "cellX": cell.cellX,
                        "cellY": cell.cellY,
                        "x": workX + (cell.cellX * cellWidth),
                        "y": workY + (cell.cellY * cellHeight)
                    })
                    continue
                }
            }
            pending.push({ "modelIndex": i, "path": path })
        }

        for (var p = 0; p < pending.length; ++p) {
            var rowPending = pending[p]
            var freeSlot = firstFreeSlot(occupied)
            if (freeSlot < 0) {
                break
            }
            var freeCell = slotToCell(freeSlot)
            occupied[String(freeSlot)] = rowPending.path
            nextSlotByPath[rowPending.path] = freeSlot
            placed.push({
                "modelIndex": rowPending.modelIndex,
                "path": rowPending.path,
                "slot": freeSlot,
                "cellX": freeCell.cellX,
                "cellY": freeCell.cellY,
                "x": workX + (freeCell.cellX * cellWidth),
                "y": workY + (freeCell.cellY * cellHeight)
            })
        }

        return ({
            "placed": placed,
            "slotByPath": nextSlotByPath
        })
    }

    function movePathToCell(pathValue, targetCellX, targetCellY, entries, slotByPathIn) {
        var path = String(pathValue || "")
        var slotByPath = slotByPathIn ? Object.assign({}, slotByPathIn) : ({})
        if (path.length <= 0 || !isValidCell(targetCellX, targetCellY)) {
            return slotByPath
        }

        var preferredSlot = cellToSlot(targetCellX, targetCellY)
        var occupied = ({})
        var src = entries ? entries : []

        for (var i = 0; i < src.length; ++i) {
            var row = src[i] || ({})
            var p = String(row.path || "")
            if (p.length <= 0 || p === path) {
                continue
            }
            var s = Number(slotByPath[p])
            if (Number.isInteger(s) && s >= 0) {
                var cell = slotToCell(s)
                if (cell.valid && occupied[String(s)] === undefined) {
                    occupied[String(s)] = p
                }
            }
        }

        var chosen = nearestFreeSlot(preferredSlot, occupied)
        if (chosen < 0) {
            chosen = firstFreeSlot(occupied)
        }
        if (chosen >= 0) {
            slotByPath[path] = chosen
        }
        return slotByPath
    }

    function cleanUp(entries) {
        var src = entries ? entries : []
        var next = ({})
        var slots = orderedSlots()
        for (var i = 0; i < src.length && i < slots.length; ++i) {
            var row = src[i] || ({})
            var path = String(row.path || "")
            if (path.length <= 0) {
                continue
            }
            next[path] = slots[i]
        }
        return next
    }
}
