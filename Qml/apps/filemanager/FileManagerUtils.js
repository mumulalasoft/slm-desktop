.pragma library

function basename(pathValue) {
    var p = String(pathValue || "")
    var idx = p.lastIndexOf("/")
    if (idx < 0) {
        return p
    }
    return p.slice(idx + 1)
}

function isImageName(nameValue) {
    var s = String(nameValue || "").toLowerCase()
    return s.endsWith(".png") || s.endsWith(".jpg") || s.endsWith(".jpeg") ||
           s.endsWith(".gif") || s.endsWith(".bmp") || s.endsWith(".webp") ||
           s.endsWith(".svg") || s.endsWith(".avif") || s.endsWith(".heic")
}

function isVideoName(nameValue) {
    var s = String(nameValue || "").toLowerCase()
    return s.endsWith(".mp4") || s.endsWith(".mkv") || s.endsWith(".webm") ||
           s.endsWith(".mov") || s.endsWith(".avi") || s.endsWith(".m4v") ||
           s.endsWith(".flv") || s.endsWith(".wmv") || s.endsWith(".mpeg") ||
           s.endsWith(".mpg") || s.endsWith(".3gp") || s.endsWith(".ts") ||
           s.endsWith(".m2ts") || s.endsWith(".ogv")
}

function isPdfName(nameValue) {
    return String(nameValue || "").toLowerCase().endsWith(".pdf")
}

function isPreviewCandidateName(nameValue) {
    return isImageName(nameValue) || isVideoName(nameValue) || isPdfName(nameValue)
}

function toFileUrl(pathValue) {
    var p = String(pathValue || "")
    if (p.length === 0) {
        return ""
    }
    if (p.indexOf("://") >= 0) {
        return p
    }
    return "file://" + encodeURI(p)
}

function relativeTimeText(isoDateTime) {
    var s = String(isoDateTime || "")
    if (s.length === 0) {
        return ""
    }
    var t = Date.parse(s)
    if (isNaN(t)) {
        return ""
    }
    var now = Date.now()
    var diffSec = Math.max(0, Math.floor((now - t) / 1000))
    if (diffSec < 10) {
        return "now"
    }
    if (diffSec < 60) {
        return diffSec + "s"
    }
    var minutes = Math.floor(diffSec / 60)
    if (minutes < 60) {
        return minutes + "m"
    }
    var hours = Math.floor(minutes / 60)
    if (hours < 24) {
        return hours + "h"
    }
    if (hours < 48) {
        return "yesterday"
    }
    var days = Math.floor(hours / 24)
    if (days < 7) {
        return days + "d"
    }
    var weeks = Math.floor(days / 7)
    if (weeks < 5) {
        return weeks + "w"
    }
    var months = Math.floor(days / 30)
    if (months < 12) {
        return months + "mo"
    }
    var years = Math.floor(days / 365)
    return years + "y"
}

function dayBucket(isoDateTime) {
    var s = String(isoDateTime || "")
    if (s.length === 0) {
        return "older"
    }
    var t = Date.parse(s)
    if (isNaN(t)) {
        return "older"
    }
    var nowDate = new Date()
    var itemDate = new Date(t)
    var startToday = new Date(nowDate.getFullYear(), nowDate.getMonth(), nowDate.getDate()).getTime()
    var startItem = new Date(itemDate.getFullYear(), itemDate.getMonth(), itemDate.getDate()).getTime()
    var dayDiff = Math.floor((startToday - startItem) / (24 * 3600 * 1000))
    if (dayDiff <= 0) {
        return "today"
    }
    if (dayDiff === 1) {
        return "yesterday"
    }
    return "older"
}

function recentCountForBucket(model, bucketName) {
    var c = 0
    for (var i = 0; i < model.count; ++i) {
        var row = model.get(i)
        if (row && dayBucket(String(row.lastOpened || "")) === bucketName) {
            c += 1
        }
    }
    return c
}

function recentBucketTitle(bucketName) {
    if (bucketName === "today") {
        return "Today"
    }
    if (bucketName === "yesterday") {
        return "Yesterday"
    }
    return "Older"
}

function shouldShowRecentHeader(model, index, lastOpened) {
    var current = dayBucket(lastOpened)
    if (index <= 0) {
        return true
    }
    var prev = model.get(index - 1)
    if (!prev) {
        return true
    }
    return dayBucket(String(prev.lastOpened || "")) !== current
}

function buildTargetPath(dirPath, fileName, attempt) {
    var dir = String(dirPath || "")
    var name = String(fileName || "")
    if (attempt <= 0) {
        return dir + "/" + name
    }
    var dot = name.lastIndexOf(".")
    if (dot > 0) {
        return dir + "/" + name.slice(0, dot) + " copy " + attempt + name.slice(dot)
    }
    return dir + "/" + name + " copy " + attempt
}
