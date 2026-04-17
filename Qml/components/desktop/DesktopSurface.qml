import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop
import "../../apps/filemanager" as FM
import "../../apps/filemanager/FileManagerSelection.js" as FileManagerSelection
import "../contextmenu" as ContextMenuComp

Item {
    id: root

    property var desktopViewController: null
    property var desktopMenuProvider: null
    property var shellApi: null

    readonly property var fileManagerApiRef: (typeof FileManagerApi !== "undefined") ? FileManagerApi : null
    readonly property var modelFactoryRef: (typeof FileManagerModelFactory !== "undefined") ? FileManagerModelFactory : null

    property var fileModel: null
    property string desktopPath: {
        if (typeof DesktopSettings !== "undefined" && DesktopSettings && DesktopSettings.desktopPath) {
            var p = String(DesktopSettings.desktopPath || "").trim()
            if (p.length > 0) {
                return p
            }
        }
        return "~/Desktop"
    }

    property string viewMode: "grid"
    property int selectedEntryIndex: -1
    property var selectedEntryIndexes: []
    property int selectionAnchorIndex: -1
    property bool contentLoading: false
    property string toolbarSearchText: ""
    property bool trashView: false

    signal selectionChanged(var selection)
    signal directoryChanged(string path)

    function _parentDir(pathValue) {
        var p = String(pathValue || "")
        var idx = p.lastIndexOf("/")
        if (idx <= 0) {
            return "~"
        }
        return p.slice(0, idx)
    }

    function _selectionRows() {
        var out = []
        if (!fileModel || !fileModel.entryAt) {
            return out
        }
        var indexes = selectedEntryIndexes ? selectedEntryIndexes : []
        for (var i = 0; i < indexes.length; ++i) {
            var idx = Number(indexes[i])
            if (idx < 0) {
                continue
            }
            var row = fileModel.entryAt(idx)
            if (row && row.ok) {
                out.push(row)
            }
        }
        return out
    }

    function _syncSelectionToController() {
        var rows = _selectionRows()
        if (desktopViewController && desktopViewController.setSelection) {
            desktopViewController.setSelection(selectedEntryIndexes, rows)
        }
        if (desktopMenuProvider) {
            desktopMenuProvider.selection = rows
            desktopMenuProvider.path = desktopPath
        }
        selectionChanged(rows)
    }

    function _syncItemsToController() {
        if (!desktopViewController || !desktopViewController.setItems || !fileModel || !fileModel.entryAt) {
            return
        }
        var rows = []
        var count = Number(fileModel.count || 0)
        for (var i = 0; i < count; ++i) {
            var row = fileModel.entryAt(i)
            if (row && row.ok) {
                rows.push(row)
            }
        }
        desktopViewController.setItems(rows)
    }

    function _ensureDesktopDirectory() {
        if (!fileManagerApiRef || !fileManagerApiRef.statPath || !fileManagerApiRef.createDirectory) {
            return
        }
        var stat = fileManagerApiRef.statPath(desktopPath)
        if (!stat || !stat.ok || !stat.exists || !stat.isDir) {
            fileManagerApiRef.createDirectory(desktopPath, true)
        }
    }

    function _initModel() {
        if (fileModel || !modelFactoryRef || !modelFactoryRef.createModel) {
            return
        }
        var modelObj = modelFactoryRef.createModel(root)
        if (!modelObj) {
            return
        }
        fileModel = modelObj
        fileModel.currentPath = desktopPath
        if (desktopViewController && desktopViewController.setPath) {
            desktopViewController.setPath(desktopPath)
        }
        if (desktopMenuProvider) {
            desktopMenuProvider.desktopSurface = root
            desktopMenuProvider.path = desktopPath
        }
        directoryChanged(desktopPath)
    }

    function _reloadDesktop() {
        _ensureDesktopDirectory()
        if (!fileModel) {
            _initModel()
            return
        }
        fileModel.currentPath = desktopPath
        if (fileModel.refresh) {
            fileModel.refresh()
        }
        if (desktopMenuProvider) {
            desktopMenuProvider.enabled = true
        }
    }

    function openSelection() {
        if (!fileModel || !fileModel.activate || selectedEntryIndex < 0) {
            return
        }
        fileModel.activate(selectedEntryIndex)
    }

    function renameSelection() {
        if (!fileModel || !fileModel.entryAt || selectedEntryIndex < 0) {
            return
        }
        var row = fileModel.entryAt(selectedEntryIndex)
        if (!row || !row.ok) {
            return
        }
        var p = String(row.path || "")
        if (shellApi && shellApi.detachedFileManagerVisible
                && shellApi.detachedFileManagerWindow
                && shellApi.detachedFileManagerWindow.renamePathIfReady
                && shellApi.detachedFileManagerWindow.renamePathIfReady(p)) {
            return
        }
        if (shellApi && shellApi.pendingDetachedFileManagerRenamePath !== undefined) {
            shellApi.pendingDetachedFileManagerRenamePath = p
            shellApi.detachedFileManagerPath = _parentDir(p)
            shellApi.detachedFileManagerVisible = true
            return
        }
        if (fileModel.renameAt) {
            var baseName = String(row.name || "")
            if (baseName.length <= 0) {
                return
            }
            fileModel.renameAt(selectedEntryIndex, baseName + " - Renamed")
        }
    }

    function _selectedPaths() {
        var rows = _selectionRows()
        var out = []
        for (var i = 0; i < rows.length; ++i) {
            out.push(String(rows[i].path || ""))
        }
        return out
    }

    function compressSelection() {
        if (!fileManagerApiRef || !fileManagerApiRef.startCompressArchive) {
            return
        }
        var paths = _selectedPaths()
        if (paths.length <= 0) {
            return
        }
        var stamp = Qt.formatDateTime(new Date(), "yyyyMMdd-HHmmss")
        var archivePath = String(desktopPath || "~/Desktop") + "/Desktop-Selection-" + stamp + ".tar"
        fileManagerApiRef.startCompressArchive(paths, archivePath, "tar")
    }

    function moveSelectionToTrash() {
        if (!fileManagerApiRef || !fileManagerApiRef.startTrashPaths) {
            return
        }
        var paths = _selectedPaths()
        if (paths.length <= 0) {
            return
        }
        fileManagerApiRef.startTrashPaths(paths)
    }

    function openPropertiesSelection() {
        if (selectedEntryIndex < 0 || !fileModel || !fileModel.entryAt) {
            return
        }
        var row = fileModel.entryAt(selectedEntryIndex)
        if (!row || !row.ok) {
            return
        }
        var p = String(row.path || "")
        if (shellApi && shellApi.detachedFileManagerVisible
                && shellApi.detachedFileManagerWindow
                && shellApi.detachedFileManagerWindow.showPropertiesIfReady
                && shellApi.detachedFileManagerWindow.showPropertiesIfReady(p)) {
            return
        }
        if (shellApi && shellApi.pendingDetachedFileManagerPropertiesPath !== undefined) {
            shellApi.pendingDetachedFileManagerPropertiesPath = p
            shellApi.detachedFileManagerPath = _parentDir(p)
            shellApi.detachedFileManagerVisible = true
            return
        }
        if (fileManagerApiRef && fileManagerApiRef.startOpenPathInFileManager) {
            fileManagerApiRef.startOpenPathInFileManager(p, "desktop-properties")
        }
    }

    function createNewFolder() {
        if (!fileModel || !fileModel.createDirectory) {
            return
        }
        fileModel.createDirectory("New Folder")
    }

    function openTerminalHere() {
        if (typeof AppExecutionGate !== "undefined" && AppExecutionGate && AppExecutionGate.launchTerminalAt) {
            AppExecutionGate.launchTerminalAt(desktopPath)
        }
    }

    function selectAllEntries() {
        if (!fileModel) {
            return
        }
        var out = []
        var count = Number(fileModel.count || 0)
        for (var i = 0; i < count; ++i) {
            out.push(i)
        }
        selectedEntryIndexes = out
        selectedEntryIndex = count > 0 ? Number(out[0]) : -1
        selectionAnchorIndex = selectedEntryIndex
        _syncSelectionToController()
    }

    function clearSelection() {
        selectedEntryIndexes = []
        selectedEntryIndex = -1
        selectionAnchorIndex = -1
        _syncSelectionToController()
    }

    function openHomePath() {
        if (fileModel) {
            fileModel.currentPath = "~"
        }
    }

    function openDesktopPath() {
        if (fileModel) {
            fileModel.currentPath = desktopPath
        }
    }

    function applySelectionRequest(indexValue, modifiers) {
        FileManagerSelection.applySelectionRequest(root, Qt, Number(indexValue), Number(modifiers || 0))
        _syncSelectionToController()
    }

    function applySelectionRect(indexesValue, modifiers, anchorIndex) {
        selectedEntryIndexes = FileManagerSelection.normalizeSelection(root, indexesValue)
        selectedEntryIndex = selectedEntryIndexes.length > 0 ? selectedEntryIndexes[0] : -1
        selectionAnchorIndex = Number(anchorIndex || selectedEntryIndex)
        _syncSelectionToController()
    }

    Component.onCompleted: {
        _reloadDesktop()
    }

    onDesktopPathChanged: {
        _reloadDesktop()
    }

    Timer {
        id: backendRetryTimer
        interval: 1200
        repeat: true
        running: !root.fileModel
        onTriggered: {
            if (root.desktopMenuProvider) {
                root.desktopMenuProvider.enabled = false
            }
            root._initModel()
        }
    }

    Connections {
        target: root.fileModel
        ignoreUnknownSignals: true
        function onCurrentPathChanged() {
            var p = String(root.fileModel && root.fileModel.currentPath ? root.fileModel.currentPath : root.desktopPath)
            if (root.desktopViewController && root.desktopViewController.setPath) {
                root.desktopViewController.setPath(p)
            }
            if (root.desktopMenuProvider) {
                root.desktopMenuProvider.path = p
                root.desktopMenuProvider.enabled = true
            }
            root.directoryChanged(p)
        }
        function onCountChanged() {
            root._syncItemsToController()
            root._syncSelectionToController()
        }
    }

    Connections {
        target: root.fileManagerApiRef
        ignoreUnknownSignals: true
        function onDirectoryChanged(pathValue) {
            var p = String(pathValue || "")
            if (p === String(root.desktopPath || "") && root.fileModel && root.fileModel.refresh) {
                root.fileModel.refresh()
            }
        }
    }

    FM.FileManagerContentView {
        id: contentView
        anchors.fill: parent
        anchors.topMargin: 42
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        anchors.bottomMargin: 20
        fileModel: root.fileModel
        viewMode: root.viewMode
        searchText: root.toolbarSearchText
        contentLoading: root.contentLoading
        trashView: root.trashView
        selectedIndex: root.selectedEntryIndex
        selectedIndexes: root.selectedEntryIndexes
        thumbnailsEnabled: true
        hostItem: root

        onSelectedIndexRequested: function(index, modifiers) {
            root.applySelectionRequest(index, modifiers)
        }
        onActivateRequested: function(index) {
            root.applySelectionRequest(index, Qt.NoModifier)
            root.openSelection()
        }
        onBackgroundContextRequested: function(x, y) {
            root.clearSelection()
            var p = contentView.mapToItem(root, x, y)
            desktopCtxMenu.openFolderMenu(p.x, p.y)
        }
        onContextMenuRequested: function(index, x, y) {
            root.applySelectionRequest(index, Qt.NoModifier)
            var p = contentView.mapToItem(root, x, y)
            desktopCtxMenu.openFolderMenu(p.x, p.y)
        }
        onSelectionRectRequested: function(indexes, modifiers, anchorIndex) {
            root.applySelectionRect(indexes, modifiers, anchorIndex)
        }
        onClearSelectionRequested: root.clearSelection()
        onCreateFolderRequested: root.createNewFolder()
        onOpenHomeRequested: root.openHomePath()
    }

    ContextMenuComp.DesktopContextMenu {
        id: desktopCtxMenu
        anchors.fill: parent
        desktopPath: root.desktopPath

        onNewFolder: root.createNewFolder()
        onNewTextFile: {
            if (root.fileManagerApiRef && root.fileManagerApiRef.createEmptyFile) {
                root.fileManagerApiRef.createEmptyFile(String(root.desktopPath || "~/Desktop") + "/New File.txt")
            }
        }
        onOpenTerminal: root.openTerminalHere()
    }
}
