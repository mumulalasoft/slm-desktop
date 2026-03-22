import QtQuick 2.15
import Desktop_Shell

Item {
    id: root

    required property var hostRoot
    property var fileManagerApi: null
    property var fileManagerModelFactory: null
    property var cursorController: null
    property var quickTypeClearTimerRef: null
    property var batchOverlayPopupRef: null

    Component.onCompleted: {
        hostRoot.rebuildSidebarItems()
        if (fileManagerApi && fileManagerApi.refreshStorageLocationsAsync) {
            fileManagerApi.refreshStorageLocationsAsync()
        }
        hostRoot.primaryFileModel = hostRoot.fileModel
        if (hostRoot.fileModel && hostRoot.fileModel.currentPath !== undefined) {
            hostRoot.selectedSidebarPath = String(hostRoot.fileModel.currentPath || "~")
            hostRoot.navigationLastPath = hostRoot.selectedSidebarPath
            hostRoot.restoreTabState(hostRoot.selectedSidebarPath)
            if (hostRoot.fileModel.searchText !== undefined) {
                hostRoot.fileModel.searchText = String(hostRoot.toolbarSearchText || "")
            }
        } else {
            hostRoot.restoreTabState("~")
        }
        hostRoot.syncDeepSearchBusyCursor()
        hostRoot.updateBatchOverlayFromApi()
    }

    Component.onDestruction: {
        if (cursorController && cursorController.stopBusy) {
            cursorController.stopBusy()
        }
        var owned = hostRoot.ownedTabModels || []
        for (var i = 0; i < owned.length; ++i) {
            var m = owned[i]
            if (m && fileManagerModelFactory && fileManagerModelFactory.destroyModel) {
                fileManagerModelFactory.destroyModel(m)
            }
        }
        hostRoot.ownedTabModels = []
    }

    Connections {
        target: hostRoot

        function onToolbarSearchTextChanged() {
            if (!hostRoot.fileModel || hostRoot.fileModel.searchText === undefined) {
                return
            }
            if (String(hostRoot.toolbarSearchText || "").trim().length > 0) {
                hostRoot.contentLoading = true
            }
            hostRoot.fileModel.searchText = String(hostRoot.toolbarSearchText || "")
        }

        function onActiveTabIndexChanged() {
            hostRoot.saveTabState()
        }

        function onBatchOverlayVisibleChanged() {
            hostRoot.batchOverlayVisible = false
            if (batchOverlayPopupRef && batchOverlayPopupRef.opened) {
                batchOverlayPopupRef.close()
            }
        }

        function onFileModelChanged() {
            hostRoot.contentLoading = !!hostRoot.fileModel
            hostRoot.syncDeepSearchBusyCursor()
        }

        function onWindowActiveChanged() {
            if (!hostRoot.windowActive) {
                hostRoot.closeAllContextMenus()
            }
        }
    }

    FileManagerModelConnections {
        hostRoot: root.hostRoot
        fileModel: root.hostRoot.fileModel
        quickTypeClearTimerRef: root.quickTypeClearTimerRef
    }

    FileManagerRuntimeConnections {
        hostRoot: root.hostRoot
        fileManagerApi: root.fileManagerApi
    }
}
