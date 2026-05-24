import QtQuick 2.15

QtObject {
    id: root

    property int draggingFromIndex: -1
    property int draggingToIndex: -1
    property real draggingDeltaX: 0
    property real dragStartMarkerCenterX: -1000
    property real reorderMarkerX: -1000
    property string dragPreviewIconPath: ""

    function targetIndexFromDelta(fromIndex, deltaX, maxPinned) {
        if (fromIndex < 0 || maxPinned < 0) {
            return -1
        }
        var slotWidth = 76
        var steps = Math.round(deltaX / slotWidth)
        return Math.max(0, Math.min(maxPinned, fromIndex + steps))
    }

    function markerXFromCenter(centerX, markerWidth) {
        if (centerX < -500) {
            return -1000
        }
        return centerX - (markerWidth * 0.5)
    }

    function insertionPinnedPosFromLocalX(localX, centers) {
        if (!centers || centers.length === 0) {
            return 0
        }
        for (var c = 0; c < centers.length; ++c) {
            if (localX < centers[c]) {
                return c
            }
        }
        return centers.length
    }

    function markerCenterX(pinnedCenters) {
        if (draggingFromIndex < 0) {
            return -1000
        }
        var sourceCenter = dragStartMarkerCenterX
        if (sourceCenter < 0) {
            return -1000
        }
        var desiredCenter = sourceCenter + draggingDeltaX

        if (!pinnedCenters || pinnedCenters.length === 0) {
            return -1000
        }

        var minCenter = Number.MAX_VALUE
        var maxCenter = -Number.MAX_VALUE
        for (var i = 0; i < pinnedCenters.length; ++i) {
            minCenter = Math.min(minCenter, pinnedCenters[i])
            maxCenter = Math.max(maxCenter, pinnedCenters[i])
        }

        if (minCenter === Number.MAX_VALUE) {
            return -1000
        }
        return Math.max(minCenter, Math.min(maxCenter, desiredCenter))
    }

    function startDrag(modelIndex, sourceCenter, iconPath, markerWidth) {
        dragStartMarkerCenterX = sourceCenter
        reorderMarkerX = markerXFromCenter(sourceCenter, markerWidth)
        draggingFromIndex = modelIndex
        draggingToIndex = modelIndex
        draggingDeltaX = 0
        dragPreviewIconPath = iconPath || ""
    }

    function moveDrag(deltaX, maxPinned, pinnedCenters, markerWidth) {
        if (draggingFromIndex < 0) {
            return
        }
        draggingDeltaX = deltaX
        var target = targetIndexFromDelta(draggingFromIndex, deltaX, maxPinned)
        if (target >= 0) {
            draggingToIndex = target
        }
        reorderMarkerX = markerXFromCenter(markerCenterX(pinnedCenters), markerWidth)
    }

    function finishDrag(deltaX, maxPinned) {
        if (draggingFromIndex < 0) {
            return { valid: false, fromIndex: -1, target: -1 }
        }
        var fromIndex = draggingFromIndex
        var target = targetIndexFromDelta(fromIndex, deltaX, maxPinned)
        clearDrag()
        return { valid: true, fromIndex: fromIndex, target: target }
    }

    function clearDrag() {
        draggingFromIndex = -1
        draggingToIndex = -1
        draggingDeltaX = 0
        dragStartMarkerCenterX = -1000
        reorderMarkerX = -1000
        dragPreviewIconPath = ""
    }
}
