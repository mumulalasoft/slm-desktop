.pragma library

function basename(pathValue) {
    var p = String(pathValue || "")
    var idx = p.lastIndexOf("/")
    if (idx < 0) {
        return p
    }
    return p.slice(idx + 1)
}

function parentPathOf(pathValue) {
    var p = String(pathValue || "")
    while (p.length > 1 && p.charAt(p.length - 1) === "/") {
        p = p.slice(0, p.length - 1)
    }
    var idx = p.lastIndexOf("/")
    if (idx <= 0) {
        return "/"
    }
    return p.slice(0, idx)
}

function isPathSameOrInside(pathValue, parentValue) {
    var path = String(pathValue || "")
    var parent = String(parentValue || "")
    if (path.length <= 0 || parent.length <= 0) {
        return false
    }
    while (path.length > 1 && path.charAt(path.length - 1) === "/") {
        path = path.slice(0, path.length - 1)
    }
    while (parent.length > 1 && parent.charAt(parent.length - 1) === "/") {
        parent = parent.slice(0, parent.length - 1)
    }
    return path === parent || path.indexOf(parent + "/") === 0
}

function shellSingleQuote(textValue) {
    var s = String(textValue || "")
    return "'" + s.replace(/'/g, "'\"'\"'") + "'"
}
