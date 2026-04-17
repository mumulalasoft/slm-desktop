.pragma library
.import "PortalChooserFlow.js" as PortalChooserFlow
.import "PortalChooserNav.js" as PortalChooserNav
.import "../screenshot/ScreenshotController.js" as ScreenshotController
.import "../screenshot/ScreenshotFlow.js" as ScreenshotFlow
.import "../screenshot/ScreenshotSaveController.js" as ScreenshotSaveController

function keepScreenshotSaveDialogOpen(errorCode) {
    var code = String(errorCode || "")
    return code === "permission-denied"
            || code === "no-space"
            || code === "target-busy"
            || code === "mkdir-failed"
}

function entriesModel(shell) {
    return shell ? shell.portalChooserEntriesModelRef : null
}

function placesModel(shell) {
    return shell ? shell.portalChooserPlacesModelRef : null
}

function resetSelection(shell) {
    shell.portalChooserSelectedIndex = -1
    shell.portalChooserAnchorIndex = -1
    shell.portalChooserSelectedPaths = ({})
}

function clearSelectionAndPreview(shell) {
    resetSelection(shell)
    shell.portalChooserPreviewPath = ""
}

function selectionCount(shell) {
    return PortalChooserFlow.countSelected(shell.portalChooserSelectedPaths)
}

function validateSaveName(nameValue) {
    return PortalChooserFlow.validateSaveName(nameValue)
}

function goUp(shell) {
    var p = String(shell.portalChooserCurrentDir || "")
    var slash = p.lastIndexOf("/")
    var model = entriesModel(shell)
    if (!model) {
        return
    }
    if (slash > 0) {
        loadDirectory(shell, model, p.substring(0, slash))
        return
    }
    loadDirectory(shell, model, "~")
}

function primaryActionEnabled(shell) {
    var selectedCount = Number(selectionCount(shell))
    var saveValidation = validateSaveName(String(shell.portalChooserName || ""))
    if (typeof PortalChooserLogicHelper !== "undefined"
            && PortalChooserLogicHelper
            && PortalChooserLogicHelper.canPrimaryAction) {
        return !!PortalChooserLogicHelper.canPrimaryAction(
                    String(shell.portalChooserMode || ""),
                    !!shell.portalChooserSelectFolders,
                    String(shell.portalChooserName || ""),
                    String(shell.portalChooserCurrentDir || ""),
                    selectedCount)
    }
    if (shell.portalChooserMode === "save") {
        return saveValidation.ok
    }
    if (shell.portalChooserSelectFolders) {
        return String(shell.portalChooserCurrentDir || "").trim().length > 0
    }
    return selectedCount > 0
}

function invertSelection(shell, entriesModel) {
    if (!shell.portalChooserAllowMultiple) {
        return
    }
    var total = Number(entriesModel.count || 0)
    if (total <= 0) {
        clearSelectionAndPreview(shell)
        return
    }
    var map = ({})
    for (var i = 0; i < total; ++i) {
        var row = entriesModel.get(i)
        if (!row) {
            continue
        }
        var p = String(row.path || "")
        if (shell.portalChooserSelectedPaths[p] === true) {
            continue
        }
        map[p] = true
    }
    shell.portalChooserSelectedPaths = map
    if (Object.keys(map).length <= 0) {
        shell.portalChooserSelectedIndex = -1
    } else if (shell.portalChooserSelectedIndex < 0 || shell.portalChooserSelectedIndex >= total) {
        shell.portalChooserSelectedIndex = 0
    }
    updatePreviewPath(shell)
}

function triggerPrimaryAction(shell) {
    if (!primaryActionEnabled(shell)) {
        return
    }
    finishChooser(shell, true, false, "")
}

function openChooser(shell, requestId, options) {
    if (shell.portalFileChooserVisible
            && shell.portalChooserRequestId.length > 0
            && shell.portalChooserRequestId !== String(requestId || "")
            && typeof PortalUiBridge !== "undefined"
            && PortalUiBridge
            && PortalUiBridge.submitFileChooserResult) {
        PortalUiBridge.submitFileChooserResult(shell.portalChooserRequestId, {
                                                   "ok": false,
                                                   "canceled": true,
                                                   "error": "interrupted"
                                               })
    }
    var opts = options || {}
    shell.portalChooserRequestId = String(requestId || "")
    shell.portalChooserOptions = opts
    shell.portalChooserMode = String(opts.mode || "open").toLowerCase()
    shell.portalChooserAllowMultiple = !!opts.multiple
    shell.portalChooserSelectFolders = !!opts.selectFolders || shell.portalChooserMode === "folder"
    shell.portalChooserTitle = String(opts.title || (shell.portalChooserMode === "save" ? "Save As" : "Open"))
    shell.portalChooserName = String(opts.name || "")
    var prefFolder = loadUiPreferences(shell)
    if (String(opts.sortKey || "").trim().length > 0) {
        shell.portalChooserSortKey = String(opts.sortKey || "name")
    }
    if (opts.sortDescending !== undefined) {
        shell.portalChooserSortDescending = !!opts.sortDescending
    }
    if (opts.showHidden !== undefined) {
        shell.portalChooserShowHidden = !!opts.showHidden
    }
    shell.portalChooserFilters = []
    shell.portalChooserFilterIndex = 0
    shell.portalChooserValidationError = ""
    shell.portalChooserFilters = PortalChooserFlow.parseFilters(opts.filters || [])
    refreshStoragePlaces(shell, placesModel(shell))
    var startFolder = String(opts.folder || "").trim()
    if (startFolder.length <= 0) {
        startFolder = prefFolder
    }
    var model = entriesModel(shell)
    if (model) {
        loadDirectory(shell, model, startFolder)
    }
    rebuildPlacesModel(shell, placesModel(shell))
    shell.portalFileChooserVisible = true
    shell.portalChooserPathEditMode = false
    shell.portalChooserOverwriteDialogVisible = false
    shell.portalChooserOverwriteTargetPath = ""
}

function completeInternalChooser(shell, ok, canceled, selected, errorText) {
    var kind = String(shell.portalChooserInternalKind || "")
    var payload = shell.portalChooserInternalPayload || ({})
    var reqId = String(payload.requestId || shell.screenshotRequestId || "")
    if (reqId.length > 0) {
        shell.screenshotRequestId = reqId
    }
    shell.portalChooserInternalKind = ""
    shell.portalChooserInternalPayload = ({})
    if (kind === "screenshot_pick_folder") {
        ScreenshotSaveController.log(shell, "folder-chooser-finish", {
                                "ok": !!ok,
                                "canceled": !!canceled,
                                "selectedCount": selected ? selected.length : 0
                            })
        if (!canceled && ok && selected.length > 0) {
            shell.screenshotSaveFolder = String(selected[0]
                                                || shell.screenshotSaveFolder
                                                || ScreenshotSaveController.defaultSaveFolder(shell))
            ScreenshotSaveController.persistSaveFolder(shell.screenshotSaveFolder)
            shell.screenshotSaveBlockedByNoSpace = false
            shell.screenshotSaveErrorCode = ""
            shell.screenshotSaveErrorHint = ""
        }
        if (String(shell.screenshotSaveSourcePath || "").length > 0) {
            shell.screenshotSaveDialogVisible = true
        }
        return
    }

    if (kind !== "screenshot_save") {
        return
    }
    var sourcePath = String(payload.sourcePath || "")
    if (sourcePath.length <= 0) {
        return
    }
    if (canceled || !ok || selected.length <= 0) {
        if (typeof FileManagerApi !== "undefined" && FileManagerApi && FileManagerApi.removePath) {
            FileManagerApi.removePath(sourcePath, false)
        }
        return
    }
    var finalPath = String(selected[0] || "")
    if (finalPath.length <= 0) {
        ScreenshotController.showResultNotification(shell, false, "", "save-failed")
        return
    }
    var typeInfo = ScreenshotSaveController.outputTypeAt(shell, shell.screenshotSaveTypeIndex)
    var ext = String(typeInfo && typeInfo.ext ? typeInfo.ext : "png")
    var format = String(typeInfo && typeInfo.format ? typeInfo.format : "png")
    var quality = Number(typeInfo && typeInfo.quality !== undefined ? typeInfo.quality : -1)
    var dot = finalPath.lastIndexOf(".")
    if (dot > 0) {
        finalPath = finalPath.substring(0, dot)
    }
    finalPath += "." + ext
    if (typeof ScreenshotSaveHelper !== "undefined"
            && ScreenshotSaveHelper
            && ScreenshotSaveHelper.saveImageFile) {
        var saveRes = ScreenshotSaveHelper.saveImageFile(sourcePath,
                                                         finalPath,
                                                         format,
                                                         quality,
                                                         true)
        if (!!(saveRes && saveRes.ok)) {
            ScreenshotSaveController.persistSaveFolder(ScreenshotFlow.dirName(finalPath))
            ScreenshotController.showResultNotification(shell, true, finalPath, "")
        } else {
            var internalErr = String((saveRes && saveRes.error) ? saveRes.error : "io-error")
            var internalCode = ScreenshotSaveController.applySaveErrorState(shell, internalErr, finalPath)
            if (keepScreenshotSaveDialogOpen(internalCode)) {
                shell.screenshotSaveSourcePath = sourcePath
                if (String(shell.screenshotSaveName || "").trim().length <= 0) {
                    shell.screenshotSaveName = ScreenshotFlow.baseName(sourcePath)
                }
                if (String(shell.screenshotSaveFolder || "").trim().length <= 0) {
                    shell.screenshotSaveFolder = ScreenshotFlow.dirName(finalPath)
                }
                shell.screenshotSaveDialogVisible = true
                if (internalCode === "permission-denied") {
                    Qt.callLater(function() {
                        if (shell.screenshotSaveDialogVisible) {
                            ScreenshotSaveController.chooseSaveFolder(shell)
                        }
                    })
                }
            }
            ScreenshotController.showResultNotification(shell, false, "", internalErr)
        }
    } else {
        ScreenshotController.showResultNotification(shell, false, "", String(errorText || "io-error"))
    }
}

function finishChooser(shell, ok, canceled, errorText, skipOverwriteCheck) {
    shell.portalChooserValidationError = ""
    var selected = PortalChooserFlow.collectSelectedPaths(shell.portalChooserSelectedPaths)
    selected = PortalChooserFlow.withFolderFallback(
                selected, shell.portalChooserSelectFolders, shell.portalChooserCurrentDir)
    var saveResult = PortalChooserFlow.saveSelection(
                shell.portalChooserMode, shell.portalChooserName, shell.portalChooserCurrentDir, selected)
    if (!saveResult.ok) {
        shell.portalChooserValidationError = String(saveResult.error || "Invalid filename")
        return
    }
    selected = saveResult.selected || []

    if (ok && shell.portalChooserMode === "save"
            && !skipOverwriteCheck
            && !shell.portalChooserOverwriteAlwaysThisSession
            && selected.length > 0) {
        var target = String(selected[0] || "").trim()
        if (target.length > 0 && typeof FileManagerApi !== "undefined" && FileManagerApi && FileManagerApi.statPath) {
            var st = FileManagerApi.statPath(target)
            if (st && st.ok) {
                shell.portalChooserOverwriteTargetPath = target
                shell.portalChooserOverwriteDialogVisible = true
                return
            }
        }
    }

    var payload = PortalChooserFlow.buildResultPayload(
                ok, canceled, errorText, shell.portalChooserMode, shell.portalChooserCurrentDir, selected)
    var requestId = shell.portalChooserRequestId
    if (requestId.indexOf("__internal_") === 0) {
        completeInternalChooser(shell, !!ok, !!canceled, selected, String(errorText || ""))
    } else if (typeof PortalUiBridge !== "undefined" && PortalUiBridge && PortalUiBridge.submitFileChooserResult) {
        PortalUiBridge.submitFileChooserResult(requestId, payload)
    }
    saveUiPreferences(shell)
    shell.portalFileChooserVisible = false
    shell.portalChooserOverwriteDialogVisible = false
    shell.portalChooserOverwriteTargetPath = ""
    shell.portalChooserRequestId = ""
    shell.portalChooserOptions = ({})
    resetSelection(shell)
}

function setSingleSelection(shell, entriesModel, pathValue, isDirValue, nameValue) {
    var p = String(pathValue || "")
    if (p.length <= 0) {
        return
    }
    var single = ({})
    single[p] = true
    shell.portalChooserSelectedPaths = single
    if (shell.portalChooserMode === "save" && !isDirValue) {
        shell.portalChooserName = String(nameValue || "")
    }
    for (var i = 0; i < entriesModel.count; ++i) {
        var row = entriesModel.get(i)
        if (row && String(row.path || "") === p) {
            shell.portalChooserSelectedIndex = i
            shell.portalChooserAnchorIndex = i
            break
        }
    }
}

function selectIndex(shell, entriesModel, listPane, indexValue, positionMode) {
    var total = Number(entriesModel.count || 0)
    if (total <= 0) {
        shell.portalChooserSelectedIndex = -1
        shell.portalChooserSelectedPaths = ({})
        return
    }
    var idx = Math.max(0, Math.min(total - 1, Number(indexValue)))
    var row = entriesModel.get(idx)
    if (!row) {
        return
    }
    shell.portalChooserSelectedIndex = idx
    setSingleSelection(shell, entriesModel, String(row.path || ""), !!row.isDir, String(row.name || ""))
    if (listPane && listPane.listView && listPane.listView.positionViewAtIndex) {
        listPane.listView.positionViewAtIndex(idx, positionMode)
    }
}

function pageStep(shell, listPane) {
    var rowH = 32
    var viewportH = (listPane && listPane.listView && listPane.listView.height > 0)
            ? Number(listPane.listView.height) : 320
    return Math.max(1, Math.floor(viewportH / rowH) - 1)
}

function pathFieldFocused(pathBar) {
    return !!(pathBar && pathBar.editorActiveFocus)
}

function saveNameFocused(saveNameRow) {
    return !!(saveNameRow && saveNameRow.fieldActiveFocus)
}

function inputFocused(pathBar, saveNameRow) {
    return pathFieldFocused(pathBar) || saveNameFocused(saveNameRow)
}

function entryCount(entriesModel) {
    return PortalChooserNav.entryCount(entriesModel.count)
}

function currentIndexOrZero(shell) {
    return PortalChooserNav.currentIndexOrZero(shell.portalChooserSelectedIndex)
}

function canHandleListShortcut(shell, entriesModel, pathBar, saveNameRow, requireMultiple) {
    return PortalChooserNav.canHandleListShortcut(
                inputFocused(pathBar, saveNameRow),
                !!requireMultiple,
                !!shell.portalChooserAllowMultiple,
                entryCount(entriesModel))
}

function selectRangeTo(shell, entriesModel, listPane, indexValue) {
    var total = Number(entriesModel.count || 0)
    if (total <= 0) {
        resetSelection(shell)
        return
    }
    var target = Math.max(0, Math.min(total - 1, Number(indexValue)))
    if (!shell.portalChooserAllowMultiple) {
        selectIndex(shell, entriesModel, listPane, target, shell.listViewContainMode)
        return
    }
    var anchor = Number(shell.portalChooserAnchorIndex)
    if (anchor < 0 || anchor >= total) {
        anchor = (shell.portalChooserSelectedIndex >= 0) ? Number(shell.portalChooserSelectedIndex) : target
        shell.portalChooserAnchorIndex = anchor
    }
    var from = Math.min(anchor, target)
    var to = Math.max(anchor, target)
    var map = ({})
    for (var i = from; i <= to; ++i) {
        var row = entriesModel.get(i)
        if (row) {
            map[String(row.path || "")] = true
        }
    }
    shell.portalChooserSelectedPaths = map
    shell.portalChooserSelectedIndex = target
    if (listPane && listPane.listView && listPane.listView.positionViewAtIndex) {
        listPane.listView.positionViewAtIndex(target, shell.listViewContainMode)
    }
    updatePreviewPath(shell)
}

function addRangeTo(shell, entriesModel, listPane, indexValue) {
    var total = Number(entriesModel.count || 0)
    if (total <= 0) {
        resetSelection(shell)
        return
    }
    var target = Math.max(0, Math.min(total - 1, Number(indexValue)))
    if (!shell.portalChooserAllowMultiple) {
        selectIndex(shell, entriesModel, listPane, target, shell.listViewContainMode)
        return
    }
    var anchor = Number(shell.portalChooserAnchorIndex)
    if (anchor < 0 || anchor >= total) {
        anchor = (shell.portalChooserSelectedIndex >= 0) ? Number(shell.portalChooserSelectedIndex) : target
        shell.portalChooserAnchorIndex = anchor
    }
    var from = Math.min(anchor, target)
    var to = Math.max(anchor, target)
    var map = ({})
    var oldMap = shell.portalChooserSelectedPaths || ({})
    var keys = Object.keys(oldMap)
    for (var k = 0; k < keys.length; ++k) {
        map[keys[k]] = oldMap[keys[k]]
    }
    for (var i = from; i <= to; ++i) {
        var row = entriesModel.get(i)
        if (row) {
            map[String(row.path || "")] = true
        }
    }
    shell.portalChooserSelectedPaths = map
    shell.portalChooserSelectedIndex = target
    if (listPane && listPane.listView && listPane.listView.positionViewAtIndex) {
        listPane.listView.positionViewAtIndex(target, shell.listViewContainMode)
    }
    updatePreviewPath(shell)
}

function toggleIndexSelection(shell, entriesModel, listPane, indexValue, keepAnchor) {
    var total = Number(entriesModel.count || 0)
    if (total <= 0) {
        resetSelection(shell)
        return
    }
    var idx = Math.max(0, Math.min(total - 1, Number(indexValue)))
    var row = entriesModel.get(idx)
    if (!row) {
        return
    }
    var p = String(row.path || "")
    if (p.length <= 0) {
        return
    }
    var map = ({})
    var oldMap = shell.portalChooserSelectedPaths || ({})
    var keys = Object.keys(oldMap)
    for (var i = 0; i < keys.length; ++i) {
        map[keys[i]] = oldMap[keys[i]]
    }
    map[p] = !(map[p] === true)
    shell.portalChooserSelectedPaths = map
    shell.portalChooserSelectedIndex = idx
    if (!keepAnchor) {
        shell.portalChooserAnchorIndex = idx
    }
    updatePreviewPath(shell)
    if (listPane && listPane.listView && listPane.listView.positionViewAtIndex) {
        listPane.listView.positionViewAtIndex(idx, shell.listViewContainMode)
    }
}

function moveSelection(shell, entriesModel, listPane, pathBar, saveNameRow, delta, mode) {
    if (!canHandleListShortcut(shell, entriesModel, pathBar, saveNameRow, mode === "add")) {
        return
    }
    var next = PortalChooserNav.nextIndex(shell.portalChooserSelectedIndex, delta)
    if (mode === "range") {
        selectRangeTo(shell, entriesModel, listPane, next)
        return
    }
    if (mode === "add") {
        addRangeTo(shell, entriesModel, listPane, next)
        return
    }
    selectIndex(shell, entriesModel, listPane, next, shell.listViewContainMode)
}

function selectBoundary(shell, entriesModel, listPane, pathBar, saveNameRow, atEnd, mode) {
    if (!canHandleListShortcut(shell, entriesModel, pathBar, saveNameRow, mode === "add")) {
        return
    }
    var target = PortalChooserNav.boundaryTarget(entryCount(entriesModel), !!atEnd)
    if (mode === "range") {
        selectRangeTo(shell, entriesModel, listPane, target)
        return
    }
    if (mode === "add") {
        addRangeTo(shell, entriesModel, listPane, target)
        return
    }
    selectIndex(shell,
                entriesModel,
                listPane,
                target,
                atEnd ? shell.listViewEndMode : shell.listViewBeginningMode)
}

function toggleCurrentSelection(shell, entriesModel, listPane, pathBar, saveNameRow, keepAnchor) {
    if (!canHandleListShortcut(shell, entriesModel, pathBar, saveNameRow, false)) {
        return
    }
    var cur = currentIndexOrZero(shell)
    if (!shell.portalChooserAllowMultiple) {
        selectIndex(shell, entriesModel, listPane, cur, shell.listViewContainMode)
        return
    }
    toggleIndexSelection(shell, entriesModel, listPane, cur, !!keepAnchor)
}

function selectAllEntries(shell, entriesModel, listPane, pathBar, saveNameRow) {
    if (!canHandleListShortcut(shell, entriesModel, pathBar, saveNameRow, false)) {
        return
    }
    var total = entryCount(entriesModel)
    if (!shell.portalChooserAllowMultiple) {
        selectBoundary(shell, entriesModel, listPane, pathBar, saveNameRow, false, "single")
        return
    }
    var map = ({})
    for (var i = 0; i < total; ++i) {
        var row = entriesModel.get(i)
        if (row) {
            map[String(row.path || "")] = true
        }
    }
    shell.portalChooserSelectedPaths = map
    shell.portalChooserAnchorIndex = 0
    shell.portalChooserSelectedIndex = Math.max(0, total - 1)
    if (listPane && listPane.listView && listPane.listView.positionViewAtIndex) {
        listPane.listView.positionViewAtIndex(shell.portalChooserSelectedIndex, shell.listViewEndMode)
    }
    updatePreviewPath(shell)
}

function clearWhenHasSelection(shell, pathBar, saveNameRow) {
    if (inputFocused(pathBar, saveNameRow)) {
        return
    }
    if (Number(selectionCount(shell)) <= 0) {
        return
    }
    clearSelectionAndPreview(shell)
}

function handleGoUpShortcut(shell, pathBar, saveNameRow, useLogicHelper) {
    if (useLogicHelper
            && typeof PortalChooserLogicHelper !== "undefined"
            && PortalChooserLogicHelper
            && PortalChooserLogicHelper.shouldGoUpOnBackspace) {
        if (PortalChooserLogicHelper.shouldGoUpOnBackspace(pathFieldFocused(pathBar),
                                                           saveNameFocused(saveNameRow))) {
            goUp(shell)
        }
        return
    }
    if (typeof PortalChooserLogicHelper !== "undefined"
            && PortalChooserLogicHelper
            && PortalChooserLogicHelper.shouldGoUpOnAltUp) {
        if (PortalChooserLogicHelper.shouldGoUpOnAltUp(pathFieldFocused(pathBar),
                                                       saveNameFocused(saveNameRow))) {
            goUp(shell)
        }
        return
    }
    if (useLogicHelper) {
        if (!inputFocused(pathBar, saveNameRow)) {
            goUp(shell)
        }
        return
    }
    goUp(shell)
}

function focusPathEditor(pathBar) {
    if (pathBar) {
        pathBar.focusEditorAndSelectAll()
    }
}

function handleEnterShortcut(shell, pathBar, pathTextValue) {
    var dirFocused = pathFieldFocused(pathBar)
    var pathText = String(pathTextValue || "")
    if (typeof PortalChooserLogicHelper !== "undefined"
            && PortalChooserLogicHelper
            && PortalChooserLogicHelper.shouldLoadDirectoryOnEnter
            && PortalChooserLogicHelper.shouldTriggerPrimaryOnEnter) {
        if (PortalChooserLogicHelper.shouldLoadDirectoryOnEnter(dirFocused)) {
            var model = entriesModel(shell)
            if (model) {
                loadDirectory(shell, model, pathText)
            }
            return
        }
        if (PortalChooserLogicHelper.shouldTriggerPrimaryOnEnter(dirFocused)) {
            triggerPrimaryAction(shell)
        }
        return
    }
    if (dirFocused) {
        var model2 = entriesModel(shell)
        if (model2) {
            loadDirectory(shell, model2, pathText)
        }
        return
    }
    triggerPrimaryAction(shell)
}

function handleEscapeShortcut(shell, pathBar) {
    if (shell.portalChooserPathEditMode && pathFieldFocused(pathBar)) {
        shell.portalChooserPathEditMode = false
        return
    }
    if (shell.portalChooserOverwriteDialogVisible) {
        shell.portalChooserOverwriteDialogVisible = false
        return
    }
    if (typeof PortalChooserLogicHelper !== "undefined"
            && PortalChooserLogicHelper
            && PortalChooserLogicHelper.shouldCancelOnEscape) {
        if (!PortalChooserLogicHelper.shouldCancelOnEscape()) {
            return
        }
    }
    if (Object.keys(shell.portalChooserSelectedPaths || {}).length > 0) {
        clearSelectionAndPreview(shell)
        return
    }
    finishChooser(shell, false, true, "canceled")
}

function isImagePath(pathValue) {
    var p = String(pathValue || "").toLowerCase()
    return p.endsWith(".png")
            || p.endsWith(".jpg")
            || p.endsWith(".jpeg")
            || p.endsWith(".webp")
            || p.endsWith(".bmp")
            || p.endsWith(".gif")
}

function updatePreviewPath(shell) {
    shell.portalChooserPreviewPath = ""
    var keys = Object.keys(shell.portalChooserSelectedPaths || {})
    for (var i = 0; i < keys.length; ++i) {
        var k = keys[i]
        if (shell.portalChooserSelectedPaths[k] !== true) {
            continue
        }
        if (isImagePath(k)) {
            shell.portalChooserPreviewPath = k
            return
        }
    }
}

function defaultWidth(shell) {
    return Math.min(940, Math.max(760, Number(shell.width || 0) - 88))
}

function defaultHeight(shell) {
    return Math.min(620, Math.max(500, Number(shell.height || 0) - 88))
}

function nameColumnWidth(shell, totalWidth) {
    return Math.max(120,
                    Number(totalWidth || 0)
                    - Number(shell.portalChooserDateColumnWidth || 140)
                    - Number(shell.portalChooserKindColumnWidth || 92)
                    - 16)
}

function clampColumns(shell, totalWidth) {
    var avail = Math.max(220, Number(totalWidth || 0) - 16)
    var minName = 120
    var minDate = 90
    var minKind = 80
    shell.portalChooserDateColumnWidth = Math.max(minDate, Math.min(shell.portalChooserDateColumnWidth, avail - minName - minKind))
    shell.portalChooserKindColumnWidth = Math.max(minKind, Math.min(shell.portalChooserKindColumnWidth, avail - minName - minDate))
}

function formatBytes(bytesValue) {
    var b = Number(bytesValue || 0)
    if (!isFinite(b) || b < 0) {
        return ""
    }
    var units = ["B", "KB", "MB", "GB", "TB", "PB"]
    var idx = 0
    while (b >= 1024 && idx < units.length - 1) {
        b /= 1024
        idx += 1
    }
    var precision = (b >= 100 || idx === 0) ? 0 : 1
    return b.toFixed(precision) + " " + units[idx]
}

function loadUiPreferences(shell) {
    var prefGet = function(key, fallbackValue) {
        if (typeof DesktopSettings !== "undefined" && DesktopSettings && DesktopSettings.settingValue) {
            return DesktopSettings.settingValue(String(key || ""), fallbackValue)
        }
        return fallbackValue
    }
    var mode = String(shell.portalChooserMode || "open")
    var widthKey = "portalChooser." + mode + ".width"
    var heightKey = "portalChooser." + mode + ".height"
    var sortKeyKey = "portalChooser." + mode + ".sortKey"
    var sortDescKey = "portalChooser." + mode + ".sortDescending"
    var showHiddenKey = "portalChooser." + mode + ".showHidden"
    var folderKey = "portalChooser." + mode + ".lastFolder"
    var dateColKey = "portalChooser." + mode + ".dateColWidth"
    var kindColKey = "portalChooser." + mode + ".kindColWidth"
    var defW = defaultWidth(shell)
    var defH = defaultHeight(shell)

    var savedW = Number(prefGet(widthKey, defW))
    var savedH = Number(prefGet(heightKey, defH))
    shell.portalChooserWindowWidth = Math.max(760, Math.min(Number(shell.width || 0) - 24, isNaN(savedW) ? defW : savedW))
    shell.portalChooserWindowHeight = Math.max(500, Math.min(Number(shell.height || 0) - 24, isNaN(savedH) ? defH : savedH))

    shell.portalChooserSortKey = String(prefGet(sortKeyKey, "name") || "name")
    shell.portalChooserSortDescending = !!prefGet(sortDescKey, false)
    shell.portalChooserShowHidden = !!prefGet(showHiddenKey, false)
    shell.portalChooserDateColumnWidth = Number(prefGet(dateColKey, 140) || 140)
    shell.portalChooserKindColumnWidth = Number(prefGet(kindColKey, 92) || 92)
    clampColumns(shell, shell.portalChooserWindowWidth - 20)
    return String(prefGet(folderKey, "~") || "~")
}

function saveUiPreferences(shell) {
    if (typeof DesktopSettings === "undefined" || !DesktopSettings || !DesktopSettings.setSettingValue) {
        return
    }
    var mode = String(shell.portalChooserMode || "open")
    DesktopSettings.setSettingValue("portalChooser." + mode + ".sortKey", String(shell.portalChooserSortKey || "name"))
    DesktopSettings.setSettingValue("portalChooser." + mode + ".sortDescending", !!shell.portalChooserSortDescending)
    DesktopSettings.setSettingValue("portalChooser." + mode + ".showHidden", !!shell.portalChooserShowHidden)
    DesktopSettings.setSettingValue("portalChooser." + mode + ".lastFolder", String(shell.portalChooserCurrentDir || "~"))
    DesktopSettings.setSettingValue("portalChooser." + mode + ".dateColWidth", Number(shell.portalChooserDateColumnWidth || 140))
    DesktopSettings.setSettingValue("portalChooser." + mode + ".kindColWidth", Number(shell.portalChooserKindColumnWidth || 92))
}

function saveWindowSize(shell) {
    if (typeof DesktopSettings === "undefined"
            || !DesktopSettings
            || !DesktopSettings.setSettingValue
            || !shell.portalFileChooserVisible) {
        return
    }
    var mode = String(shell.portalChooserMode || "open")
    DesktopSettings.setSettingValue("portalChooser." + mode + ".width", Number(shell.portalChooserWindowWidth || 0))
    DesktopSettings.setSettingValue("portalChooser." + mode + ".height", Number(shell.portalChooserWindowHeight || 0))
}

function wildcardToRegExp(pattern) {
    var p = String(pattern || "")
    if (p.length === 0) {
        return null
    }
    var esc = p.replace(/[-\/\\^$+?.()|[\]{}]/g, "\\$&")
    esc = esc.replace(/\*/g, ".*").replace(/\?/g, ".")
    try {
        return new RegExp("^" + esc + "$", "i")
    } catch (e) {
        return null
    }
}

function filterPatterns(shell) {
    var filters = shell.portalChooserFilters || []
    if (filters.length <= 0 || shell.portalChooserFilterIndex < 0 || shell.portalChooserFilterIndex >= filters.length) {
        return []
    }
    var row = filters[shell.portalChooserFilterIndex]
    if (!row) {
        return []
    }
    var list = row.patterns
    if (!list || list.length <= 0) {
        return []
    }
    return list
}

function fileAccepted(shell, nameValue) {
    var patterns = filterPatterns(shell)
    if (!patterns || patterns.length <= 0) {
        return true
    }
    var n = String(nameValue || "")
    for (var i = 0; i < patterns.length; ++i) {
        var rx = wildcardToRegExp(patterns[i])
        if (rx && rx.test(n)) {
            return true
        }
    }
    return false
}

function expandPath(pathValue) {
    var p = String(pathValue || "").trim()
    if (p.length === 0) {
        return "~"
    }
    return p
}

function defaultPlaces() {
    return [
                { "label": "Home", "path": "~", "icon": "go-home" },
                { "label": "Desktop", "path": "~/Desktop", "icon": "user-desktop" },
                { "label": "Documents", "path": "~/Documents", "icon": "folder-documents" },
                { "label": "Downloads", "path": "~/Downloads", "icon": "folder-download" },
                { "label": "Pictures", "path": "~/Pictures", "icon": "folder-pictures" },
                { "label": "Videos", "path": "~/Videos", "icon": "folder-videos" }
            ]
}

function storageStableKey(deviceValue, labelValue) {
    var d = String(deviceValue || "").trim()
    if (d.length > 0) {
        return d
    }
    return "label:" + String(labelValue || "").trim().toLowerCase()
}

function rebuildPlacesModel(shell, placesModel) {
    if (!placesModel || !placesModel.clear || !placesModel.append) {
        return
    }
    placesModel.clear()
    placesModel.append({
                           "header": true,
                           "label": "Places",
                           "path": "",
                           "icon": "",
                           "mounted": false,
                           "device": "",
                           "storage": false,
                           "isRoot": false,
                           "stableKey": ""
                       })
    var base = defaultPlaces()
    for (var i = 0; i < base.length; ++i) {
        var b = base[i] || ({})
        placesModel.append({
                               "header": false,
                               "label": String(b.label || ""),
                               "path": String(b.path || ""),
                               "icon": String(b.icon || "folder"),
                               "mounted": true,
                               "device": "",
                               "storage": false,
                               "isRoot": false,
                               "stableKey": String(b.path || b.label || "")
                           })
    }
    var storage = shell.portalChooserStoragePlaces || []
    if (storage.length > 0) {
        placesModel.append({
                               "header": true,
                               "label": "Storage",
                               "path": "",
                               "icon": "",
                               "mounted": false,
                               "device": "",
                               "storage": false,
                               "isRoot": false,
                               "stableKey": ""
                           })
        for (var j = 0; j < storage.length; ++j) {
            var s = storage[j] || ({})
            placesModel.append({
                                   "header": false,
                                   "label": String(s.label || ""),
                                   "path": String(s.path || ""),
                                   "icon": String(s.icon || "drive-harddisk-symbolic"),
                                   "mounted": !!s.mounted,
                                   "device": String(s.device || ""),
                                   "storage": true,
                                   "isRoot": !!s.isRoot,
                                   "stableKey": String(s.stableKey || "")
                               })
        }
    }
}

function refreshStoragePlaces(shell, placesModel) {
    var mapped = []
    var seen = ({})
    var previous = shell.portalChooserStoragePlaces || []
    var previousIndex = ({})
    for (var p = 0; p < previous.length; ++p) {
        var prev = previous[p] || ({})
        var prevKey = String(prev.stableKey || storageStableKey(prev.device, prev.label))
        if (prevKey.length > 0 && previousIndex[prevKey] === undefined) {
            previousIndex[prevKey] = p
        }
    }
    if (typeof FileManagerApi !== "undefined" && FileManagerApi && FileManagerApi.storageLocations) {
        var rows = FileManagerApi.storageLocations() || []
        for (var i = 0; i < rows.length; ++i) {
            var row = rows[i]
            if (!row) {
                continue
            }
            var label = String(row.label || row.name || row.device || "Storage")
            var path = String(row.path || row.rootPath || "")
            var device = String(row.device || "")
            if (path === "/") {
                label = "Filesystem"
            }
            if (path.startsWith("/run") || path.startsWith("/boot/efi")) {
                continue
            }
            var key = storageStableKey(device, label)
            if (seen[key] === true) {
                continue
            }
            seen[key] = true
            mapped.push({
                            "label": label,
                            "path": path,
                            "icon": String(row.icon || "drive-harddisk-symbolic"),
                            "mounted": !!row.mounted,
                            "device": device,
                            "storage": true,
                            "isRoot": !!row.isRoot,
                            "stableKey": key,
                            "__oldIndex": (previousIndex[key] !== undefined) ? Number(previousIndex[key]) : (100000 + i)
                        })
        }
        mapped.sort(function(a, b) {
            return Number(a.__oldIndex || 0) - Number(b.__oldIndex || 0)
        })
        for (var m = 0; m < mapped.length; ++m) {
            delete mapped[m].__oldIndex
        }
        if (FileManagerApi.refreshStorageLocationsAsync) {
            FileManagerApi.refreshStorageLocationsAsync()
        }
    }
    shell.portalChooserStoragePlaces = mapped
    rebuildPlacesModel(shell, placesModel)
}

function breadcrumbSegments(shell) {
    var p = String(shell.portalChooserCurrentDir || "")
    if (p.length <= 0) {
        return []
    }
    if (p === "~") {
        return [{ "label": "Home", "path": "~" }]
    }
    if (!p.startsWith("/")) {
        return [{ "label": p, "path": p }]
    }
    var parts = p.split("/")
    var out = [{ "label": "/", "path": "/" }]
    var acc = ""
    for (var i = 0; i < parts.length; ++i) {
        var part = String(parts[i] || "")
        if (part.length <= 0) {
            continue
        }
        acc += "/" + part
        out.push({ "label": part, "path": acc })
    }
    return out
}

function sortRows(shell, rows) {
    rows.sort(function(a, b) {
        var da = !!a.isDir
        var db = !!b.isDir
        if (da !== db) {
            return da ? -1 : 1
        }
        var key = String(shell.portalChooserSortKey || "name")
        var av = ""
        var bv = ""
        if (key === "date") {
            av = Date.parse(String(a.lastModified || ""))
            bv = Date.parse(String(b.lastModified || ""))
            if (isNaN(av)) av = 0
            if (isNaN(bv)) bv = 0
        } else if (key === "type") {
            av = String(a.suffix || "").toLowerCase()
            bv = String(b.suffix || "").toLowerCase()
        } else {
            av = String(a.name || "").toLowerCase()
            bv = String(b.name || "").toLowerCase()
        }
        var cmp = 0
        if (av < bv) cmp = -1
        else if (av > bv) cmp = 1
        if (shell.portalChooserSortDescending) {
            cmp = -cmp
        }
        return cmp
    })
}

function loadDirectory(shell, entriesModel, pathValue) {
    var target = expandPath(pathValue)
    if (typeof FileManagerApi === "undefined" || !FileManagerApi || !FileManagerApi.listDirectory) {
        entriesModel.clear()
        shell.portalChooserErrorText = "File service unavailable"
        return
    }
    var result = FileManagerApi.listDirectory(target, shell.portalChooserShowHidden, true)
    if (!(result && result.ok)) {
        entriesModel.clear()
        clearSelectionAndPreview(shell)
        shell.portalChooserErrorText = String((result && result.error) ? result.error : "Failed to load folder")
        return
    }
    shell.portalChooserErrorText = ""
    shell.portalChooserCurrentDir = String(result.path || target)
    entriesModel.clear()
    var rows = result.entries || []
    var accepted = []
    for (var i = 0; i < rows.length; ++i) {
        var row = rows[i]
        if (!row) {
            continue
        }
        var isDir = !!row.isDir
        if (shell.portalChooserSelectFolders && !isDir) {
            continue
        }
        var name = String(row.name || "")
        if (!isDir && !shell.portalChooserSelectFolders && !fileAccepted(shell, name)) {
            continue
        }
        accepted.push({
                          "name": name,
                          "path": String(row.path || ""),
                          "isDir": isDir,
                          "suffix": String(row.suffix || ""),
                          "lastModified": String(row.lastModified || ""),
                          "bytes": Number(row.size !== undefined ? row.size : -1),
                          "mimeType": String(row.mimeType || row.mime || "")
                      })
    }
    sortRows(shell, accepted)
    for (var a = 0; a < accepted.length; ++a) {
        entriesModel.append(accepted[a])
    }
    clearSelectionAndPreview(shell)
    saveUiPreferences(shell)
}
