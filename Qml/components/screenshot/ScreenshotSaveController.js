.pragma library
.import "ScreenshotFlow.js" as ScreenshotFlow

function showResultNotification(shell, ok, path, errorText) {
    if (!shell) {
        return
    }
    if (typeof NotificationManager === "undefined" || !NotificationManager || !NotificationManager.Notify) {
        return
    }
    var summary = ok ? "Screenshot saved" : "Screenshot failed"
    var code = ok ? "" : ScreenshotFlow.normalizeErrorCode(errorText)
    shell.screenshotLastErrorCode = code
    var body = ok ? String(path || "") : ScreenshotFlow.errorMessage(code, String(errorText || ""))
    log(shell, "notify", {
            "ok": !!ok,
            "path": String(path || ""),
            "errorCode": code,
            "rawError": String(errorText || "")
        })
    NotificationManager.Notify("Slm Desktop",
                               0,
                               "camera-photo-symbolic",
                               summary,
                               body,
                               [],
                               ok ? ({
                                         "requestId": String(shell.screenshotRequestId || "")
                                     }) : ({
                                               "requestId": String(shell.screenshotRequestId || ""),
                                               "errorCode": code,
                                               "rawError": String(errorText || "")
                                           }),
                               3500)
}

function tempPath() {
    var ts = Qt.formatDateTime(new Date(), "yyyyMMdd-HHmmss-zzz")
    return "/tmp/SLM-Screenshot-" + ts + ".png"
}

function newRequestId(sourceTag) {
    var ts = Date.now()
    var suffix = Math.floor(Math.random() * 100000)
    return "shot-" + ts + "-" + suffix + "-" + String(sourceTag || "ui")
}

function beginRequest(shell, sourceTag) {
    if (!shell) {
        return ""
    }
    shell.screenshotRequestId = newRequestId(sourceTag)
    return shell.screenshotRequestId
}

function log(shell, stage, details, requestIdValue) {
    if (!shell) {
        return
    }
    var rid = String(requestIdValue || shell.screenshotRequestId || "")
    var extra = details || ({})
    console.log("[slm][screenshot] stage=" + String(stage || "")
                + " request_id=" + rid
                + " details=" + JSON.stringify(extra))
}

function defaultSaveFolder(shell) {
    if (!shell) {
        return "~/Pictures/Screenshots"
    }
    if (typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.getPreference) {
        var v = String(UIPreferences.getPreference("screenshot.saveFolder", "~/Pictures/Screenshots") || "")
        if (v.trim().length > 0) {
            return v
        }
    }
    return "~/Pictures/Screenshots"
}

function persistSaveFolder(folderValue) {
    var folder = String(folderValue || "").trim()
    if (folder.length <= 0) {
        return
    }
    if (typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.setPreference) {
        UIPreferences.setPreference("screenshot.saveFolder", folder)
    }
}

function openSaveDialog(shell, capturedPath, requestIdValue) {
    if (!shell) {
        return
    }
    var p = String(capturedPath || "")
    if (p.length === 0) {
        return
    }
    if (String(requestIdValue || "").length > 0) {
        shell.screenshotRequestId = String(requestIdValue)
    }
    shell.screenshotSaveSourcePath = p
    shell.screenshotSaveName = ScreenshotFlow.baseName(p)
    shell.screenshotSaveFolder = defaultSaveFolder(shell)
    shell.screenshotSaveTypeIndex = 0
    shell.screenshotSaveErrorCode = ""
    shell.screenshotSaveErrorHint = ""
    shell.screenshotSaveBlockedByNoSpace = false
    shell.screenshotSaveDialogVisible = true
    log(shell, "save-dialog-open", {
            "sourcePath": p,
            "folder": shell.screenshotSaveFolder
        })
}

function shouldKeepSaveDialogOpen(errorCode) {
    var code = String(errorCode || "")
    return code === "permission-denied"
            || code === "no-space"
            || code === "target-busy"
            || code === "mkdir-failed"
}

function outputTypeAt(shell, indexValue) {
    if (!shell) {
        return ({ "label": "PNG image (.png)", "ext": "png", "format": "png", "quality": -1 })
    }
    var idx = Number(indexValue)
    if (!isFinite(idx) || idx < 0 || idx >= shell.screenshotOutputTypes.length) {
        return shell.screenshotOutputTypes[0]
    }
    var item = shell.screenshotOutputTypes[idx]
    return item ? item : shell.screenshotOutputTypes[0]
}

function saveFullPath(shell) {
    if (!shell) {
        return ""
    }
    var typeInfo = outputTypeAt(shell, shell.screenshotSaveTypeIndex)
    var ext = String(typeInfo && typeInfo.ext ? typeInfo.ext : "png")
    if (typeof ScreenshotSaveHelper !== "undefined"
            && ScreenshotSaveHelper
            && ScreenshotSaveHelper.buildTargetPathWithExtension) {
        return String(ScreenshotSaveHelper.buildTargetPathWithExtension(
                          String(shell.screenshotSaveFolder || defaultSaveFolder(shell)),
                          String(shell.screenshotSaveName || ""),
                          ext))
    }
    var folder = String(shell.screenshotSaveFolder || "").trim()
    if (folder.length === 0) {
        folder = defaultSaveFolder(shell)
    }
    var name = String(shell.screenshotSaveName || "").trim()
    if (name.length === 0) {
        name = "Screenshot"
    }
    var dot = name.lastIndexOf(".")
    if (dot > 0) {
        name = name.substring(0, dot)
    }
    name += "." + ext
    return folder.endsWith("/") ? (folder + name) : (folder + "/" + name)
}

function saveNameError(nameValue) {
    if (typeof ScreenshotSaveHelper !== "undefined"
            && ScreenshotSaveHelper
            && ScreenshotSaveHelper.validateFileName) {
        return String(ScreenshotSaveHelper.validateFileName(String(nameValue || "")))
    }
    var n = String(nameValue || "").trim()
    if (n.length <= 0) return "Filename is empty"
    if (n === "." || n === "..") return "Invalid filename"
    if (/[\\\/:*?"<>|]/.test(n)) return "Filename contains invalid characters"
    return ""
}

function cancelSaveDialog(shell) {
    if (!shell) {
        return
    }
    var source = String(shell.screenshotSaveSourcePath || "")
    log(shell, "save-dialog-cancel", { "sourcePath": source })
    dismissSaveDialogs(shell)
    resetSaveDraft(shell)
    if (source.length > 0
            && typeof FileManagerApi !== "undefined"
            && FileManagerApi
            && FileManagerApi.removePath) {
        FileManagerApi.removePath(source, false)
    }
    shell.screenshotRequestId = ""
}

function dismissSaveDialogs(shell) {
    if (!shell) {
        return
    }
    shell.screenshotSaveDialogVisible = false
    shell.screenshotOverwriteDialogVisible = false
    shell.screenshotOverwriteTargetPath = ""
}

function resetSaveDraft(shell) {
    if (!shell) {
        return
    }
    shell.screenshotSaveSourcePath = ""
    shell.screenshotSaveName = ""
    shell.screenshotSaveFolder = ""
    shell.screenshotSaveTypeIndex = 0
    shell.screenshotSaveErrorCode = ""
    shell.screenshotSaveErrorHint = ""
    shell.screenshotSaveBlockedByNoSpace = false
}

function formatBytes(bytesValue) {
    var n = Number(bytesValue || 0)
    if (!(n > 0)) {
        return "0 B"
    }
    var units = ["B", "KB", "MB", "GB", "TB", "PB"]
    var idx = 0
    while (n >= 1024 && idx < units.length - 1) {
        n /= 1024
        idx += 1
    }
    var decimals = n >= 100 || idx === 0 ? 0 : (n >= 10 ? 1 : 2)
    return n.toFixed(decimals) + " " + units[idx]
}

function storageHintForPath(shell, pathValue) {
    if (!shell) {
        return ""
    }
    if (typeof ScreenshotSaveHelper === "undefined"
            || !ScreenshotSaveHelper
            || !ScreenshotSaveHelper.storageInfoForPath) {
        return ""
    }
    var info = ScreenshotSaveHelper.storageInfoForPath(String(pathValue || ""))
    if (!info || !info.ok) {
        return ""
    }
    var avail = Number(info.bytesAvailable !== undefined ? info.bytesAvailable : -1)
    var total = Number(info.bytesTotal !== undefined ? info.bytesTotal : -1)
    if (!(avail >= 0) || !(total > 0)) {
        return ""
    }
    var display = String(info.displayName || info.rootPath || "")
    if (display.length <= 0) {
        display = "destination"
    }
    return formatBytes(avail) + " free of " + formatBytes(total) + " on " + display
}

function applySaveErrorState(shell, rawErrorCode, targetPathValue) {
    if (!shell) {
        return ""
    }
    var code = ScreenshotFlow.normalizeErrorCode(rawErrorCode)
    shell.screenshotSaveErrorCode = code
    if (code === "no-space") {
        shell.screenshotSaveBlockedByNoSpace = true
        var hint = storageHintForPath(shell, ScreenshotFlow.dirName(String(targetPathValue || saveFullPath(shell))))
        shell.screenshotSaveErrorHint = hint.length > 0 ? hint : ScreenshotFlow.errorMessage(code, String(rawErrorCode || ""))
    } else if (code === "permission-denied") {
        shell.screenshotSaveBlockedByNoSpace = false
        shell.screenshotSaveErrorHint = "Folder is not writable. Choose another location."
    } else if (code === "target-busy") {
        shell.screenshotSaveBlockedByNoSpace = false
        shell.screenshotSaveErrorHint = "Destination is busy. Choose another name or folder."
    } else if (code === "mkdir-failed") {
        shell.screenshotSaveBlockedByNoSpace = false
        shell.screenshotSaveErrorHint = "Cannot create folder in selected location."
    } else {
        shell.screenshotSaveBlockedByNoSpace = false
        shell.screenshotSaveErrorHint = ScreenshotFlow.errorMessage(code, String(rawErrorCode || ""))
    }
    return code
}

function finalizeSave(shell, forceOverwrite) {
    if (!shell) {
        return
    }
    var sourcePath = String(shell.screenshotSaveSourcePath || "")
    if (sourcePath.length <= 0) {
        shell.screenshotSaveDialogVisible = false
        return
    }
    var finalPath = saveFullPath(shell)
    log(shell, "save-finalize-begin", {
            "forceOverwrite": !!forceOverwrite,
            "sourcePath": sourcePath,
            "targetPath": finalPath
        })
    if (!forceOverwrite) {
        var st = (typeof FileManagerApi !== "undefined" && FileManagerApi && FileManagerApi.statPath)
                ? FileManagerApi.statPath(finalPath) : ({ "ok": false })
        if (st && st.ok) {
            shell.screenshotOverwriteTargetPath = finalPath
            shell.screenshotOverwriteDialogVisible = true
            log(shell, "save-overwrite-prompt", { "targetPath": finalPath })
            return
        }
    } else {
        if (typeof FileManagerApi !== "undefined" && FileManagerApi && FileManagerApi.removePath) {
            FileManagerApi.removePath(finalPath, false)
        }
    }

    var typeInfo = outputTypeAt(shell, shell.screenshotSaveTypeIndex)
    var targetFormat = String(typeInfo && typeInfo.format ? typeInfo.format : "png")
    var targetQuality = Number(typeInfo && typeInfo.quality !== undefined ? typeInfo.quality : -1)
    var ok = false
    var err = "save-failed"
    shell.screenshotSaveErrorCode = ""
    shell.screenshotSaveErrorHint = ""
    if (typeof ScreenshotSaveHelper !== "undefined"
            && ScreenshotSaveHelper
            && ScreenshotSaveHelper.saveImageFile) {
        var saveRes = ScreenshotSaveHelper.saveImageFile(sourcePath,
                                                         finalPath,
                                                         targetFormat,
                                                         targetQuality,
                                                         !!forceOverwrite)
        ok = !!(saveRes && saveRes.ok)
        if (!ok) {
            err = String((saveRes && saveRes.error) ? saveRes.error : "save-failed")
        }
    } else {
        err = "io-error"
    }
    if (ok) {
        dismissSaveDialogs(shell)
        resetSaveDraft(shell)
        persistSaveFolder(ScreenshotFlow.dirName(finalPath))
        showResultNotification(shell, true, finalPath, "")
        shell.screenshotRequestId = ""
    } else {
        var code = applySaveErrorState(shell, err, finalPath)
        if (shouldKeepSaveDialogOpen(code)) {
            shell.screenshotSaveDialogVisible = true
            shell.screenshotOverwriteDialogVisible = false
            shell.screenshotOverwriteTargetPath = ""
            showResultNotification(shell, false, "", err)
            if (code === "permission-denied") {
                Qt.callLater(function() {
                    if (shell.screenshotSaveDialogVisible) {
                        chooseSaveFolder(shell)
                    }
                })
            }
            return
        }
        dismissSaveDialogs(shell)
        resetSaveDraft(shell)
        showResultNotification(shell, false, "", err)
        shell.screenshotRequestId = ""
    }
}

function submitSaveDialog(shell) {
    if (!shell) {
        return
    }
    var err = saveNameError(shell.screenshotSaveName)
    if (err.length > 0) {
        showResultNotification(shell, false, "", "invalid-filename")
        return
    }
    finalizeSave(shell, false)
}

function confirmOverwrite(shell) {
    finalizeSave(shell, true)
}

function cancelOverwrite(shell) {
    if (!shell) {
        return
    }
    var target = String(shell.screenshotOverwriteTargetPath || "")
    shell.screenshotOverwriteDialogVisible = false
    shell.screenshotOverwriteTargetPath = ""
    shell.screenshotLastErrorCode = "overwrite-canceled"
    log(shell, "save-overwrite-cancel", { "targetPath": target })
}

function chooseSaveFolder(shell) {
    if (!shell || !shell.screenshotSaveDialogVisible) {
        return
    }
    shell.screenshotSaveDialogVisible = false
    shell.portalChooserInternalKind = "screenshot_pick_folder"
    shell.portalChooserInternalPayload = ({
            "requestId": String(shell.screenshotRequestId || "")
        })
    log(shell, "folder-chooser-open", {
            "folder": String(shell.screenshotSaveFolder || defaultSaveFolder(shell))
        })
    if (shell.portalChooserApiRef && shell.portalChooserApiRef.openPortalFileChooser) {
        shell.portalChooserApiRef.openPortalFileChooser("__internal_screenshot_pick_folder_" + String(Date.now()), {
                                                            "mode": "folder",
                                                            "selectFolders": true,
                                                            "title": "Choose Folder",
                                                            "folder": String(shell.screenshotSaveFolder || defaultSaveFolder(shell)),
                                                            "multiple": false
                                                        })
    }
}
