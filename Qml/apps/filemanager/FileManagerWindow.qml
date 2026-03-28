import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import Slm_Desktop
import Style
import "." as FM
import "./FileManagerOps.js" as FileManagerOps
import "./FileManagerKeys.js" as FileManagerKeys
import "./FileManagerSidebarOps.js" as FileManagerSidebarOps
import "./FileManagerSidebarContextOps.js" as FileManagerSidebarContextOps
import "./FileManagerTabs.js" as FileManagerTabs
import "./FileManagerSelection.js" as FileManagerSelection
import "./FileManagerNavigation.js" as FileManagerNavigation
import "./FileManagerOpenWith.js" as FileManagerOpenWith
import "./FileManagerBatch.js" as FileManagerBatch
import "./FileManagerWindowActions.js" as FileManagerWindowActions
import "./FileManagerContentDnd.js" as FileManagerContentDnd
import "./FileManagerPathOps.js" as FileManagerPathOps
import "./FileManagerDisplay.js" as FileManagerDisplay
import "./FileManagerPathUtils.js" as FileManagerPathUtils

Rectangle {
    id: root
    property var fileModel: null
    property string viewMode: "list"
    property int selectedEntryIndex: -1
    property var selectedEntryIndexes: []
    property int selectionAnchorIndex: -1
    property string selectedSidebarPath: "~"
    property int contextEntryIndex: -1
    property string contextEntryPath: ""
    property string contextEntryName: ""
    property bool contextEntryIsDir: false
    readonly property bool contextEntryProtected: isProtectedEntryPath(
                                                      contextEntryPath)
    property string renameDialogError: ""
    property string renameInputText: ""
    property var contextOpenWithApps: []
    property var contextOpenWithAllApps: []
    property string contextOpenWithRequestId: ""
    property string contextOpenWithPath: ""
    property var contextSlmActions: []
    property string contextSlmActionsRequestId: ""
    property var contextSlmShareActions: []
    property string contextSlmShareActionsRequestId: ""
    property bool slmTreeDebugEnabled: !!slmActionTreeDebug
    property var pendingSlmActionRequests: ({})
    property var pendingSlmDragDropRequests: ({})
    property var pendingOpenActions: ({})
    property string pendingMountDevice: ""
    property string pendingConnectServerUri: ""
    property var connectServerTypes: [{
            "label": "Public FTP",
            "scheme": "ftp",
            "port": 21
        }, {
            "label": "FTP (with login)",
            "scheme": "ftp",
            "port": 21
        }, {
            "label": "SSH",
            "scheme": "sftp",
            "port": 22
        }, {
            "label": "AFP (Apple Filing Protocol)",
            "scheme": "afp",
            "port": 548
        }, {
            "label": "Windows share",
            "scheme": "smb",
            "port": 445
        }, {
            "label": "WebDAV (HTTP)",
            "scheme": "dav",
            "port": 80
        }, {
            "label": "Secure WebDAV (HTTPS)",
            "scheme": "davs",
            "port": 443
        }]
    property int connectServerTypeIndex: 4
    property string connectServerHost: ""
    property int connectServerPort: 445
    property string connectServerFolder: "/"
    property string connectServerError: ""
    property bool connectServerBusy: false
    property var storageOrderMap: ({})
    property int storageOrderNext: 1
    property string openWithSearchText: ""
    property string openWithDialogMode: "open" // open | openin | setdefault
    property string clipboardPath: ""
    property var clipboardPaths: []
    property bool clipboardCut: false
    property bool toolbarSearchExpanded: false
    property string toolbarSearchText: ""
    property bool contentLoading: false
    property var navigationBackStack: []
    property var navigationForwardStack: []
    property bool navigationInternalChange: false
    property string navigationLastPath: ""
    property bool batchOverlayVisible: false
    property string batchOverlayTitle: ""
    property string batchOverlayDetail: ""
    property real batchOverlayProgress: 0.0
    property bool batchOverlayPaused: false
    property bool batchOverlayIndeterminate: false
    property bool dndActive: false
    property bool dndCopyMode: false
    property var dndSourcePaths: []
    property string dndSidebarHoverPath: ""
    property string dndSidebarSpringOpenPath: ""
    property real dndGhostX: 0
    property real dndGhostY: 0
    property string dndGhostLabel: ""
    property int activeTabIndex: 0
    property bool tabStateReady: false
    property var primaryFileModel: null
    property var ownedTabModels: []
    property var tabModelRefs: []
    property string sidebarContextPath: ""
    property string sidebarContextLabel: ""
    property string sidebarContextRowType: ""
    property string sidebarContextDevice: ""
    property bool sidebarContextMounted: false
    property bool sidebarContextBrowsable: false
    property var compressPendingPaths: []
    property string compressOutputDirectory: "~"
    property string compressFormat: "tar"
    property string quickPreviewTitleText: ""
    property string quickPreviewMetaText: ""
    property string quickPreviewImageSource: ""
    property string quickPreviewFallbackIconSource: ""
    property var propertiesEntry: ({})
    property var propertiesStat: ({})
    property int propertiesTabIndex: 0
    property string propertiesOpenWithName: ""
    property var propertiesOpenWithApps: []
    property int propertiesOpenWithCurrentIndex: -1
    property string propertiesOpenWithRequestId: ""
    property string propertiesOpenWithPath: ""
    property var folderShareInfoCache: ({})
    property string propertiesSharePath: ""
    property string quickTypeBuffer: ""
    property string pendingPortalChooserRequestId: ""
    property string pendingPortalChooserAction: ""
    property var pendingPortalChooserSources: []
    readonly property bool propertiesShowDeviceUsage: !!(propertiesStat
                                                         && propertiesStat.isDir)
                                                      && Number(
                                                          (propertiesStat
                                                           && propertiesStat.volumeBytesTotal) ? propertiesStat.volumeBytesTotal : -1) > 0
    readonly property bool canNavigateBack: navigationBackStack
                                            && navigationBackStack.length > 0
    readonly property bool canNavigateForward: navigationForwardStack
                                               && navigationForwardStack.length > 0
    readonly property string trashFilesPath: "~/.local/share/Trash/files"
    readonly property var fileManagerApiRef: (typeof FileManagerApi !== "undefined") ? FileManagerApi : null
    readonly property var modelFactoryRef: (typeof FileManagerModelFactory !== "undefined") ? FileManagerModelFactory : null
    readonly property var uiPreferencesRef: (typeof UIPreferences !== "undefined") ? UIPreferences : null
    readonly property var tabModelRef: tabModel
    readonly property var appCommandRouterRef: (typeof AppCommandRouter !== "undefined") ? AppCommandRouter : null
    readonly property var notificationManagerRef: (typeof NotificationManager !== "undefined") ? NotificationManager : null
    readonly property var cursorControllerRef: (typeof CursorController !== "undefined") ? CursorController : null
    readonly property var dialogsRef: fileManagerDialogs
    readonly property var connectServerDialogRef: dialogsRef ? dialogsRef.connectServerDialogRef : null
    readonly property var batchOverlayPopupDialogRef: dialogsRef ? dialogsRef.batchOverlayPopupRef : null
    readonly property var fileManagerMenusDialogRef: dialogsRef ? dialogsRef.fileManagerMenusRef : null
    readonly property var sidebarContextMenuDialogRef: dialogsRef ? dialogsRef.sidebarContextMenuRef : null
    readonly property var clearRecentsDialogRef: dialogsRef ? dialogsRef.clearRecentsDialogRef : null
    readonly property var quickPreviewDialogRef: dialogsRef ? dialogsRef.quickPreviewDialogRef : null
    readonly property var renameDialogRef: dialogsRef ? dialogsRef.renameDialogRef : null
    readonly property var propertiesDialogRef: dialogsRef ? dialogsRef.propertiesDialogRef : null
    readonly property var compressDialogRef: dialogsRef ? dialogsRef.compressDialogRef : null
    readonly property var openWithDialogRef: dialogsRef ? dialogsRef.openWithDialogRef : null
    readonly property var folderShareDialogRef: dialogsRef ? dialogsRef.folderShareDialogRef : null
    readonly property var shareSheetRef: dialogsRef ? dialogsRef.shareSheetRef : null

    function isRecentPath(pathValue) {
        return String(pathValue || "").trim() === "__recent__"
    }
    function isTrashPath(pathValue) {
        var p = String(pathValue || "")
        if (p.length === 0) {
            return false
        }
        return p === "__trash__" || p === trashFilesPath || p.indexOf(
                    "/.local/share/Trash/files") >= 0 || p.indexOf(
                    "trash://") === 0
    }
    readonly property bool recentView: isRecentPath(selectedSidebarPath)
    readonly property bool trashView: isTrashPath(selectedSidebarPath)
    signal closeRequested
    signal openInNewWindowRequested(string path)
    signal printRequested(string documentUri, string documentTitle, bool preferPdfOutput)
    readonly property var hostWindow: Window.window
    readonly property bool useNativeWindowDecoration: hostWindow ? ((hostWindow.flags & Qt.FramelessWindowHint) === 0) : false
    readonly property bool windowActive: hostWindow ? hostWindow.active : true
    readonly property bool windowMaximized: hostWindow ? hostWindow.visibility === Window.Maximized
                                                         || hostWindow.visibility
                                                         === Window.FullScreen : false
    readonly property real sidebarMenuFontPx: Math.max(12,
                                                       Number(Theme.fontSize(
                                                                  "body")))
    readonly property real sidebarEntryIconPx: Math.max(
                                                   18, Math.round(
                                                       sidebarMenuFontPx * 1.30))
    readonly property real sidebarActionIconPx: Math.max(
                                                    13, Math.round(
                                                        sidebarMenuFontPx * 0.95))

    color: "transparent"
    radius: Theme.radiusWindow
    border.width: Theme.borderWidthNone
    border.color: "transparent"
    antialiasing: true
    focus: true

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: Theme.color("fileManagerWindowBg")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("fileManagerWindowBorder")
        antialiasing: true
        z: -1
    }

    function openPath(pathValue) { FileManagerPathOps.openPath(root, fileManagerApiRef, pathValue) }
    function initSingleTab(pathValue) { FileManagerPathOps.initSingleTab(root, modelFactoryRef, pathValue) }
    function openConnectServerDialog() { FileManagerPathOps.openConnectServerDialog(root, connectServerDialogRef) }
    function submitConnectServer() { FileManagerPathOps.submitConnectServer(root, fileManagerApiRef) }
    function tabTitleFromPath(pathValue) { return FileManagerTabs.tabTitleFromPath(root, pathValue) }
    function updateTabPath(indexValue, pathValue) { FileManagerTabs.updateTabPath(root, uiPreferencesRef, indexValue, pathValue) }
    function addTab(pathValue, activate) { FileManagerTabs.addTab(root, modelFactoryRef, uiPreferencesRef, pathValue, activate) }
    function openPathInNewTab(pathValue) { FileManagerTabs.openPathInNewTab(root, modelFactoryRef, uiPreferencesRef, pathValue) }
    function switchToTab(indexValue) { FileManagerTabs.switchToTab(root, indexValue) }
    function closeTab(indexValue) { FileManagerTabs.closeTab(root, modelFactoryRef, uiPreferencesRef, indexValue) }
    function saveTabState() { FileManagerTabs.saveTabState(root, uiPreferencesRef) }
    function restoreTabState(defaultPath) { FileManagerTabs.restoreTabState(root, uiPreferencesRef, defaultPath) }
    function openContextEntryInNewTab() { FileManagerTabs.openContextEntryInNewTab(root, modelFactoryRef, uiPreferencesRef) }
    function openSidebarContextPath(pathValue) { FileManagerSidebarContextOps.openSidebarContextPath(root, pathValue) }
    function openSidebarContextPathInNewTab(pathValue) { FileManagerSidebarContextOps.openSidebarContextPathInNewTab(root, pathValue) }
    function openSidebarContextPathInNewWindow(pathValue) { FileManagerSidebarContextOps.openSidebarContextPathInNewWindow(root, pathValue) }
    function sidebarContextCanOpenPath() { return FileManagerSidebarContextOps.sidebarContextCanOpenPath(root) }
    function sidebarContextCanOpenInNewWindow() { return FileManagerSidebarContextOps.sidebarContextCanOpenInNewWindow(root) }
    function sidebarContextCanMount() { return FileManagerSidebarContextOps.sidebarContextCanMount(root) }
    function sidebarContextCanUnmount() { return FileManagerSidebarContextOps.sidebarContextCanUnmount(root) }
    function sidebarContextMount() { FileManagerSidebarContextOps.sidebarContextMount(root, fileManagerApiRef) }
    function sidebarContextUnmount() { FileManagerSidebarContextOps.sidebarContextUnmount(root, fileManagerApiRef) }
    function sidebarContextRevealInFileManager() { FileManagerSidebarContextOps.sidebarContextRevealInFileManager(root, fileManagerApiRef) }
    function sidebarContextCopyPath() { FileManagerSidebarContextOps.sidebarContextCopyPath(root, fileManagerApiRef) }
    function storageUsageRatio(bytesAvailableValue, bytesTotalValue) { return FileManagerSidebarContextOps.storageUsageRatio(bytesAvailableValue, bytesTotalValue) }
    function formatStorageBytes(bytesValue) { return FileManagerSidebarContextOps.formatStorageBytes(bytesValue) }
    function storageCapacityText(bytesAvailableValue, bytesTotalValue) { return FileManagerSidebarContextOps.storageCapacityText(bytesAvailableValue, bytesTotalValue) }
    function formatDateTimeHuman(isoValue) { return FileManagerDisplay.formatDateTimeHuman(Qt, isoValue) }
    function fileTypeDisplay(statValue, entryValue) { return FileManagerDisplay.fileTypeDisplay(statValue, entryValue) }
    function locationDisplay(statValue, entryValue) { return FileManagerDisplay.locationDisplay(root, statValue, entryValue) }
    function storageUsageColor(ratioValue) { return FileManagerSidebarContextOps.storageUsageColor(Theme, ratioValue) }
    function storageTrackColor(mountedValue) { return FileManagerSidebarContextOps.storageTrackColor(Theme, mountedValue) }
    function toggleStorageMount(rowPath, rowDevice, mounted) { FileManagerSidebarContextOps.toggleStorageMount(root, fileManagerApiRef, rowPath, rowDevice, mounted) }
    function openHome() { openPath("~") }
    function isPrimaryModifier(modifiers) { return FileManagerSelection.isPrimaryModifier(modifiers, Qt) }
    function isTextEntryFocusItem(item) { return FileManagerSelection.isTextEntryFocusItem(item) }
    function isEditingTextInput() { return FileManagerSelection.isEditingTextInput(root) }

    function isProtectedEntryPath(pathValue) {
        var p = String(pathValue || "")
        if (p.length <= 0) {
            return false
        }
        if (fileManagerApiRef && fileManagerApiRef.isProtectedPath) {
            return !!fileManagerApiRef.isProtectedPath(p)
        }
        return false
    }

    function clearSelection() { FileManagerSelection.clearSelection(root) }
    function setSingleSelection(indexValue) { FileManagerSelection.setSingleSelection(root, indexValue) }
    function normalizeSelection(indexesValue) { FileManagerSelection.normalizeSelection(root, indexesValue) }
    function applySelectionRequest(indexValue, modifiers) { FileManagerSelection.applySelectionRequest(root, Qt, indexValue, modifiers) }
    function selectAllVisibleEntries() { FileManagerSelection.selectAllVisibleEntries(root) }

    function openToolbarSearch(selectAllText) {
        var headerRef = windowBody ? windowBody.headerBarRef : null
        if (headerRef && headerRef.focusSearchField) {
            headerRef.focusSearchField(selectAllText)
            return
        }
        root.toolbarSearchExpanded = true
    }

    function closeToolbarSearch(clearText) {
        if (!!clearText) {
            root.toolbarSearchText = ""
            var clearHeaderRef = windowBody ? windowBody.headerBarRef : null
            if (clearHeaderRef && clearHeaderRef.clearSearchField) {
                clearHeaderRef.clearSearchField()
            }
        }
        root.toolbarSearchExpanded = false
        var blurHeaderRef = windowBody ? windowBody.headerBarRef : null
        if (blurHeaderRef && blurHeaderRef.blurSearchField) {
            blurHeaderRef.blurSearchField()
        }
    }

    function parentPathOf(pathValue) { return FileManagerPathUtils.parentPathOf(pathValue) }
    function isPathSameOrInside(pathValue, parentValue) { return FileManagerPathUtils.isPathSameOrInside(pathValue, parentValue) }

    function sidebarDropTargetAt(sceneX, sceneY) {
        var sidebarRef = windowBody ? windowBody.sidebarPaneRef : null
        if (!sidebarRef || !sidebarRef.dropTargetAt) {
            return ({ "ok": false })
        }
        return sidebarRef.dropTargetAt(sceneX, sceneY)
    }

    function beginContentDrag(index, pathValue, nameValue, copyMode, sceneX, sceneY) { FileManagerContentDnd.beginContentDrag(root, windowBody && windowBody.mainPaneRef ? windowBody.mainPaneRef.contentViewRef : null, dndSidebarSpringOpenTimer, index, pathValue, nameValue, copyMode, sceneX, sceneY) }

    function updateContentDrag(sceneX, sceneY, copyMode) { FileManagerContentDnd.updateContentDrag(root, sidebarDropTargetAt, dndSidebarSpringOpenTimer, sceneX, sceneY, copyMode) }

    function finishContentDrag() { FileManagerContentDnd.finishContentDrag(root, windowBody && windowBody.mainPaneRef ? windowBody.mainPaneRef.contentViewRef : null, dndSidebarSpringOpenTimer, fileManagerApiRef) }

    function applySelectionRect(indexesValue, modifiers, anchorIndex) { FileManagerSelection.applySelectionRect(root, Qt, indexesValue, modifiers, anchorIndex) }
    function handleTypeToSelect(textValue) { return FileManagerSelection.handleTypeToSelect(root, textValue, quickTypeClearTimer, windowBody && windowBody.mainPaneRef ? windowBody.mainPaneRef.contentViewRef : null) }
    function syncDeepSearchBusyCursor() { FileManagerBatch.syncDeepSearchBusyCursor(root, cursorControllerRef) }
    function batchOperationLabel(typeValue) { return FileManagerBatch.batchOperationLabel(typeValue) }
    function updateBatchOverlayFromApi() { FileManagerBatch.updateBatchOverlayFromApi(root, batchOverlayPopupDialogRef) }
    function toggleBatchPauseResume() { FileManagerBatch.toggleBatchPauseResume(root, fileManagerApiRef) }
    function cancelBatchOperation() { FileManagerBatch.cancelBatchOperation(root, fileManagerApiRef) }
    function recordNavigationPath(pathValue) { FileManagerNavigation.recordNavigationPath(root, pathValue) }

    Timer {
        id: dndSidebarSpringOpenTimer
        interval: 560
        repeat: false
        onTriggered: {
            if (!root.dndActive) {
                return
            }
            var p = String(root.dndSidebarSpringOpenPath || "")
            var current = String(
                        root.fileModel && root.fileModel.currentPath
                        !== undefined ? root.fileModel.currentPath : "")
            if (p.length <= 0 || p === "__trash__" || p === current) {
                return
            }
            root.openPath(p)
        }
    }

    Timer {
        id: quickTypeClearTimer
        interval: 1000
        repeat: false
        onTriggered: {
            root.quickTypeBuffer = ""
        }
    }

    function navigateBack() { FileManagerNavigation.navigateBack(root) }
    function navigateForward() { FileManagerNavigation.navigateForward(root) }
    function titleButtonIcon(kind, hovered, pressed) { return FileManagerWindowActions.titleButtonIcon(root, kind, hovered, pressed) }
    function minimizeWindow() { FileManagerWindowActions.minimizeWindow(hostWindow) }
    function toggleMaximizeWindow() { FileManagerWindowActions.toggleMaximizeWindow(hostWindow, windowMaximized, Window) }
    function notifyResult(title, resultValue) { FileManagerWindowActions.notifyResult(notificationManagerRef, title, resultValue) }

    function basename(pathValue) { return FileManagerPathUtils.basename(pathValue) }
    function createAsyncRequestId() { return FileManagerWindowActions.createAsyncRequestId() }
    function trackOpenAction(requestId, title, refreshOpenWith) { FileManagerWindowActions.trackOpenAction(root, requestId, title, refreshOpenWith) }
    function openTargetViaExecutionGate(pathValue, sourceLabel) { return FileManagerWindowActions.openTargetViaExecutionGate(root, appCommandRouterRef, fileManagerApiRef, pathValue, sourceLabel) }
    function openContextEntry() { FileManagerWindowActions.openContextEntry(root, root.fileModel, appCommandRouterRef, fileManagerApiRef) }
    function openContextEntryInNewWindow() { FileManagerWindowActions.openContextEntryInNewWindow(root) }
    function contextOpenWithEntry(indexValue) { return FileManagerOpenWith.contextOpenWithEntry(root, indexValue) }
    function contextDefaultOpenWithEntry() { return FileManagerOpenWith.contextDefaultOpenWithEntry(root) }
    function contextRecommendedOpenWithEntry(indexValue) { return FileManagerOpenWith.contextRecommendedOpenWithEntry(root, indexValue) }
    function contextRecommendedOpenWithEntries() { return FileManagerOpenWith.contextRecommendedOpenWithEntries(root) }
    function contextRecommendedOpenWithCount() { return FileManagerOpenWith.contextRecommendedOpenWithCount(root) }
    function openWithSortScore(row) { return FileManagerOpenWith.openWithSortScore(row) }
    function sortOpenWithRows(rows) { return FileManagerOpenWith.sortOpenWithRows(rows) }
    function openWithSecondaryTag(row) { return FileManagerOpenWith.openWithSecondaryTag(row) }
    function filteredOpenWithCount() { return FileManagerOpenWith.filteredOpenWithCount(root) }
    function refreshContextOpenWithApps() { FileManagerOpenWith.refreshContextOpenWithApps(root, fileManagerApiRef) }
    function refreshContextSlmActions(targetValue) {
        root.contextSlmActions = []
        root.contextSlmActionsRequestId = ""
        if (!fileManagerApiRef || !fileManagerApiRef.startSlmContextActions) {
            return
        }
        var paths = root.selectedPaths()
        if (!paths || paths.length <= 0) {
            var cp = String(root.contextEntryPath || "")
            if (cp.length > 0) {
                paths = [cp]
            }
        }
        var rid = root.createAsyncRequestId()
        var res = fileManagerApiRef.startSlmContextActions(paths || [], String(targetValue || ""), rid)
        if (!!res && !!res.ok) {
            if (res.actions && res.actions.length !== undefined) {
                root.contextSlmActions = res.actions
            }
            root.contextSlmActionsRequestId = rid
        }
    }
    function refreshContextSlmShareActions(targetValue) {
        root.contextSlmShareActions = []
        root.contextSlmShareActionsRequestId = ""
        if (!fileManagerApiRef || !fileManagerApiRef.startSlmCapabilityActions) {
            return
        }
        var paths = root.selectedPaths()
        if (!paths || paths.length <= 0) {
            var cp = String(root.contextEntryPath || "")
            if (cp.length > 0) {
                paths = [cp]
            }
        }
        var rid = root.createAsyncRequestId()
        var res = fileManagerApiRef.startSlmCapabilityActions("Share",
                                                               paths || [],
                                                               String(targetValue || ""),
                                                               rid)
        if (!!res && !!res.ok) {
            if (res.actions && res.actions.length !== undefined) {
                root.contextSlmShareActions = res.actions
            }
            root.contextSlmShareActionsRequestId = rid
        }
    }
    function debugDumpSlmContextMenuTree(targetValue) {
        if (!root.slmTreeDebugEnabled || !fileManagerApiRef
                || !fileManagerApiRef.startSlmContextMenuTreeDebug
                || !fileManagerApiRef.slmContextMenuController) {
            return
        }
        var paths = root.selectedPaths()
        if (!paths || paths.length <= 0) {
            var cp = String(root.contextEntryPath || "")
            if (cp.length > 0) {
                paths = [cp]
            }
        }
        var res = fileManagerApiRef.startSlmContextMenuTreeDebug(paths || [],
                                                                  String(targetValue || ""))
        var seen = ({})
        var ctl = fileManagerApiRef.slmContextMenuController
        if (!ctl || !ctl.children) {
            return
        }
        function dumpNode(nodeId, depth) {
            if (depth > 8) {
                return
            }
            var id = String(nodeId || "root")
            if (seen[id]) {
                return
            }
            seen[id] = true
            var rows = ctl.children(id) || []
            for (var i = 0; i < rows.length; ++i) {
                var row = rows[i] || ({})
                var rowId = String(row.id || "")
                var rowType = String(row.type || "")
                if (rowType === "submenu" && !!row.hasChildren && rowId.length > 0) {
                    dumpNode(rowId, depth + 1)
                }
            }
        }
        dumpNode("root", 0)
    }
    function slmContextMenuRootItems() {
        if (!fileManagerApiRef || !fileManagerApiRef.slmContextMenuController) {
            return []
        }
        var ctl = fileManagerApiRef.slmContextMenuController
        if (!ctl || !ctl.rootItems) {
            return []
        }
        return ctl.rootItems()
    }
    function slmContextMenuChildren(nodeIdValue) {
        if (!fileManagerApiRef || !fileManagerApiRef.slmContextMenuController) {
            return []
        }
        var ctl = fileManagerApiRef.slmContextMenuController
        if (!ctl || !ctl.children) {
            return []
        }
        return ctl.children(String(nodeIdValue || "root"))
    }
    function invokeSlmContextAction(actionIdValue) {
        var actionId = String(actionIdValue || "")
        if (actionId.length <= 0 || !fileManagerApiRef || !fileManagerApiRef.slmContextMenuController) {
            return
        }
        var ctl = fileManagerApiRef.slmContextMenuController
        if (ctl && ctl.invoke) {
            ctl.invoke(actionId)
        }
    }
    function invokeSlmCapabilityAction(capabilityValue, actionIdValue, pathsValue) {
        var capability = String(capabilityValue || "")
        var actionId = String(actionIdValue || "")
        if (capability.length <= 0 || actionId.length <= 0
                || !fileManagerApiRef
                || !fileManagerApiRef.startInvokeSlmCapabilityAction) {
            return
        }
        var paths = pathsValue
        if (!paths) {
            paths = root.selectedPaths()
            if (!paths || paths.length <= 0) {
                var cp = String(root.contextEntryPath || "")
                if (cp.length > 0) {
                    paths = [cp]
                }
            }
        }
        var rid = root.createAsyncRequestId()
        var pending = root.pendingSlmActionRequests || ({})
        var next = {}
        var keys = Object.keys(pending)
        for (var i = 0; i < keys.length; ++i) {
            next[keys[i]] = pending[keys[i]]
        }
        next[rid] = {
            "title": capability === "Share" ? "Share" : (capability === "DragDrop" ? "Drag & Drop" : "Action"),
            "actionId": actionId
        }
        root.pendingSlmActionRequests = next
        fileManagerApiRef.startInvokeSlmCapabilityAction(capability,
                                                         actionId,
                                                         paths || [],
                                                         rid)
    }

    function startResolveSlmDragDropAction(sourceUris, targetUri, copyMode) {
        if (!fileManagerApiRef || !fileManagerApiRef.startResolveSlmDragDropAction) {
            return
        }
        var rid = root.createAsyncRequestId()
        var next = {}
        var keys = Object.keys(root.pendingSlmDragDropRequests)
        for (var i = 0; i < keys.length; ++i) {
            next[keys[i]] = root.pendingSlmDragDropRequests[keys[i]]
        }
        next[rid] = {
            "sourceUris": sourceUris,
            "targetUri": targetUri,
            "copyMode": !!copyMode
        }
        root.pendingSlmDragDropRequests = next
        fileManagerApiRef.startResolveSlmDragDropAction(sourceUris, targetUri, rid)
    }
    function refreshPropertiesOpenWithApps(pathValue) { FileManagerOpenWith.refreshPropertiesOpenWithApps(root, fileManagerApiRef, pathValue) }
    function applyPropertiesOpenWithSelection(indexValue) { FileManagerOpenWith.applyPropertiesOpenWithSelection(root, fileManagerApiRef, indexValue) }
    function folderShareInfoForPath(pathValue) {
        var p = String(pathValue || "")
        if (p.length <= 0 || !fileManagerApiRef || !fileManagerApiRef.folderShareInfo) {
            return ({
                        "ok": false,
                        "enabled": false
                    })
        }
        var res = fileManagerApiRef.folderShareInfo(p)
        if (!!res && !!res.ok) {
            var next = {}
            var keys = Object.keys(folderShareInfoCache || ({}))
            for (var i = 0; i < keys.length; ++i) {
                next[keys[i]] = folderShareInfoCache[keys[i]]
            }
            next[String(res.path || p)] = res
            folderShareInfoCache = next
        }
        return res || ({
                           "ok": false,
                           "enabled": false
                       })
    }
    function prepareFolderShareDialog(pathValue) {
        var info = folderShareInfoForPath(pathValue)
        if (folderShareDialogRef && folderShareDialogRef.applyFromInfo) {
            folderShareDialogRef.applyFromInfo(info)
        }
    }
    function openFolderShareDialog(pathValue) {
        var p = String(pathValue || "")
        if (p.length <= 0) {
            p = String(contextEntryPath || "")
        }
        if (p.length <= 0 || !folderShareDialogRef || !folderShareDialogRef.openForPath) {
            return
        }
        folderShareDialogRef.openForPath(p)
    }
    function applyFolderShareConfig(pathValue, options) {
        if (!fileManagerApiRef || !fileManagerApiRef.configureFolderShare) {
            return ({
                        "ok": false,
                        "error": "api-unavailable"
                    })
        }
        var res = fileManagerApiRef.configureFolderShare(String(pathValue || ""),
                                                         options || ({}))
        if (!!res && !!res.ok) {
            var cp = String(res.path || pathValue || "")
            if (cp.length > 0) {
                var next = {}
                var keys = Object.keys(folderShareInfoCache || ({}))
                for (var i = 0; i < keys.length; ++i) {
                    next[keys[i]] = folderShareInfoCache[keys[i]]
                }
                next[cp] = res
                folderShareInfoCache = next
                propertiesSharePath = cp
            }
            refreshCurrent()
            notifyResult("Bagikan Folder", {
                             "ok": true,
                             "message": "Folder sekarang dibagikan di jaringan"
                         })
        }
        return res
    }
    function disableFolderShare(pathValue) {
        if (!fileManagerApiRef || !fileManagerApiRef.disableFolderShare) {
            return ({
                        "ok": false,
                        "error": "api-unavailable"
                    })
        }
        var res = fileManagerApiRef.disableFolderShare(String(pathValue || ""))
        if (!!res && !!res.ok) {
            var cp = String(res.path || pathValue || "")
            if (cp.length > 0) {
                var next = {}
                var keys = Object.keys(folderShareInfoCache || ({}))
                for (var i = 0; i < keys.length; ++i) {
                    next[keys[i]] = folderShareInfoCache[keys[i]]
                }
                next[cp] = res
                folderShareInfoCache = next
                propertiesSharePath = cp
            }
            refreshCurrent()
            notifyResult("Bagikan Folder", {
                             "ok": true,
                             "message": "Berbagi folder dihentikan"
                         })
        }
        return res
    }
    function copyFolderShareAddress(pathValue) {
        if (!fileManagerApiRef || !fileManagerApiRef.copyFolderShareAddress) {
            return ({
                        "ok": false,
                        "error": "api-unavailable"
                    })
        }
        var res = fileManagerApiRef.copyFolderShareAddress(String(pathValue || ""))
        notifyResult("Bagikan Folder", res)
        return res
    }
    function folderSharingEnvironment() {
        if (!fileManagerApiRef || !fileManagerApiRef.folderSharingEnvironment) {
            return ({
                        "ok": false,
                        "ready": false,
                        "error": "api-unavailable",
                        "issues": []
                    })
        }
        return fileManagerApiRef.folderSharingEnvironment()
    }
    function repairFolderSharingEnvironment() {
        if (!fileManagerApiRef || !fileManagerApiRef.repairFolderSharingEnvironment) {
            return ({
                        "ok": false,
                        "ready": false,
                        "error": "api-unavailable",
                        "issues": [],
                        "actions": []
                    })
        }
        return fileManagerApiRef.repairFolderSharingEnvironment()
    }
    function installMissingComponent(componentIdValue) {
        if (!fileManagerApiRef || !fileManagerApiRef.installMissingComponent) {
            return ({
                        "ok": false,
                        "error": "api-unavailable"
                    })
        }
        return fileManagerApiRef.installMissingComponent(String(componentIdValue || ""))
    }
    function openContextEntryInApp(appIdValue) { FileManagerOpenWith.openContextEntryInApp(root, fileManagerApiRef, appIdValue) }
    function setDefaultContextEntryApp(appIdValue) { FileManagerOpenWith.setDefaultContextEntryApp(root, fileManagerApiRef, appIdValue) }
    function openWithOtherApplication() { FileManagerOpenWith.openWithOtherApplication(root, openWithDialogRef) }
    function setDefaultWithOtherApplication() { FileManagerOpenWith.setDefaultWithOtherApplication(root, openWithDialogRef) }
    function openInOtherApplication() { FileManagerOpenWith.openInOtherApplication(root, openWithDialogRef) }
    function openContextEntryInFileManager() { FileManagerOps.openContextEntryInFileManager(root, fileManagerApiRef) }
    function closeAllContextMenus() { FileManagerOps.closeAllContextMenus(fileManagerMenusDialogRef, sidebarContextMenuDialogRef) }
    function popupContextEntryMenu(x, y) { FileManagerOps.popupContextEntryMenu(root, fileManagerMenusDialogRef, sidebarContextMenuDialogRef, x, y) }
    function shellSingleQuote(textValue) { return FileManagerPathUtils.shellSingleQuote(textValue) }
    function requestRenameContextEntry() { FileManagerOps.requestRenameContextEntry(root, renameDialogRef) }
    function applyRenameContextEntry() { FileManagerOps.applyRenameContextEntry(root, renameDialogRef) }
    function moveContextEntryToTrash() { FileManagerWindowActions.moveContextEntryToTrash(root, fileManagerApiRef) }
    function selectedEntry() { return FileManagerOps.selectedEntry(root) }
    function selectedPaths() { return FileManagerOps.selectedPaths(root) }
    function selectedHasProtectedPath() { return FileManagerOps.selectedHasProtectedPath(root) }
    function selectedAllDirectories() { return FileManagerOps.selectedAllDirectories(root) }
    function openSelectedEntries() { FileManagerOps.openSelectedEntries(root) }
    function openSelectedInNewTabs() { FileManagerOps.openSelectedInNewTabs(root) }
    function openSelectedInNewWindows() { FileManagerOps.openSelectedInNewWindows(root) }
    function copySelected(cutMode) { FileManagerOps.copySelected(root, cutMode) }
    function pasteIntoCurrent() { FileManagerOps.pasteIntoCurrent(root, fileManagerApiRef) }
    function chooseDestinationForSelection(moveMode) { FileManagerOps.chooseDestinationForSelection(root, fileManagerApiRef, moveMode) }
    function deleteSelected(permanentDelete) { FileManagerOps.deleteSelected(root, fileManagerApiRef, permanentDelete) }
    function restoreSelectedFromTrash() { FileManagerOps.restoreSelectedFromTrash(root, fileManagerApiRef) }
    function restoreAllFromTrash() { FileManagerOps.restoreAllFromTrash(root, fileManagerApiRef) }
    function deleteSelectedOrEmptyTrash() { FileManagerOps.deleteSelectedOrEmptyTrash(root, fileManagerApiRef) }
    function refreshCurrent() { FileManagerOps.refreshCurrent(root) }
    function createNewFolder() { FileManagerOps.createNewFolder(root) }
    function createNewFile() { FileManagerOps.createNewFile(root) }
    function clearRecentFiles() { FileManagerOps.clearRecentFiles(root, fileManagerApiRef) }
    function requestClearRecentFiles() { FileManagerOps.requestClearRecentFiles(clearRecentsDialogRef) }
    function copySelectedPathToClipboard() { FileManagerOps.copySelectedPathToClipboard(root, fileManagerApiRef) }
    function copySelectedAsLinkToClipboard() { FileManagerOps.copySelectedAsLinkToClipboard(root, fileManagerApiRef) }
    function invertSelection() { FileManagerOps.invertSelection(root) }
    function addContextEntryToBookmarks() { FileManagerOps.addContextEntryToBookmarks(root, fileManagerApiRef) }
    function sendSelectionByEmail() { FileManagerOps.sendSelectionByEmail(root, appCommandRouterRef) }
    function canPrintSelection() { return FileManagerOps.canPrintSelection(root) }
    function requestPrintSelection() { FileManagerOps.requestPrintSelection(root) }
    function sendSelectionViaBluetooth() { FileManagerOps.sendSelectionViaBluetooth(root, appCommandRouterRef) }
    function compressSelection() { FileManagerOps.compressSelection(root, fileManagerApiRef, compressDialogRef) }
    function applyCompressSelection() { FileManagerOps.applyCompressSelection(root, fileManagerApiRef, compressDialogRef) }
    function showPropertiesForSelection() { FileManagerOps.showPropertiesForSelection(root, fileManagerApiRef, propertiesDialogRef) }
    function showPropertiesForPath(pathValue) { FileManagerOps.showPropertiesForPath(root, fileManagerApiRef, pathValue, propertiesDialogRef) }
    function isImageSuffix(nameValue) { return FileManagerOps.isImageSuffix(nameValue) }
    function previewSourceForEntry(entry) { return FileManagerOps.previewSourceForEntry(entry) }
    function toggleQuickPreview() { FileManagerOps.toggleQuickPreview(root, quickPreviewDialogRef) }
    function loadStorageSidebarItems() { FileManagerSidebarOps.loadStorageSidebarItems(root, sidebarModel, fileManagerApiRef) }
    function rebuildSidebarItems() { FileManagerSidebarOps.rebuildSidebarItems(root, sidebarModel, fileManagerApiRef) }

    function renameSelected() {
        var entry = selectedEntry()
        if (!entry || !entry.ok) {
            return
        }
        root.contextEntryIndex = root.selectedEntryIndex
        root.contextEntryPath = String(entry.path || "")
        root.contextEntryName = String(entry.name || basename(
                                           root.contextEntryPath))
        root.contextEntryIsDir = !!entry.isDir
        requestRenameContextEntry()
    }

    ListModel {
        id: sidebarModel
    }

    ListModel {
        id: tabModel
    }

    FM.FileManagerApiConnections {
        hostRoot: root
        fileManagerApi: fileManagerApiRef
        connectServerDialogRef: connectServerDialogRef
    }

    FM.FileManagerWindowBody {
        id: windowBody
        anchors.fill: parent
        hostRoot: root
        sidebarModel: sidebarModel
        tabModel: tabModel
        sidebarContextMenuRef: sidebarContextMenuDialogRef
        fileManagerDialogsRef: fileManagerDialogs
    }

    FM.FileManagerDragGhost {
        active: root.dndActive
        copyMode: root.dndCopyMode
        ghostX: root.dndGhostX
        ghostY: root.dndGhostY
        label: root.dndGhostLabel
    }

    FM.FileManagerDialogs {
        id: fileManagerDialogs
        hostRoot: root
    }

    Keys.onPressed: function (event) {
        var headerRef = windowBody ? windowBody.headerBarRef : null
        var mainPaneRef = windowBody ? windowBody.mainPaneRef : null
        var searchFieldRef = (headerRef && headerRef.searchFieldRef) ? headerRef.searchFieldRef : null
        if (FileManagerKeys.handleWindowKey(root, event,
                                            mainPaneRef ? mainPaneRef.contentViewRef : null,
                                            searchFieldRef,
                                            quickPreviewDialogRef)) {
            event.accepted = true
        }
    }

    FM.FileManagerLifecycle {
        hostRoot: root
        fileManagerApi: fileManagerApiRef
        fileManagerModelFactory: modelFactoryRef
        cursorController: cursorControllerRef
        quickTypeClearTimerRef: quickTypeClearTimer
        batchOverlayPopupRef: batchOverlayPopupDialogRef
    }
}
