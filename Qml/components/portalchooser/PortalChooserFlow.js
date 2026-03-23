.pragma library

function parseFilters(incomingFilters) {
    var out = []
    var rows = incomingFilters || []
    for (var i = 0; i < rows.length; ++i) {
        var f = rows[i]
        if (!f) {
            continue
        }
        var label = String(f.label || "Files")
        var patterns = f.patterns || []
        if (patterns.length <= 0 && f.pattern) {
            patterns = [String(f.pattern)]
        }
        out.push({
                     "label": label,
                     "patterns": patterns
                 })
    }
    return out
}

function countSelected(selectedMap) {
    var map = selectedMap || ({})
    var keys = Object.keys(map)
    var count = 0
    for (var i = 0; i < keys.length; ++i) {
        if (map[keys[i]] === true) {
            count += 1
        }
    }
    return count
}

function collectSelectedPaths(selectedMap) {
    var selected = []
    var map = selectedMap || ({})
    var keys = Object.keys(map)
    for (var i = 0; i < keys.length; ++i) {
        var k = keys[i]
        if (map[k] === true) {
            selected.push(k)
        }
    }
    return selected
}

function withFolderFallback(selected, selectFolders, currentDir) {
    var out = selected || []
    if (!!selectFolders && out.length <= 0) {
        var folder = String(currentDir || "").trim()
        if (folder.length > 0) {
            out = [folder]
        }
    }
    return out
}

function validateSaveName(nameValue) {
    var name = String(nameValue || "").trim()
    if (name.length <= 0) {
        return ({ "ok": false, "error": "Filename is required" })
    }
    if (name === "." || name === "..") {
        return ({ "ok": false, "error": "Invalid filename" })
    }
    if (name.indexOf("/") >= 0 || name.indexOf("\\") >= 0) {
        return ({ "ok": false, "error": "Filename cannot contain path separators" })
    }
    if (name.indexOf("\u0000") >= 0) {
        return ({ "ok": false, "error": "Filename contains invalid character" })
    }
    return ({ "ok": true, "error": "" })
}

function saveSelection(mode, chooserName, currentDir, selected) {
    var result = {
        "ok": true,
        "error": "",
        "selected": selected || []
    }
    if (String(mode || "") !== "save") {
        return result
    }
    var name = String(chooserName || "").trim()
    if (name.length <= 0) {
        return result
    }
    var check = validateSaveName(name)
    if (!check.ok) {
        return {
            "ok": false,
            "error": String(check.error || "Invalid filename"),
            "selected": selected || []
        }
    }
    return {
        "ok": true,
        "error": "",
        "selected": [String(currentDir || "") + "/" + name]
    }
}

function buildUris(paths) {
    var out = []
    var rows = paths || []
    for (var i = 0; i < rows.length; ++i) {
        out.push("file://" + rows[i])
    }
    return out
}

function buildResultPayload(ok, canceled, errorText, mode, currentDir, selected) {
    var paths = selected || []
    var uris = buildUris(paths)
    return {
        "ok": !!ok,
        "canceled": !!canceled,
        "error": String(errorText || ""),
        "mode": String(mode || ""),
        "currentDir": String(currentDir || ""),
        "paths": paths,
        "uris": uris,
        "path": paths.length > 0 ? paths[0] : "",
        "uri": uris.length > 0 ? uris[0] : ""
    }
}
