import QtQuick 2.15
import "PortalChooserController.js" as PortalChooserController
import "PortalChooserFlow.js" as PortalChooserFlow

QtObject {
    id: api

    required property var shellRoot
    required property var entriesModel
    required property var placesModel
    property var listPane: null
    property var pathBar: null
    property var saveNameRow: null

    function portalChooserResetSelection() {
        shellRoot.portalChooserSelectedIndex = -1
        shellRoot.portalChooserAnchorIndex = -1
        shellRoot.portalChooserSelectedPaths = ({})
    }

    function portalChooserClearSelectionAndPreview() {
        portalChooserResetSelection()
        shellRoot.portalChooserPreviewPath = ""
    }

    function portalChooserDefaultWidth() {
        return PortalChooserController.defaultWidth(shellRoot)
    }

    function portalChooserDefaultHeight() {
        return PortalChooserController.defaultHeight(shellRoot)
    }

    function portalChooserNameColumnWidth(totalWidth) {
        return PortalChooserController.nameColumnWidth(shellRoot, totalWidth)
    }

    function portalChooserClampColumns(totalWidth) {
        PortalChooserController.clampColumns(shellRoot, totalWidth)
    }

    function portalChooserFormatBytes(bytesValue) {
        return PortalChooserController.formatBytes(bytesValue)
    }

    function portalChooserLoadUiPreferences() {
        return PortalChooserController.loadUiPreferences(shellRoot)
    }

    function portalChooserSaveUiPreferences() {
        PortalChooserController.saveUiPreferences(shellRoot)
    }

    function portalChooserSaveWindowSize() {
        PortalChooserController.saveWindowSize(shellRoot)
    }

    function portalChooserFilterPatterns() {
        return PortalChooserController.filterPatterns(shellRoot)
    }

    function portalChooserFileAccepted(nameValue) {
        return PortalChooserController.fileAccepted(shellRoot, nameValue)
    }

    function portalChooserExpandPath(pathValue) {
        return PortalChooserController.expandPath(pathValue)
    }

    function portalChooserPlaces() {
        return PortalChooserController.defaultPlaces()
    }

    function portalChooserStorageStableKey(deviceValue, labelValue) {
        return PortalChooserController.storageStableKey(deviceValue, labelValue)
    }

    function rebuildPortalChooserPlacesModel() {
        PortalChooserController.rebuildPlacesModel(shellRoot, placesModel)
    }

    function portalChooserRefreshStoragePlaces() {
        PortalChooserController.refreshStoragePlaces(shellRoot, placesModel)
    }

    function portalChooserBreadcrumbSegments() {
        return PortalChooserController.breadcrumbSegments(shellRoot)
    }

    function portalChooserSortRows(rows) {
        PortalChooserController.sortRows(shellRoot, rows)
    }

    function portalChooserSetSingleSelection(pathValue, isDirValue, nameValue) {
        PortalChooserController.setSingleSelection(shellRoot, entriesModel, pathValue, isDirValue, nameValue)
    }

    function portalChooserSelectIndex(indexValue, positionMode) {
        var mode = (positionMode === undefined) ? shellRoot.listViewContainMode : positionMode
        PortalChooserController.selectIndex(shellRoot, entriesModel, listPane, indexValue, mode)
    }

    function portalChooserPageStep() {
        return PortalChooserController.pageStep(shellRoot, listPane)
    }

    function portalChooserPathFieldFocused() {
        return PortalChooserController.pathFieldFocused(pathBar)
    }

    function portalChooserSaveNameFocused() {
        return PortalChooserController.saveNameFocused(saveNameRow)
    }

    function portalChooserInputFocused() {
        return PortalChooserController.inputFocused(pathBar, saveNameRow)
    }

    function portalChooserEntryCount() {
        return PortalChooserController.entryCount(entriesModel)
    }

    function portalChooserCurrentIndexOrZero() {
        return PortalChooserController.currentIndexOrZero(shellRoot)
    }

    function portalChooserCanHandleListShortcut(requireMultiple) {
        return PortalChooserController.canHandleListShortcut(
                    shellRoot, entriesModel, pathBar, saveNameRow, requireMultiple)
    }

    function portalChooserMoveSelection(delta, mode) {
        PortalChooserController.moveSelection(
                    shellRoot, entriesModel, listPane, pathBar, saveNameRow, delta, mode)
    }

    function portalChooserSelectBoundary(atEnd, mode) {
        PortalChooserController.selectBoundary(
                    shellRoot, entriesModel, listPane, pathBar, saveNameRow, atEnd, mode)
    }

    function portalChooserToggleCurrentSelection(keepAnchor) {
        PortalChooserController.toggleCurrentSelection(
                    shellRoot, entriesModel, listPane, pathBar, saveNameRow, keepAnchor)
    }

    function portalChooserSelectAllEntries() {
        PortalChooserController.selectAllEntries(
                    shellRoot, entriesModel, listPane, pathBar, saveNameRow)
    }

    function portalChooserClearWhenHasSelection() {
        PortalChooserController.clearWhenHasSelection(shellRoot, pathBar, saveNameRow)
    }

    function portalChooserHandleGoUpShortcut(useLogicHelper) {
        PortalChooserController.handleGoUpShortcut(shellRoot, pathBar, saveNameRow, useLogicHelper)
    }

    function portalChooserFocusPathEditor() {
        PortalChooserController.focusPathEditor(pathBar)
    }

    function portalChooserHandleEnterShortcut(pathTextValue) {
        PortalChooserController.handleEnterShortcut(shellRoot, pathBar, pathTextValue)
    }

    function portalChooserHandleEscapeShortcut() {
        PortalChooserController.handleEscapeShortcut(shellRoot, pathBar)
    }

    function portalChooserSelectRangeTo(indexValue) {
        PortalChooserController.selectRangeTo(shellRoot, entriesModel, listPane, indexValue)
    }

    function portalChooserAddRangeTo(indexValue) {
        PortalChooserController.addRangeTo(shellRoot, entriesModel, listPane, indexValue)
    }

    function portalChooserToggleIndexSelection(indexValue, keepAnchor) {
        PortalChooserController.toggleIndexSelection(shellRoot, entriesModel, listPane, indexValue, keepAnchor)
    }

    function portalChooserIsImagePath(pathValue) {
        return PortalChooserController.isImagePath(pathValue)
    }

    function portalChooserUpdatePreviewPath() {
        PortalChooserController.updatePreviewPath(shellRoot)
    }

    function portalChooserLoadDirectory(pathValue) {
        PortalChooserController.loadDirectory(shellRoot, entriesModel, pathValue)
    }

    function portalChooserGoUp() {
        PortalChooserController.goUp(shellRoot)
    }

    function portalChooserPrimaryActionEnabled() {
        return PortalChooserController.primaryActionEnabled(shellRoot)
    }

    function portalChooserSelectionCount() {
        return PortalChooserFlow.countSelected(shellRoot.portalChooserSelectedPaths)
    }

    function portalChooserInvertSelection() {
        PortalChooserController.invertSelection(shellRoot, entriesModel)
    }

    function portalChooserValidateSaveName(nameValue) {
        return PortalChooserFlow.validateSaveName(nameValue)
    }

    function portalChooserTriggerPrimaryAction() {
        PortalChooserController.triggerPrimaryAction(shellRoot)
    }

    function openPortalFileChooser(requestId, options) {
        PortalChooserController.openChooser(shellRoot, requestId, options)
    }

    function completeInternalPortalChooser(ok, canceled, selected, errorText) {
        PortalChooserController.completeInternalChooser(shellRoot, ok, canceled, selected, errorText)
    }

    function finishPortalFileChooser(ok, canceled, errorText, skipOverwriteCheck) {
        PortalChooserController.finishChooser(shellRoot, ok, canceled, errorText, skipOverwriteCheck)
    }
}
