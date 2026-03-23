.pragma library

function entryCount(rawCount) {
    return Number(rawCount || 0)
}

function currentIndexOrZero(selectedIndex) {
    return (Number(selectedIndex) >= 0) ? Number(selectedIndex) : 0
}

function canHandleListShortcut(inputFocused, requireMultiple, allowMultiple, totalCount) {
    if (inputFocused) {
        return false
    }
    if (requireMultiple && !allowMultiple) {
        return false
    }
    return Number(totalCount || 0) > 0
}

function nextIndex(selectedIndex, delta) {
    return currentIndexOrZero(selectedIndex) + Number(delta || 0)
}

function boundaryTarget(totalCount, atEnd) {
    var total = Number(totalCount || 0)
    return atEnd ? (total - 1) : 0
}
