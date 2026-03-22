.pragma library

function formatDateTimeHuman(qtObject, isoValue) {
    var s = String(isoValue || "")
    if (s.length <= 0) {
        return "-"
    }
    var d = new Date(s)
    if (isNaN(d.getTime())) {
        return s
    }
    return qtObject.formatDateTime(d, "yyyy-MM-dd HH:mm:ss")
}

function fileTypeDisplay(statValue, entryValue) {
    if (!!(statValue && statValue.isDir)) {
        return "Folder"
    }
    var mime = String((statValue && statValue.mimeType) ? statValue.mimeType : "")
    var suffix = String((entryValue && entryValue.suffix) ? entryValue.suffix : "")
    if (mime.indexOf("image/") === 0) {
        var mimeHead = ""
        if (mime.indexOf("/") > 0) {
            mimeHead = String(mime.split("/")[1] || "").toUpperCase()
        }
        var head = mimeHead.length > 0 ? mimeHead : (suffix.length > 0 ? suffix.toUpperCase() : "Image")
        return head + " image"
    }
    if (mime.indexOf("/") > 0) {
        var tail = mime.split("/")[1]
        tail = tail.replace(/[-_]+/g, " ")
        if (tail.length > 0) {
            return tail.charAt(0).toUpperCase() + tail.slice(1)
        }
    }
    if (suffix.length > 0) {
        return suffix.toUpperCase() + " file"
    }
    return "File"
}

function locationDisplay(root, statValue, entryValue) {
    var p = String((statValue && statValue.absolutePath)
                   ? statValue.absolutePath
                   : ((entryValue && entryValue.path) ? entryValue.path : ""))
    if (p.length <= 0) {
        return ""
    }
    if (!!(statValue && statValue.isDir)) {
        return p
    }
    return root.parentPathOf(p)
}
