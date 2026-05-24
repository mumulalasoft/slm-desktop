.pragma library

function scaleForDistance(distance, hoverActive) {
    if (!hoverActive) {
        return 1.0
    }
    var radius = 192.0
    if (distance >= radius) {
        return 1.0
    }
    var t = distance / radius
    var gaussian = Math.exp(-3.45 * t * t)
    return 1.0 + (0.64 * gaussian)
}

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
