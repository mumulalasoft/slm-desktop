.pragma library
.import "FileManagerUtils.js" as FileManagerUtils

function copyOrMoveSelected(root, cutMode) {
    var entries = root.selectedEntryInfos ? root.selectedEntryInfos() : []
    if (!entries || entries.length === 0) {
        var item = root.selectedEntryInfo ? root.selectedEntryInfo() : {}
        if (item && item.ok) {
            entries = [item]
        }
    }
    if (!entries || entries.length === 0) {
        return
    }
    var first = entries[0]
    root.clipboardPath = String(first.path || "")
    root.clipboardPaths = []
    for (var i = 0; i < entries.length; ++i) {
        var p = String(entries[i].path || "")
        if (p.length > 0 && root.clipboardPaths.indexOf(p) < 0) {
            root.clipboardPaths.push(p)
        }
    }
    root.clipboardName = FileManagerUtils.basename(root.clipboardPath)
    root.clipboardCut = !!cutMode
}

function moveSelectedToTrash(root, fileManagerApi, fileModel) {
    if (!fileManagerApi || !fileManagerApi.startTrashPaths) {
        return
    }
    var entries = root.selectedEntryInfos ? root.selectedEntryInfos() : []
    if (!entries || entries.length === 0) {
        var item = root.selectedEntryInfo ? root.selectedEntryInfo() : {}
        if (item && item.ok) {
            entries = [item]
        }
    }
    if (!entries || entries.length === 0) {
        return
    }
    var paths = []
    for (var i = 0; i < entries.length; ++i) {
        var p = String(entries[i].path || "")
        if (p.length > 0 && paths.indexOf(p) < 0) {
            paths.push(p)
        }
    }
    if (paths.length === 0) {
        return
    }
    var res = fileManagerApi.startTrashPaths(paths)
    if (root.notifyResult) {
        root.notifyResult("Move to Trash", res)
    }
}

function applyRename(root, fileModel) {
    if (!fileModel || !fileModel.renameAt || root.renameTargetIndex < 0) {
        root.renameSheetVisible = false
        return
    }
    var res = fileModel.renameAt(root.renameTargetIndex, root.renameTargetName)
    if (root.notifyResult) {
        root.notifyResult("Rename", res)
    }
    if (res && res.ok && root.refreshLibraryData) {
        root.refreshLibraryData()
    }
    root.renameSheetVisible = false
}

function requestRenameContextEntry(root, renameDialog) {
    if (!root.fileModel || root.contextEntryIndex < 0) {
        return
    }
    if (root.contextEntryProtected) {
        root.notifyResult("Rename", {
                              "ok": false,
                              "error": "protected-path"
                          })
        return
    }
    var originalName = String(root.contextEntryName || "")
    root.renameDialogError = ""
    root.renameInputText = originalName
    if (renameDialog && renameDialog.openAndFocus) {
        renameDialog.openAndFocus(originalName, !!root.contextEntryIsDir)
    } else if (renameDialog && renameDialog.open) {
        renameDialog.open()
    }
}

function applyRenameContextEntry(root, renameDialog) {
    if (!root.fileModel || !root.fileModel.renameAt
            || root.contextEntryIndex < 0) {
        if (renameDialog && renameDialog.close) {
            renameDialog.close()
        }
        return
    }
    var nextName = String(root.renameInputText || "").trim()
    if (nextName.length === 0) {
        root.renameDialogError = "Name cannot be empty."
        return
    }
    if (nextName === "." || nextName === ".." || nextName.indexOf("/") >= 0) {
        root.renameDialogError = "Invalid file name."
        return
    }
    var currentName = String(root.contextEntryName || "").trim()
    if (nextName === currentName) {
        if (renameDialog && renameDialog.close) {
            renameDialog.close()
        }
        return
    }
    var res = root.fileModel.renameAt(root.contextEntryIndex, nextName)
    if (!res || !res.ok) {
        root.renameDialogError = String(
                    (res && res.error) ? res.error : "rename-failed")
        root.notifyResult("Rename", res)
        return
    }
    root.renameDialogError = ""
    if (root.fileModel.refresh) {
        root.fileModel.refresh()
    }
    if (renameDialog && renameDialog.close) {
        renameDialog.close()
    }
}

function openContextEntryInFileManager(root, fileManagerApi) {
    var p = String(root.contextEntryPath || "")
    if (p.length === 0) {
        return
    }
    if (!fileManagerApi || !fileManagerApi.startOpenPathInFileManager) {
        return
    }
    var reqId = root.createAsyncRequestId()
    var res = fileManagerApi.startOpenPathInFileManager(p, reqId)
    if (!res || !res.ok) {
        root.notifyResult("Open In", res)
        return
    }
    root.trackOpenAction(reqId, "Open In", false)
}

function openContextEntryInCode(root, fileManagerApi) {
    if (root.contextEntryIndex < 0) {
        return
    }
    var p = String(root.contextEntryPath || "")
    if (p.length <= 0) {
        return
    }
    if (!fileManagerApi || !fileManagerApi.startOpenPathWithApp) {
        return
    }
    var appIds = ["code.desktop", "codium.desktop", "com.visualstudio.code.desktop"]
    for (var i = 0; i < appIds.length; ++i) {
        var reqId = root.createAsyncRequestId()
        var res = fileManagerApi.startOpenPathWithApp(p, appIds[i], reqId)
        if (!!res && !!res.ok) {
            root.trackOpenAction(reqId, "Open in Code", false)
            return
        }
    }
    root.notifyResult("Open in Code", {
                          "ok": false,
                          "error": "code-application-not-found"
                      })
}

function closeAllContextMenus(fileManagerMenus, sidebarContextMenu) {
    if (fileManagerMenus && fileManagerMenus.closeAllMenus) {
        fileManagerMenus.closeAllMenus()
    }
    if (sidebarContextMenu && sidebarContextMenu.close) {
        sidebarContextMenu.close()
    }
}

function popupContextEntryMenu(root, fileManagerMenus, sidebarContextMenu, x, y) {
    closeAllContextMenus(fileManagerMenus, sidebarContextMenu)
    if (root.trashView) {
        if (root.refreshContextSlmActions) {
            root.refreshContextSlmActions("file")
        }
        if (root.refreshContextSlmShareActions) {
            root.refreshContextSlmShareActions("file")
        }
        if (root.debugDumpSlmContextMenuTree) {
            root.debugDumpSlmContextMenuTree("file")
        }
        fileManagerMenus.openTrashMenu(x, y)
        return
    }
    if (root.selectedEntryIndexes && root.selectedEntryIndexes.length > 1) {
        if (root.refreshContextSlmActions) {
            root.refreshContextSlmActions("selection")
        }
        if (root.refreshContextSlmShareActions) {
            root.refreshContextSlmShareActions("selection")
        }
        if (root.debugDumpSlmContextMenuTree) {
            root.debugDumpSlmContextMenuTree("selection")
        }
        fileManagerMenus.openMultiSelectionMenu(x, y)
        return
    }
    if (root.contextEntryIsDir) {
        if (root.refreshContextSlmActions) {
            root.refreshContextSlmActions("directory")
        }
        if (root.refreshContextSlmShareActions) {
            root.refreshContextSlmShareActions("directory")
        }
        if (root.debugDumpSlmContextMenuTree) {
            root.debugDumpSlmContextMenuTree("directory")
        }
        fileManagerMenus.openFolderMenu(x, y)
        return
    }
    if (root.refreshContextSlmActions) {
        root.refreshContextSlmActions("file")
    }
    if (root.refreshContextSlmShareActions) {
        root.refreshContextSlmShareActions("file")
    }
    if (root.debugDumpSlmContextMenuTree) {
        root.debugDumpSlmContextMenuTree("file")
    }
    fileManagerMenus.openFileMenu(x, y)
}

function selectedEntry(root) {
    if (!root.fileModel || !root.fileModel.entryAt) {
        return ({
                    "ok": false
                })
    }
    var idx = Number(root.selectedEntryIndex)
    if (idx < 0 && root.selectedEntryIndexes
            && root.selectedEntryIndexes.length > 0) {
        idx = Number(root.selectedEntryIndexes[root.selectedEntryIndexes.length - 1])
    }
    if (idx < 0) {
        return ({
                    "ok": false
                })
    }
    return root.fileModel.entryAt(idx)
}

function selectedPaths(root) {
    var out = []
    if (root.fileModel && root.fileModel.entryAt
            && root.selectedEntryIndexes
            && root.selectedEntryIndexes.length > 0) {
        for (var i = 0; i < root.selectedEntryIndexes.length; ++i) {
            var idx = Number(root.selectedEntryIndexes[i])
            if (!(idx >= 0)) {
                continue
            }
            var row = root.fileModel.entryAt(idx)
            if (!row || !row.ok) {
                continue
            }
            var rowPath = String(row.path || "")
            if (rowPath.length > 0 && out.indexOf(rowPath) < 0) {
                out.push(rowPath)
            }
        }
        return out
    }
    var entry = selectedEntry(root)
    if (!entry || !entry.ok) {
        return out
    }
    var p = String(entry.path || "")
    if (p.length > 0) {
        out.push(p)
    }
    return out
}

function selectedRows(root) {
    var out = []
    if (!root.fileModel || !root.fileModel.entryAt) {
        return out
    }
    var indexes = root.selectedEntryIndexes || []
    for (var i = 0; i < indexes.length; ++i) {
        var idx = Number(indexes[i])
        if (!(idx >= 0)) {
            continue
        }
        var row = root.fileModel.entryAt(idx)
        if (!row || !row.ok) {
            continue
        }
        out.push(row)
    }
    return out
}

function selectedHasProtectedPath(root) {
    var paths = selectedPaths(root)
    for (var i = 0; i < paths.length; ++i) {
        if (root.isProtectedEntryPath(String(paths[i] || ""))) {
            return true
        }
    }
    return false
}

function selectedAllDirectories(root) {
    var rows = selectedRows(root)
    if (!rows || rows.length <= 0) {
        return false
    }
    for (var i = 0; i < rows.length; ++i) {
        if (!rows[i] || !rows[i].ok || !rows[i].isDir) {
            return false
        }
    }
    return true
}

function openSelectedEntries(root) {
    var rows = selectedRows(root)
    if (!rows || rows.length <= 0) {
        return
    }
    if (rows.length === 1 && root.contextEntryIndex >= 0) {
        root.openContextEntry()
        return
    }
    for (var i = 0; i < rows.length; ++i) {
        var row = rows[i]
        if (!row || !row.ok) {
            continue
        }
        var p = String(row.path || "")
        if (p.length <= 0) {
            continue
        }
        if (!!row.isDir) {
            root.openInNewWindowRequested(p)
        } else {
            var launchRes = root.openTargetViaExecutionGate(
                        p, "filemanager-multi-open")
            if (!launchRes || !launchRes.ok) {
                root.notifyResult("Open", launchRes)
                break
            }
        }
    }
}

function openSelectedInNewTabs(root) {
    if (!selectedAllDirectories(root)) {
        return
    }
    var rows = selectedRows(root)
    for (var i = 0; i < rows.length; ++i) {
        var p = String((rows[i] && rows[i].path) ? rows[i].path : "")
        if (p.length > 0) {
            root.addTab(p, true)
        }
    }
}

function openSelectedInNewWindows(root) {
    if (!selectedAllDirectories(root)) {
        return
    }
    var rows = selectedRows(root)
    for (var i = 0; i < rows.length; ++i) {
        var p = String((rows[i] && rows[i].path) ? rows[i].path : "")
        if (p.length > 0) {
            root.openInNewWindowRequested(p)
        }
    }
}

function allVisiblePaths(root) {
    if (!root.fileModel || root.fileModel.count === undefined
            || !root.fileModel.entryAt) {
        return []
    }
    var out = []
    var n = Number(root.fileModel.count || 0)
    for (var i = 0; i < n; ++i) {
        var row = root.fileModel.entryAt(i)
        if (!row || !row.ok) {
            continue
        }
        var p = String(row.path || "")
        if (p.length > 0 && out.indexOf(p) < 0) {
            out.push(p)
        }
    }
    return out
}

function copySelected(root, cutMode) {
    var paths = selectedPaths(root)
    if (!paths || paths.length <= 0) {
        return
    }
    root.clipboardPaths = paths.slice(0)
    root.clipboardPath = String(paths[0] || "")
    root.clipboardCut = !!cutMode
}

function pasteIntoCurrent(root, fileManagerApi) {
    var srcList = (root.clipboardPaths
                   && root.clipboardPaths.length > 0) ? root.clipboardPaths.slice(
                                                            0) : []
    if (srcList.length <= 0) {
        var single = String(root.clipboardPath || "")
        if (single.length > 0) {
            srcList = [single]
        }
    }
    if (srcList.length <= 0 || !root.fileModel) {
        return
    }
    var dir = String(root.fileModel.currentPath || "")
    if (dir.length === 0) {
        dir = "~"
    }
    if (!fileManagerApi) {
        return
    }
    var res
    if (root.clipboardCut) {
        if (!fileManagerApi.startMovePaths) {
            res = {
                "ok": false,
                "error": "async-move-api-unavailable"
            }
        } else {
            res = fileManagerApi.startMovePaths(srcList, dir, false)
        }
    } else {
        if (!fileManagerApi.startCopyPaths) {
            res = {
                "ok": false,
                "error": "async-copy-api-unavailable"
            }
        } else {
            res = fileManagerApi.startCopyPaths(srcList, dir, false)
        }
    }
    if (!res || !res.ok) {
        root.notifyResult(root.clipboardCut ? "Move" : "Copy", res)
    }
    if (res && res.ok && root.clipboardCut) {
        root.clipboardPath = ""
        root.clipboardPaths = []
        root.clipboardCut = false
    }
}

function deleteSelected(root, fileManagerApi, permanentDelete) {
    var paths = selectedPaths(root)
    if (!paths || paths.length <= 0) {
        return
    }
    if (!fileManagerApi) {
        return
    }
    var res = permanentDelete ? (fileManagerApi.startRemovePaths ? fileManagerApi.startRemovePaths(
                                                                       paths,
                                                                       true) : {
                                                                       "ok": false,
                                                                       "error": "async-delete-api-unavailable"
                                                                   }) : (fileManagerApi.startTrashPaths ? fileManagerApi.startTrashPaths(paths) : {
                                                                                                              "ok": false,
                                                                                                              "error": "async-trash-api-unavailable"
                                                                                                          })
    root.notifyResult(permanentDelete ? "Delete Permanently" : "Move to Trash",
                      res)
}

function restoreSelectedFromTrash(root, fileManagerApi) {
    var paths = selectedPaths(root)
    if (!paths || paths.length <= 0) {
        return
    }
    if (!fileManagerApi || !fileManagerApi.startRestoreTrashPaths) {
        return
    }
    var res = fileManagerApi.startRestoreTrashPaths(paths, "~")
    root.notifyResult("Restore", res)
}

function restoreAllFromTrash(root, fileManagerApi) {
    var paths = allVisiblePaths(root)
    if (!paths || paths.length <= 0) {
        return
    }
    if (!fileManagerApi || !fileManagerApi.startRestoreTrashPaths) {
        return
    }
    var res = fileManagerApi.startRestoreTrashPaths(paths, "~")
    root.notifyResult("Restore", res)
}

function deleteSelectedOrEmptyTrash(root, fileManagerApi) {
    if (!root.trashView) {
        return
    }
    var selected = selectedPaths(root)
    if (selected.length > 0) {
        deleteSelected(root, fileManagerApi, true)
        return
    }
    var all = allVisiblePaths(root)
    if (!all || all.length <= 0) {
        return
    }
    if (!fileManagerApi) {
        return
    }
    var res
    if (fileManagerApi.startEmptyTrash) {
        res = fileManagerApi.startEmptyTrash(root.trashFilesPath)
    } else if (fileManagerApi.startRemovePaths) {
        res = fileManagerApi.startRemovePaths(all, true)
    } else {
        res = {
            "ok": false,
            "error": "async-delete-api-unavailable"
        }
    }
    root.notifyResult("Delete", res)
}

function copySelectedPathToClipboard(root, fileManagerApi) {
    var paths = selectedPaths(root)
    if (!paths || paths.length <= 0) {
        var cp = String(root.contextEntryPath || "")
        if (cp.length > 0) {
            paths = [cp]
        } else {
            return
        }
    }
    if (fileManagerApi && fileManagerApi.copyTextToClipboard) {
        fileManagerApi.copyTextToClipboard(String(paths.join("\n")))
    }
}

function copySelectedAsLinkToClipboard(root, fileManagerApi) {
    var paths = selectedPaths(root)
    if (!paths || paths.length <= 0) {
        return
    }
    if (!fileManagerApi || !fileManagerApi.copyTextToClipboard) {
        return
    }
    var links = []
    for (var i = 0; i < paths.length; ++i) {
        var p = String(paths[i] || "")
        if (p.length <= 0) {
            continue
        }
        links.push("file://" + encodeURIComponent(p).replace(/%2F/g, "/"))
    }
    if (links.length > 0) {
        fileManagerApi.copyTextToClipboard(String(links.join("\n")))
    }
}

function invertSelection(root) {
    if (!root.fileModel || root.fileModel.count === undefined) {
        return
    }
    var n = Number(root.fileModel.count || 0)
    if (n <= 0) {
        root.clearSelection()
        return
    }
    var selected = root.selectedEntryIndexes || []
    var selectedSet = ({})
    for (var i = 0; i < selected.length; ++i) {
        selectedSet[String(Number(selected[i]))] = true
    }
    var next = []
    for (var j = 0; j < n; ++j) {
        if (!selectedSet[String(j)]) {
            next.push(j)
        }
    }
    root.normalizeSelection(next)
}

function refreshCurrent(root) {
    if (root.fileModel && root.fileModel.refresh) {
        root.contentLoading = true
        root.fileModel.refresh()
    }
}

function createNewFolder(root) {
    if (!root.fileModel || !root.fileModel.createDirectory) {
        return
    }
    root.fileModel.createDirectory("New Folder")
    refreshCurrent(root)
}

function createNewFile(root) {
    if (!root.fileModel || !root.fileModel.createFile) {
        return
    }
    root.fileModel.createFile("New File.txt")
    refreshCurrent(root)
}

function clearRecentFiles(root, fileManagerApi) {
    if (!fileManagerApi || !fileManagerApi.clearRecentFiles) {
        return
    }
    var res = fileManagerApi.clearRecentFiles()
    if (!res || !res.ok) {
        root.notifyResult("Clear Recents", res)
    }
    refreshCurrent(root)
}

function requestClearRecentFiles(clearRecentsDialog) {
    if (clearRecentsDialog && clearRecentsDialog.open) {
        clearRecentsDialog.open()
    }
}

function addContextEntryToBookmarks(root, fileManagerApi) {
    if (root.contextEntryIndex < 0) {
        return
    }
    if (!fileManagerApi || !fileManagerApi.addBookmark) {
        return
    }
    var p = String(root.contextEntryPath || "")
    if (p.length <= 0) {
        return
    }
    var target = root.contextEntryIsDir ? p : root.parentPathOf(p)
    var label = root.contextEntryIsDir ? String(root.contextEntryName || "") : ""
    var res = fileManagerApi.addBookmark(target, label)
    if (!res || !res.ok) {
        root.notifyResult("Add to Bookmarks", res)
        return
    }
    root.rebuildSidebarItems()
}

function sendSelectionByEmail(root, appCommandRouter) {
    var paths = selectedPaths(root)
    if (!paths || paths.length <= 0) {
        return
    }
    if (!appCommandRouter || !appCommandRouter.routeWithResult) {
        root.notifyResult("Send by Email", {
                              "ok": false,
                              "error": "command-router-unavailable"
                          })
        return
    }
    var cmd = "xdg-email"
    for (var i = 0; i < paths.length; ++i) {
        var p = String(paths[i] || "")
        if (p.length <= 0) {
            continue
        }
        cmd += " --attach " + root.shellSingleQuote(p)
    }
    var res = appCommandRouter.routeWithResult("terminal.exec", {
                                                    "command": cmd
                                                },
                                                "filemanager-context-email")
    root.notifyResult("Send by Email", res)
}

function primaryPrintableEntry(root) {
    var rows = selectedRows(root)
    if (rows && rows.length === 1) {
        var only = rows[0] || ({})
        if (!!only.ok && !only.isDir) {
            return only
        }
    }
    var entry = selectedEntry(root)
    if (!!entry && !!entry.ok && !entry.isDir) {
        return entry
    }
    var contextPath = String(root.contextEntryPath || "")
    if (contextPath.length > 0 && !root.contextEntryIsDir) {
        return {
            "ok": true,
            "name": String(root.contextEntryName || root.basename(contextPath)),
            "path": contextPath,
            "isDir": false
        }
    }
    return ({
                "ok": false
            })
}

function isPrintableMime(mimeType) {
    var mime = String(mimeType || "").trim().toLowerCase()
    if (mime.length <= 0) {
        return false
    }
    if (mime.indexOf("image/") === 0) {
        return true
    }
    if (mime.indexOf("text/") === 0) {
        return true
    }
    if (mime === "application/pdf"
            || mime === "application/postscript"
            || mime === "application/rtf"
            || mime === "application/msword"
            || mime === "application/vnd.oasis.opendocument.text"
            || mime === "application/vnd.openxmlformats-officedocument.wordprocessingml.document"
            || mime === "application/vnd.ms-excel"
            || mime === "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"
            || mime === "application/vnd.ms-powerpoint"
            || mime === "application/vnd.openxmlformats-officedocument.presentationml.presentation") {
        return true
    }
    return false
}

function isPrintableSuffix(suffixValue) {
    var ext = String(suffixValue || "").trim().toLowerCase()
    if (ext.length <= 0) {
        return false
    }
    var allowed = {
        "pdf": true,
        "ps": true,
        "txt": true,
        "md": true,
        "rtf": true,
        "odt": true,
        "doc": true,
        "docx": true,
        "xls": true,
        "xlsx": true,
        "ppt": true,
        "pptx": true,
        "png": true,
        "jpg": true,
        "jpeg": true,
        "webp": true,
        "bmp": true,
        "gif": true,
        "tif": true,
        "tiff": true,
        "svg": true
    }
    return allowed[ext] === true
}

function localFileUri(pathValue) {
    var p = String(pathValue || "").trim()
    if (p.length <= 0) {
        return ""
    }
    if (p.indexOf("file://") === 0) {
        return p
    }
    return "file://" + p
}

function canPrintSelection(root) {
    var entry = primaryPrintableEntry(root)
    if (!entry || !entry.ok || !!entry.isDir) {
        return false
    }
    var pathValue = String((entry && entry.path) ? entry.path : "").trim()
    if (pathValue.length <= 0) {
        return false
    }
    var mime = String((entry && entry.mimeType) ? entry.mimeType : "")
    var suffix = String((entry && entry.suffix) ? entry.suffix : "")
    return true
}

function prefersPdfFallbackForEntry(entry) {
    if (!entry || !entry.ok || !!entry.isDir) {
        return false
    }
    var mime = String((entry && entry.mimeType) ? entry.mimeType : "")
    var suffix = String((entry && entry.suffix) ? entry.suffix : "")
    return !(isPrintableMime(mime) || isPrintableSuffix(suffix))
}

function requestPrintSelection(root) {
    var entry = primaryPrintableEntry(root)
    if (!entry || !entry.ok || !!entry.isDir) {
        root.notifyResult("Print", {
                              "ok": false,
                              "error": "print-selection-invalid"
                          })
        return
    }
    var pathValue = String(entry.path || "").trim()
    if (pathValue.length <= 0) {
        root.notifyResult("Print", {
                              "ok": false,
                              "error": "print-path-empty"
                          })
        return
    }
    var uri = localFileUri(pathValue)
    var title = String(entry.name || root.basename(pathValue) || "Document")
    var preferPdfOutput = prefersPdfFallbackForEntry(entry)
    if (preferPdfOutput) {
        root.notifyResult("Print", {
                              "ok": true,
                              "error": "",
                              "hint": "direct-print-unsupported-using-pdf-fallback"
                          })
    }
    if (root.printRequested) {
        root.printRequested(uri, title, preferPdfOutput)
    }
}

function sendSelectionViaBluetooth(root, appCommandRouter) {
    var paths = selectedPaths(root)
    if (!paths || paths.length <= 0) {
        return
    }
    if (!appCommandRouter || !appCommandRouter.routeWithResult) {
        root.notifyResult("Send Files via Bluetooth", {
                              "ok": false,
                              "error": "command-router-unavailable"
                          })
        return
    }
    var cmd = "if command -v bluetooth-sendto >/dev/null 2>&1; then bluetooth-sendto"
    for (var i = 0; i < paths.length; ++i) {
        var p = String(paths[i] || "")
        if (p.length <= 0) {
            continue
        }
        cmd += " " + root.shellSingleQuote(p)
    }
    cmd += "; elif command -v blueman-sendto >/dev/null 2>&1; then blueman-sendto"
    for (var j = 0; j < paths.length; ++j) {
        var p2 = String(paths[j] || "")
        if (p2.length <= 0) {
            continue
        }
        cmd += " " + root.shellSingleQuote(p2)
    }
    cmd += "; else echo 'bluetooth-sendto-not-found' >&2; exit 127; fi"

    var res = appCommandRouter.routeWithResult("terminal.exec", {
                                                    "command": cmd
                                                },
                                                "filemanager-context-bluetooth")
    root.notifyResult("Send Files via Bluetooth", res)
}

function chooseDestinationForSelection(root, fileManagerApi, moveMode) {
    var sources = selectedPaths(root)
    if (!sources || sources.length <= 0) {
        return
    }
    if (!fileManagerApi || !fileManagerApi.startPortalFileChooser) {
        root.notifyResult(moveMode ? "Move" : "Copy", {
                              "ok": false,
                              "error": "portal-filechooser-unavailable"
                          })
        return
    }
    var rid = fileManagerApi.startPortalFileChooser({
                                                         "mode": "folder",
                                                         "title": moveMode ? "Move to..." : "Copy to...",
                                                         "selectFolders": true,
                                                         "multiple": false,
                                                         "folder": String(
                                                                       root.fileModel
                                                                       && root.fileModel.currentPath ? root.fileModel.currentPath : "~")
                                                     })
    root.pendingPortalChooserRequestId = String(rid || "")
    root.pendingPortalChooserAction = moveMode ? "move" : "copy"
    root.pendingPortalChooserSources = sources
}

function compressSelection(root, fileManagerApi, compressDialog) {
    var paths = selectedPaths(root)
    if (!paths || paths.length <= 0) {
        return
    }
    if (!fileManagerApi || !fileManagerApi.startCompressArchive) {
        root.notifyResult("Compress", {
                              "ok": false,
                              "error": "compress-api-unavailable"
                          })
        return
    }

    var baseDir = "~"
    if (root.fileModel && root.fileModel.currentPath !== undefined) {
        var cwd = String(root.fileModel.currentPath || "")
        if (cwd.length > 0) {
            baseDir = cwd
        }
    }

    var archiveBase = "Archive"
    if (paths.length === 1) {
        var singleName = String(root.contextEntryName || "")
        if (singleName.length <= 0) {
            singleName = root.basename(String(paths[0] || ""))
        }
        if (singleName.length > 0) {
            archiveBase = singleName
        }
    }
    archiveBase = archiveBase.replace(/[\\/:*?"<>|]+/g, "_")
    if (archiveBase.length <= 0) {
        archiveBase = "Archive"
    }

    root.compressPendingPaths = paths.slice(0)
    root.compressOutputDirectory = baseDir
    root.compressFormat = "tar"
    if (compressDialog && compressDialog.openAndFocus) {
        compressDialog.openAndFocus(archiveBase + ".tar")
    } else if (compressDialog && compressDialog.open) {
        compressDialog.open()
    }
}

function applyCompressSelection(root, fileManagerApi, compressDialog) {
    var paths = root.compressPendingPaths || []
    if (!paths || paths.length <= 0) {
        if (compressDialog && compressDialog.close) {
            compressDialog.close()
        }
        return
    }
    if (!fileManagerApi || !fileManagerApi.startCompressArchive) {
        root.notifyResult("Compress", {
                              "ok": false,
                              "error": "compress-api-unavailable"
                          })
        return
    }
    var name = String((compressDialog && compressDialog.archiveNameText)
                      ? compressDialog.archiveNameText : "").trim()
    name = name.replace(/[\\/:*?"<>|]+/g, "_")
    if (name.length <= 0) {
        return
    }
    var hasExt = (name.lastIndexOf(".") > 0)
    var fmt = String(root.compressFormat || "tar").toLowerCase()
    if (!hasExt) {
        name += (fmt === "zip") ? ".zip" : ".tar"
    }
    var archivePath = String(root.compressOutputDirectory || "~") + "/" + name
    if (compressDialog && compressDialog.close) {
        compressDialog.close()
    }
    var res = fileManagerApi.startCompressArchive(paths, archivePath, fmt)
    if (!res || !res.ok) {
        root.notifyResult("Compress", res)
    }
}

function showPropertiesForSelection(root, fileManagerApi, propertiesDialog) {
    var entry = selectedEntry(root)
    if (!entry || !entry.ok) {
        var cp = String(root.contextEntryPath || "")
        if (cp.length <= 0) {
            return
        }
        var contextIsDir = !!root.contextEntryIsDir
        entry = {
            "ok": true,
            "name": String(root.contextEntryName || root.basename(cp)),
            "path": cp,
            "isDir": contextIsDir,
            "iconName": contextIsDir ? "folder" : "text-x-generic",
            "suffix": ""
        }
    }
    root.propertiesEntry = entry
    root.propertiesTabIndex = 0
    root.propertiesOpenWithName = ""
    root.propertiesOpenWithApps = []
    root.propertiesOpenWithCurrentIndex = -1
    root.propertiesOpenWithRequestId = ""
    root.propertiesOpenWithPath = ""
    var appRow = root.contextDefaultOpenWithEntry()
    var appName = String((appRow && appRow.name) ? appRow.name : "")
    if (appName.length > 0) {
        root.propertiesOpenWithName = appName
    }
    if (fileManagerApi && fileManagerApi.statPath) {
        var statRes = fileManagerApi.statPath(String(entry.path || ""))
        root.propertiesStat = (statRes && statRes.ok) ? statRes : ({})
    } else {
        root.propertiesStat = ({})
    }
    if (!entry.isDir) {
        root.refreshPropertiesOpenWithApps(String(entry.path || ""))
    }
    if (propertiesDialog && propertiesDialog.open) {
        propertiesDialog.open()
    }
}

function showPropertiesForPath(root, fileManagerApi, pathValue, propertiesDialog) {
    var p = String(pathValue || "").trim()
    if (p.length <= 0) {
        return
    }
    var isDirValue = false
    if (fileManagerApi && fileManagerApi.statPath) {
        var st = fileManagerApi.statPath(p)
        if (st && st.ok) {
            isDirValue = !!st.isDir
        }
    }
    root.contextEntryPath = p
    root.contextEntryName = root.basename(p)
    root.contextEntryIsDir = isDirValue
    root.selectedEntryIndex = -1
    root.selectedEntryIndexes = []
    showPropertiesForSelection(root, fileManagerApi, propertiesDialog)
}

function isImageSuffix(nameValue) {
    var n = String(nameValue || "").toLowerCase()
    return n.endsWith(".png") || n.endsWith(".jpg") || n.endsWith(
                ".jpeg") || n.endsWith(".webp") || n.endsWith(
                ".gif") || n.endsWith(".bmp") || n.endsWith(".svg")
}

function previewSourceForEntry(entry) {
    if (!entry || !entry.ok || !!entry.isDir) {
        return ""
    }
    var thumb = String(entry.thumbnailPath || "")
    if (thumb.length > 0) {
        return thumb
    }
    var pathValue = String(entry.path || "")
    if (isImageSuffix(String(entry.name || pathValue))) {
        return pathValue
    }
    return ""
}

function toggleQuickPreview(root, quickPreviewDialog) {
    var entry = selectedEntry(root)
    if (!entry || !entry.ok) {
        return
    }
    root.quickPreviewTitleText = String(entry.name || "")
    root.quickPreviewMetaText = String(
                entry.isDir ? "Folder" : "File") + "  •  " + String(
                entry.path || "")
    root.quickPreviewImageSource = previewSourceForEntry(entry)
    root.quickPreviewFallbackIconSource = "image://themeicon/" + String(
                entry.iconName
                || (entry.isDir ? "folder" : "text-x-generic-symbolic"))
    if (quickPreviewDialog && quickPreviewDialog.visible) {
        quickPreviewDialog.close()
    } else if (quickPreviewDialog && quickPreviewDialog.open) {
        quickPreviewDialog.open()
    }
}
