.pragma library

function baseName(pathValue) {
    var p = String(pathValue || "")
    var slash = Math.max(p.lastIndexOf("/"), p.lastIndexOf("\\"))
    var file = slash >= 0 ? p.substring(slash + 1) : p
    if (file.length === 0) {
        return "Screenshot"
    }
    var dot = file.lastIndexOf(".")
    return dot > 0 ? file.substring(0, dot) : file
}

function dirName(pathValue) {
    var p = String(pathValue || "")
    var slash = Math.max(p.lastIndexOf("/"), p.lastIndexOf("\\"))
    if (slash <= 0) {
        return "/tmp"
    }
    return p.substring(0, slash)
}

function normalizeErrorCode(errorText) {
    var e = String(errorText || "").trim()
    if (e.length <= 0) {
        return "save-failed"
    }
    if (e === "invalid-filename"
            || e === "overwrite-canceled"
            || e === "save-failed"
            || e === "io-error"
            || e === "invalid-args"
            || e === "source-missing"
            || e === "mkdir-failed"
            || e === "target-exists"
            || e === "overwrite-remove-failed"
            || e === "permission-denied"
            || e === "no-space"
            || e === "target-busy"
            || e === "unsupported-format"
            || e === "invalid-image") {
        return e
    }
    if (e === "Filename is empty" || e === "Invalid filename" || e === "Filename contains invalid characters") {
        return "invalid-filename"
    }
    if (e === "canceled") {
        return "overwrite-canceled"
    }
    var low = e.toLowerCase()
    if (low.indexOf("permission") >= 0
            || low.indexOf("denied") >= 0
            || low.indexOf("read-only") >= 0
            || low.indexOf("no space") >= 0
            || low.indexOf("i/o") >= 0
            || low.indexOf("io-") >= 0) {
        return "io-error"
    }
    return "save-failed"
}

function errorMessage(errorCode, rawError) {
    var code = String(errorCode || "save-failed")
    if (code === "invalid-filename") {
        return "Invalid filename"
    }
    if (code === "overwrite-canceled") {
        return "Overwrite canceled"
    }
    if (code === "invalid-args") {
        return "Invalid save parameters"
    }
    if (code === "source-missing") {
        return "Captured image is missing"
    }
    if (code === "mkdir-failed") {
        return "Cannot create destination folder"
    }
    if (code === "target-exists") {
        return "Target file already exists"
    }
    if (code === "overwrite-remove-failed") {
        return "Cannot overwrite existing file"
    }
    if (code === "permission-denied") {
        return "Permission denied while saving screenshot"
    }
    if (code === "no-space") {
        return "No space left on destination"
    }
    if (code === "target-busy") {
        return "Destination is busy or locked"
    }
    if (code === "unsupported-format") {
        return "Output format is not supported"
    }
    if (code === "invalid-image") {
        return "Captured image data is invalid"
    }
    if (code === "io-error") {
        return "I/O error while saving screenshot"
    }
    if (String(rawError || "").trim().length > 0 && code !== "save-failed") {
        return String(rawError)
    }
    return "Failed to save screenshot"
}
