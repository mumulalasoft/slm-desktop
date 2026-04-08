.pragma library

function openShellContextMenu(shell, x, y) {
    var px = Math.max(8, Math.min(shell.width - shell.shellContextMenuWidth - 8, Math.round(x)))
    var py = Math.max(shell.desktopScene.panelHeight + 4,
                      Math.min(shell.height - shell.shellContextMenuHeight - 8, Math.round(y)))
    shell.shellContextMenuX = px
    shell.shellContextMenuY = py
    shell.shellContextMenuOpen = true
    shell.shellContextMenuOpenedAtMs = Date.now()
}

function closeShellContextMenu(shell) {
    if (!shell.shellContextMenuOpen) {
        return
    }
    shell.shellContextMenuOpen = false
}

function refreshMotionDebugRows(shell) {
    if (!shell.motionDebugOverlayEnabled) {
        shell.motionDebugRows = []
        return
    }
    if (typeof MotionController === "undefined" || !MotionController || !MotionController.channelsSnapshot) {
        shell.motionDebugRows = []
        return
    }
    shell.motionDebugRows = MotionController.channelsSnapshot()
}

function applyMotionTimeScale(shell) {
    var s = Number(shell.motionTimeScale)
    if (isNaN(s)) {
        s = 1.0
    }
    s = Math.max(0.05, Math.min(2.0, s))
    shell.motionTimeScale = s
    if (typeof MotionController !== "undefined" && MotionController && MotionController.setTimeScale) {
        MotionController.setTimeScale(s)
    }
    if (typeof MotionController !== "undefined" && MotionController && MotionController.setReducedMotion) {
        MotionController.setReducedMotion(!!shell.motionReducedEnabled)
    }
}

function normalizedUserFontScale(value) {
    var raw = value
    if (typeof raw === "string") {
        var s = String(raw).trim()
        if (s.endsWith("%")) {
            s = s.substring(0, s.length - 1)
        }
        raw = Number(s)
        if (isFinite(raw) && raw > 1.8) {
            raw = raw / 100.0
        }
    }
    var n = Number(raw)
    if (!isFinite(n) || n <= 0) {
        n = 1.0
    }
    return Math.max(0.9, Math.min(1.25, n))
}

function applyUserFontScalePreference(shell) {
    if (typeof Theme === "undefined" || !Theme) {
        return
    }
    if (typeof DesktopSettings === "undefined" || !DesktopSettings) {
        Theme.userFontScale = 1.25
        return
    }
    var v = DesktopSettings.fontScale
    Theme.userFontScale = normalizedUserFontScale(v)
}

function clampTothespotGeometry(shell, geom) {
    var minW = 540
    var minH = 360
    var maxW = Math.max(minW, shell.width - 24)
    var maxH = Math.max(minH, shell.height - shell.desktopScene.panelHeight - 24)
    var w = Math.max(minW, Math.min(maxW, Number(geom.width || minW)))
    var h = Math.max(minH, Math.min(maxH, Number(geom.height || minH)))
    var minX = shell.x + 8
    var maxX = shell.x + shell.width - w - 8
    if (maxX < minX) {
        maxX = minX
    }
    var minY = shell.y + shell.desktopScene.panelHeight + 8
    var maxY = shell.y + shell.height - h - 8
    if (maxY < minY) {
        maxY = minY
    }
    var x = Math.max(minX, Math.min(maxX, Number(geom.x || minX)))
    var y = Math.max(minY, Math.min(maxY, Number(geom.y || minY)))
    return ({ "x": Math.round(x), "y": Math.round(y), "width": Math.round(w), "height": Math.round(h) })
}

function restoreTothespotGeometry(shell) {
    if (!shell.tothespotWindow) {
        return
    }
    var defaultW = Math.min(760, Math.max(540, shell.width - 120))
    var defaultH = Math.min(520, Math.max(360, shell.height - 180))
    var candidate = {
        "width": (shell.tothespotSavedWidth > 0 ? shell.tothespotSavedWidth : defaultW),
        "height": (shell.tothespotSavedHeight > 0 ? shell.tothespotSavedHeight : defaultH)
    }
    var centeredX = shell.x + Math.round((shell.width - Number(candidate.width)) / 2)
    var centeredY = shell.y + Math.max(shell.desktopScene.panelHeight + 12, Math.round(shell.height * 0.08))
    candidate.x = (shell.tothespotSavedX >= 0 ? shell.tothespotSavedX : centeredX)
    candidate.y = (shell.tothespotSavedY >= 0 ? shell.tothespotSavedY : centeredY)
    var clamped = clampTothespotGeometry(shell, candidate)
    shell.tothespotRestoringGeometry = true
    shell.tothespotWindow.width = clamped.width
    shell.tothespotWindow.height = clamped.height
    shell.tothespotWindow.x = clamped.x
    shell.tothespotWindow.y = clamped.y
    shell.tothespotRestoringGeometry = false
    saveTothespotGeometry(shell)
}

function saveTothespotGeometry(shell) {
    if (shell.tothespotRestoringGeometry || !shell.tothespotWindow) {
        return
    }
    var clamped = clampTothespotGeometry(shell, {
                                             "x": shell.tothespotWindow.x,
                                             "y": shell.tothespotWindow.y,
                                             "width": shell.tothespotWindow.width,
                                             "height": shell.tothespotWindow.height
                                         })
    shell.tothespotSavedX = clamped.x
    shell.tothespotSavedY = clamped.y
    shell.tothespotSavedWidth = clamped.width
    shell.tothespotSavedHeight = clamped.height
    if (typeof DesktopSettings !== "undefined" && DesktopSettings && DesktopSettings.setSettingValue) {
        DesktopSettings.setSettingValue("tothespot.windowX", shell.tothespotSavedX)
        DesktopSettings.setSettingValue("tothespot.windowY", shell.tothespotSavedY)
        DesktopSettings.setSettingValue("tothespot.windowWidth", shell.tothespotSavedWidth)
        DesktopSettings.setSettingValue("tothespot.windowHeight", shell.tothespotSavedHeight)
    }
}

function openDetachedFileManager(shell, pathValue) {
    shell.detachedFileManagerPath = String(pathValue && String(pathValue).length > 0 ? pathValue : "~")
    shell.detachedFileManagerVisible = true
    Qt.callLater(function() {
        if (!shell.detachedFileManagerWindow) {
            return
        }
        var item = shell.detachedFileManagerWindow.openPathIfReady(shell.detachedFileManagerPath)
        if (item) {
            shell.fileManagerContent = item
        }
    })
}

function tryOpenDetachedPropertiesNow(shell, pathValue) {
    var p = String(pathValue || "").trim()
    if (p.length === 0) {
        return false
    }
    return !!(shell.detachedFileManagerWindow && shell.detachedFileManagerWindow.showPropertiesIfReady(p))
}

function requestDetachedProperties(shell, pathValue) {
    var p = String(pathValue || "").trim()
    if (p.length === 0) {
        return
    }
    shell.pendingDetachedFileManagerPropertiesPath = p
    if (tryOpenDetachedPropertiesNow(shell, p)) {
        shell.pendingDetachedFileManagerPropertiesPath = ""
        return
    }
    Qt.callLater(function() {
        if (shell.pendingDetachedFileManagerPropertiesPath.length <= 0) {
            return
        }
        if (tryOpenDetachedPropertiesNow(shell, shell.pendingDetachedFileManagerPropertiesPath)) {
            shell.pendingDetachedFileManagerPropertiesPath = ""
        }
    })
}

function tothespotIconForMime(mimeType, isDir, fallbackIcon) {
    if (isDir) {
        return "folder"
    }
    var fallback = String(fallbackIcon || "")
    if (fallback.length > 0 && fallback !== "text-x-generic") {
        return fallback
    }
    var m = String(mimeType || "").toLowerCase()
    if (m.length === 0) {
        return "text-x-generic"
    }
    if (m.indexOf("image/") === 0) return "image-x-generic"
    if (m.indexOf("video/") === 0) return "video-x-generic"
    if (m.indexOf("audio/") === 0) return "audio-x-generic"
    if (m.indexOf("text/") === 0) return "text-x-generic"
    if (m === "application/pdf") return "application-pdf"
    if (m.indexOf("zip") >= 0 || m.indexOf("x-tar") >= 0 || m.indexOf("compressed") >= 0)
        return "package-x-generic"
    if (m.indexOf("msword") >= 0 || m.indexOf("officedocument.wordprocessingml") >= 0)
        return "x-office-document"
    if (m.indexOf("spreadsheet") >= 0 || m.indexOf("officedocument.spreadsheetml") >= 0)
        return "x-office-spreadsheet"
    if (m.indexOf("presentation") >= 0 || m.indexOf("officedocument.presentationml") >= 0)
        return "x-office-presentation"
    return "text-x-generic"
}
