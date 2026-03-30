import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import Slm_Desktop
import SlmStyle as DSStyle
import "Qml/components" as Components
import "Qml/components/globalmenu" as GlobalMenuComp
import "Qml/components/overlay" as OverlayComp
import "Qml/components/portalchooser" as PortalChooserComp
import "Qml/components/print" as PrintComp
import "Qml/components/screenshot" as ScreenshotComp
import "Qml/components/overlay/LaunchpadActions.js" as LaunchpadActions
import "Qml/apps/filemanager/FileManagerGlobalMenuController.js" as FileManagerGlobalMenuController
import "Qml/components/screenshot/ScreenshotController.js" as ScreenshotController
import "Qml/components/screenshot/ScreenshotSaveController.js" as ScreenshotSaveController
import "Qml/components/shell/ShellUtils.js" as ShellUtils
import "Qml/components/shell/TothespotController.js" as TothespotController

ApplicationWindow {
    id: root
    property bool styleDarkMode: Theme.darkMode
    property bool startWindowed: (typeof AppStartWindowed !== "undefined") ? !!AppStartWindowed : false
    property int startWindowWidthHint: (typeof AppStartWindowWidth !== "undefined") ? Number(AppStartWindowWidth) : 0
    property int startWindowHeightHint: (typeof AppStartWindowHeight !== "undefined") ? Number(AppStartWindowHeight) : 0
    property bool topBarPopupExpanded: false
    property bool motionDebugOverlayEnabled: (typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.getPreference)
                                              ? !!UIPreferences.getPreference("motion.debugOverlay", false)
                                              : false
    property real motionTimeScale: (typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.getPreference)
                                    ? Number(UIPreferences.getPreference("motion.timeScale", 1.0))
                                    : 1.0
    property bool motionReducedEnabled: (typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.getPreference)
                                         ? !!UIPreferences.getPreference("motion.reduced", false)
                                         : false
    property var motionDebugRows: []
    property bool shellContextMenuOpen: false
    property bool fileManagerVisible: false
    property bool detachedFileManagerVisible: false
    property string detachedFileManagerPath: "~"
    property bool detachedFileManagerLoadFailed: false
    property string pendingDetachedFileManagerPropertiesPath: ""
    property var fileManagerContent: null
    property var appModelRef: null
    property var fileManagerApiRef: null
    property var tothespotServiceRef: null
    function openFileManagerFromShortcut(pathValue) {
        var targetPath = String(pathValue && String(pathValue).length > 0 ? pathValue : "~")
        ShellUtils.openDetachedFileManager(root, targetPath)
        Qt.callLater(function() {
            var detachedUnavailable = !detachedFileManagerWindow
                                     || root.detachedFileManagerLoadFailed
                                     || detachedFileManagerWindow.loaderStatus === Loader.Error
            if (!detachedUnavailable) {
                return
            }
            if (root.fileManagerApiRef && root.fileManagerApiRef.startOpenPathInFileManager) {
                root.fileManagerApiRef.startOpenPathInFileManager(targetPath,
                                                                  "shortcut-open-filemanager")
            }
        })
    }
    onDetachedFileManagerVisibleChanged: {
        if (!detachedFileManagerVisible) {
            detachedFileManagerLoadFailed = false
            if (detachedFileManagerWindow) {
                detachedFileManagerWindow.setLoaderActive(false)
                detachedFileManagerWindow.stopWatchdog()
            }
            return
        }
        detachedFileManagerLoadFailed = false
        if (detachedFileManagerWindow) {
            detachedFileManagerWindow.restartWatchdog()
        }
        Qt.callLater(function() {
            if (!detachedFileManagerWindow) {
                return
            }
            detachedFileManagerWindow.setLoaderActive(true)
            var item = detachedFileManagerWindow.openPathIfReady(root.detachedFileManagerPath)
            if (item) {
                root.fileManagerContent = item
            }
        })
    }
    property real shellContextMenuX: 0
    property real shellContextMenuY: 0
    readonly property int shellContextMenuWidth: 190
    readonly property int shellContextMenuHeight: 40
    readonly property int listViewContainMode: ListView.Contain
    readonly property int listViewEndMode: ListView.End
    readonly property int listViewBeginningMode: ListView.Beginning
    property double shellContextMenuOpenedAtMs: 0
    property bool areaShotSelecting: false
    property bool areaShotFromScreenshotTool: false
    property string areaShotOutputPath: ""
    property real areaShotStartX: 0
    property real areaShotStartY: 0
    property real areaShotEndX: 0
    property real areaShotEndY: 0
    property string areaShotBind: (typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.getPreference)
                                  ? String(UIPreferences.getPreference("screenshot.bindArea", "Alt+Shift+S"))
                                  : "Alt+Shift+S"
    property string fullscreenShotBind: (typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.getPreference)
                                        ? String(UIPreferences.getPreference("screenshot.bindFullscreen", "Alt+Shift+F"))
                                        : "Alt+Shift+F"
    property int pendingScreenshotDelaySec: 0
    property string pendingScreenshotMode: "screen"
    property bool screenshotSaveDialogVisible: false
    property string screenshotSaveSourcePath: ""
    property string screenshotSaveName: ""
    property string screenshotSaveFolder: ""
    property int screenshotSaveTypeIndex: 0
    property string screenshotSaveErrorCode: ""
    property string screenshotSaveErrorHint: ""
    property bool screenshotSaveBlockedByNoSpace: false
    property var screenshotOutputTypes: [
        { "label": "PNG image (.png)", "ext": "png", "format": "png", "quality": -1 },
        { "label": "JPEG image (.jpg)", "ext": "jpg", "format": "jpg", "quality": 92 },
        { "label": "BMP image (.bmp)", "ext": "bmp", "format": "bmp", "quality": -1 },
        { "label": "TIFF image (.tiff)", "ext": "tiff", "format": "tiff", "quality": -1 }
    ]
    property string screenshotLastErrorCode: ""
    property string screenshotRequestId: ""
    property bool screenshotOverwriteDialogVisible: false
    property string screenshotOverwriteTargetPath: ""
    property bool printDialogVisible: false
    property string printDocumentUri: ""
    property string printDocumentTitle: "Document"
    property bool printPreferPdfOutput: false
    property bool portalFileChooserVisible: false
    property string portalChooserRequestId: ""
    property var portalChooserOptions: ({})
    property string portalChooserMode: "open"
    property string portalChooserTitle: "Choose File"
    property string portalChooserCurrentDir: "~"
    property string portalChooserName: ""
    property bool portalChooserAllowMultiple: false
    property bool portalChooserSelectFolders: false
    property var portalChooserFilters: []
    property int portalChooserFilterIndex: 0
    property string portalChooserSortKey: "name"
    property bool portalChooserSortDescending: false
    property bool portalChooserShowHidden: false
    property string portalChooserPreviewPath: ""
    property string portalChooserErrorText: ""
    property string portalChooserValidationError: ""
    property real portalChooserDateColumnWidth: 140
    property real portalChooserKindColumnWidth: 92
    property bool portalChooserPathEditMode: false
    property var portalChooserStoragePlaces: []
    property int portalChooserSelectedIndex: -1
    property int portalChooserAnchorIndex: -1
    property var portalChooserSelectedPaths: ({})
    property int portalChooserWindowWidth: 0
    property int portalChooserWindowHeight: 0
    property bool portalChooserOverwriteDialogVisible: false
    property string portalChooserOverwriteTargetPath: ""
    property bool portalChooserOverwriteAlwaysThisSession: false
    property string portalChooserInternalKind: ""
    property var portalChooserInternalPayload: ({})
    property var portalChooserEntriesModelRef: portalChooserEntries
    property var portalChooserPlacesModelRef: portalChooserPlacesModel
    property var portalChooserApiRef: portalChooserApi
    property bool tothespotVisible: false
    property bool clipboardOverlayVisible: false
    property bool lockScreenVisible: false
    property double tothespotLastCloseMs: 0
    property string tothespotQuery: ""
    property string tothespotLastLoadedQuery: ""
    property int tothespotQueryGeneration: 0
    property int tothespotAppliedGeneration: -1
    property int tothespotSelectedIndex: -1
    property var tothespotProfileMeta: ({})
    property var tothespotTelemetryMeta: ({})
    property var tothespotTelemetryLast: ({})
    property var tothespotProviderStats: ({})
    property var tothespotPreviewData: ({})
    property bool tothespotShowDebug: (typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.getPreference)
                                      ? !!UIPreferences.getPreference("debug.verboseLogging", false)
                                      : false
    property bool tothespotNotifyClipboardResolveSuccess: (typeof UIPreferences !== "undefined"
                                                           && UIPreferences
                                                           && UIPreferences.getPreference)
                                                          ? !!UIPreferences.getPreference(
                                                              "tothespot.notifyClipboardResolveSuccess", true)
                                                          : true
    property string tothespotBind: (typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.getPreference)
                                   ? String(UIPreferences.getPreference("tothespot.bindToggle", "Alt+Space"))
                                   : "Alt+Space"
    property int tothespotSavedX: (typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.getPreference)
                                  ? Number(UIPreferences.getPreference("tothespot.windowX", -1)) : -1
    property int tothespotSavedY: (typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.getPreference)
                                  ? Number(UIPreferences.getPreference("tothespot.windowY", -1)) : -1
    property int tothespotSavedWidth: (typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.getPreference)
                                      ? Number(UIPreferences.getPreference("tothespot.windowWidth", -1)) : -1
    property int tothespotSavedHeight: (typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.getPreference)
                                       ? Number(UIPreferences.getPreference("tothespot.windowHeight", -1)) : -1
    property bool tothespotRestoringGeometry: false
    onTothespotVisibleChanged: {
        if (typeof ShellStateController !== "undefined" && ShellStateController) ShellStateController.setToTheSpotVisible(tothespotVisible)
        if (lockScreenVisible && tothespotVisible) {
            tothespotVisible = false
            return
        }
        if (!tothespotVisible) {
            tothespotLastCloseMs = Date.now()
            return
        }
        ShellUtils.restoreTothespotGeometry(root)
        TothespotController.refreshProfileMeta(root)
        TothespotController.refreshTelemetryMeta(root)
        TothespotController.refreshTelemetryLast(root)
        tothespotDebounce.stop()
        TothespotController.refreshResults(root, tothespotResultsModel, true)
    }
    onMotionDebugOverlayEnabledChanged: {
        ShellUtils.refreshMotionDebugRows(root)
    }
    onLockScreenVisibleChanged: {
        if (!lockScreenVisible) {
            return
        }
        tothespotVisible = false
        clipboardOverlayVisible = false
        shellContextMenuOpen = false
        if (desktopScene) {
            desktopScene.launchpadVisible = false
            desktopScene.workspaceVisible = false
            desktopScene.styleGalleryVisible = false
        }
    }
    onMotionReducedEnabledChanged: {
        ShellUtils.applyMotionTimeScale(root)
    }
    visible: true
    width: startWindowed
           ? Math.max(minimumWidth,
                      Math.min(Screen.width,
                               startWindowWidthHint > 0
                               ? startWindowWidthHint
                               : Math.round(Screen.width * 0.85)))
           : Screen.width
    height: startWindowed
            ? Math.max(minimumHeight,
                       Math.min(Screen.height,
                                startWindowHeightHint > 0
                                ? startWindowHeightHint
                                : Math.round(Screen.height * 0.85)))
            : Screen.height
    minimumWidth: 960
    minimumHeight: 600
    color: Theme.color("windowBg")
    flags: Qt.FramelessWindowHint
    title: "Slm Desktop"
    font.family: Theme.fontFamilyUi
    font.pixelSize: Theme.fontSize("body")

    Behavior on color {
        ColorAnimation {
            duration: Theme.transitionDuration
            easing.type: Easing.InOutQuad
        }
    }

    function syncStyleThemeFromPreferences() {
        if (typeof UIPreferences === "undefined" || !UIPreferences) {
            return
        }
        Theme.applyModeString(UIPreferences.themeMode)
        Theme.userAccentColor = UIPreferences.accentColor
        Theme.userFontScale = UIPreferences.fontScale
        DSStyle.Theme.applyModeString(UIPreferences.themeMode)
        DSStyle.Theme.userAccentColor = UIPreferences.accentColor
        DSStyle.Theme.userFontScale = UIPreferences.fontScale
    }

    onPortalChooserSelectedPathsChanged: {
        if (portalChooserApi) {
            portalChooserApi.portalChooserUpdatePreviewPath()
        }
    }

    Component.onCompleted: {
        syncStyleThemeFromPreferences()
        if (typeof AppModel !== "undefined" && AppModel) {
            appModelRef = AppModel
        }
        if (typeof FileManagerApi !== "undefined" && FileManagerApi) {
            fileManagerApiRef = FileManagerApi
        }
        if (typeof TothespotService !== "undefined" && TothespotService) {
            tothespotServiceRef = TothespotService
        }
        ShellUtils.applyUserFontScalePreference(root)
        ShellUtils.applyMotionTimeScale(root)
        ShellUtils.refreshMotionDebugRows(root)
        FileManagerGlobalMenuController.syncOverride(root, detachedFileManagerWindow)
        if (typeof SessionStateClient !== "undefined" && SessionStateClient) {
            root.lockScreenVisible = !!SessionStateClient.locked
        }
    }

    Connections {
        target: typeof UIPreferences !== "undefined" ? UIPreferences : null
        function onThemeModeChanged() { root.syncStyleThemeFromPreferences() }
        function onAccentColorChanged() { root.syncStyleThemeFromPreferences() }
        function onFontScaleChanged() { root.syncStyleThemeFromPreferences() }
    }

    GlobalMenuComp.GlobalMenuActionRouter {
        id: globalMenuActionRouter
        rootWindow: root
        fileManagerContent: root.fileManagerContent
        detachedFileManagerWindow: detachedFileManagerWindow
        onRequestFocusTothespot: TothespotController.focusFromMenu(root, tothespotWindow)
        onRequestHelpMessage: function(message) {
            ScreenshotController.showResultNotification(root, true, "", String(message || ""))
        }
    }

    Shortcut {
        sequence: "Alt+Shift+E"
        context: Qt.ApplicationShortcut
        onActivated: {
            root.openFileManagerFromShortcut("~")
        }
    }

    Shortcut {
        sequence: "Meta+E"
        context: Qt.ApplicationShortcut
        onActivated: {
            root.openFileManagerFromShortcut("~")
        }
    }

    Shortcut {
        sequence: "Alt+Shift+P"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (!root.printDocumentUri.length && root.screenshotSaveSourcePath.length) {
                root.printDocumentUri = "file://" + root.screenshotSaveSourcePath
                root.printDocumentTitle = "Captured Image"
            }
            root.printPreferPdfOutput = false
            root.printDialogVisible = true
        }
    }

    Shortcut {
        sequence: root.tothespotBind
        onActivated: {
            if (root.lockScreenVisible) {
                return
            }
            root.tothespotVisible = !root.tothespotVisible
            if (root.tothespotVisible) {
                root.clipboardOverlayVisible = false
            }
            if (root.tothespotVisible) {
                Qt.callLater(function() {
                    if (tothespotWindow && tothespotWindow.focusSearchField) {
                        tothespotWindow.focusSearchField()
                    }
                })
            }
        }
    }

    Shortcut {
        sequence: "Meta+V"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (root.lockScreenVisible) {
                return
            }
            root.clipboardOverlayVisible = !root.clipboardOverlayVisible
            if (root.clipboardOverlayVisible) {
                root.tothespotVisible = false
            }
        }
    }

    Shortcut {
        sequence: "Meta+,"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (root.lockScreenVisible) {
                return
            }
            if (typeof AppExecutionGate === "undefined" || !AppExecutionGate) {
                return
            }
            var opened = AppExecutionGate.launchDesktopId("slm-settings.desktop",
                                                          "shortcut-meta-comma")
            if (!opened) {
                opened = AppExecutionGate.launchDesktopId("slm-settings",
                                                          "shortcut-meta-comma")
            }
            if (!opened) {
                if (typeof AppBinaryDir !== "undefined"
                        && String(AppBinaryDir || "").length > 0) {
                    var localSettingsBin = String(AppBinaryDir) + "/slm-settings"
                    opened = AppExecutionGate.launchCommand(localSettingsBin, "",
                                                            "shortcut-meta-comma-local")
                }
            }
            if (!opened) {
                AppExecutionGate.launchCommand("slm-settings", "",
                                               "shortcut-meta-comma")
            }
        }
    }

    Shortcut {
        sequence: "Meta+S"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (root.lockScreenVisible) {
                return
            }
            if (desktopScene && desktopScene.toggleWorkspaceOverview) {
                desktopScene.toggleWorkspaceOverview()
            } else if (desktopScene) {
                desktopScene.workspaceVisible = !desktopScene.workspaceVisible
            }
        }
    }

    Shortcut {
        sequence: "Meta+L"
        context: Qt.ApplicationShortcut
        onActivated: {
            root.lockScreenVisible = true
            if (typeof SessionStateClient !== "undefined" && SessionStateClient && SessionStateClient.lock) {
                SessionStateClient.lock()
            }
        }
    }

    Shortcut {
        sequence: "Meta+Left"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (root.lockScreenVisible) {
                return
            }
            if (desktopScene && desktopScene.switchWorkspaceByDelta) {
                desktopScene.switchWorkspaceByDelta(-1)
            }
        }
    }

    Shortcut {
        sequence: "Meta+Right"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (root.lockScreenVisible) {
                return
            }
            if (desktopScene && desktopScene.switchWorkspaceByDelta) {
                desktopScene.switchWorkspaceByDelta(1)
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+Meta+Left"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (root.lockScreenVisible) {
                return
            }
            if (desktopScene && desktopScene.moveFocusedWindowByDelta) {
                desktopScene.moveFocusedWindowByDelta(-1)
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+Meta+Right"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (root.lockScreenVisible) {
                return
            }
            if (desktopScene && desktopScene.moveFocusedWindowByDelta) {
                desktopScene.moveFocusedWindowByDelta(1)
            }
        }
    }

    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        enabled: !!(desktopScene && desktopScene.workspaceVisible) && !root.lockScreenVisible
        onActivated: {
            if (desktopScene && desktopScene.exitWorkspaceOverview) {
                desktopScene.exitWorkspaceOverview()
            } else if (desktopScene) {
                desktopScene.workspaceVisible = false
            }
        }
    }

    Shortcut {
        sequence: "Alt+Shift+M"
        context: Qt.ApplicationShortcut
        onActivated: {
            root.motionDebugOverlayEnabled = !root.motionDebugOverlayEnabled
            ShellUtils.refreshMotionDebugRows(root)
            if (typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.setPreference) {
                UIPreferences.setPreference("motion.debugOverlay", root.motionDebugOverlayEnabled)
            }
        }
    }

    Timer {
        id: tothespotDebounce
        interval: 150
        repeat: false
        running: false
        onTriggered: TothespotController.refreshResults(root, tothespotResultsModel, false)
    }

    Timer {
        id: screenshotDelayTimer
        interval: Math.max(1, root.pendingScreenshotDelaySec) * 1000
        repeat: false
        running: false
        onTriggered: ScreenshotController.performCapture(root, root.pendingScreenshotMode)
    }

    Timer {
        id: motionDebugSampleTimer
        interval: 120
        repeat: true
        running: root.motionDebugOverlayEnabled
        onTriggered: ShellUtils.refreshMotionDebugRows(root)
    }

    OverlayComp.ShellServiceBridge {
        id: shellServiceBridge
        shellApi: root
        portalUiBridge: (typeof PortalUiBridge !== "undefined") ? PortalUiBridge : null
        portalChooserApi: portalChooserApi
        fileManagerApi: (typeof FileManagerApi !== "undefined") ? FileManagerApi : null
        tothespotService: (typeof TothespotService !== "undefined") ? TothespotService : null
        tothespotResultsModel: tothespotResultsModel
        uiPreferences: (typeof UIPreferences !== "undefined") ? UIPreferences : null
    }

    ListModel {
        id: tothespotResultsModel
        dynamicRoles: true
    }

    ListModel {
        id: portalChooserEntries
    }

    ListModel {
        id: portalChooserPlacesModel
    }

    PortalChooserComp.PortalChooserApi {
        id: portalChooserApi
        shellRoot: root
        entriesModel: portalChooserEntries
        placesModel: portalChooserPlacesModel
    }

    OverlayComp.GlobalProgressWindow {
        id: globalProgressWindow
        rootWindow: root
        overlayVisible: false
    }

    DesktopScene {
        id: desktopScene
        anchors.fill: parent
        dockItem: dockWindow.dockItem
    }

    Shortcut {
        sequence: "Print"
        onActivated: {
            ScreenshotSaveController.beginRequest(root, "shortcut-print")
            ScreenshotController.beginAreaSelection(root)
        }
    }

    Shortcut {
        sequence: root.areaShotBind
        onActivated: {
            ScreenshotSaveController.beginRequest(root, "shortcut-area-bind")
            ScreenshotController.beginAreaSelection(root)
        }
    }

    Shortcut {
        sequence: "Shift+Print"
        onActivated: {
            ScreenshotSaveController.beginRequest(root, "shortcut-shift-print")
            if (typeof AppCommandRouter !== "undefined" && AppCommandRouter && AppCommandRouter.route) {
                var result = AppCommandRouter.routeWithResult("screenshot.fullscreen", {}, "shortcut-shift-print")
                var payload = result && result.payload ? result.payload : {}
                ScreenshotController.showResultNotification(root,
                                                            !!(result && result.ok),
                                                            (payload.path || ""),
                                                            (payload.error || result.error || ""))
            }
        }
    }

    Shortcut {
        sequence: root.fullscreenShotBind
        onActivated: {
            ScreenshotSaveController.beginRequest(root, "shortcut-fullscreen-bind")
            if (typeof AppCommandRouter !== "undefined" && AppCommandRouter && AppCommandRouter.route) {
                var result = AppCommandRouter.routeWithResult(
                            "screenshot.fullscreen", {}, "shortcut-fullscreen-alt-shift-f")
                var payload = result && result.payload ? result.payload : {}
                ScreenshotController.showResultNotification(root,
                                                            !!(result && result.ok),
                                                            (payload.path || ""),
                                                            (payload.error || result.error || ""))
            }
        }
    }

    OverlayComp.AreaShotSelectorWindow {
        id: areaShotWindow
        rootWindow: root
        selecting: root.areaShotSelecting
        startX: root.areaShotStartX
        startY: root.areaShotStartY
        endX: root.areaShotEndX
        endY: root.areaShotEndY
        onCancelRequested: ScreenshotController.cancelAreaSelection(root)
        onSelectionChanged: function(x, y) {
            root.areaShotEndX = x
            root.areaShotEndY = y
        }
        onSelectionCommitted: function(x, y) {
            root.areaShotEndX = x
            root.areaShotEndY = y
            ScreenshotController.commitAreaSelection(root)
        }
    }

    ScreenshotComp.ScreenshotSaveDialog {
        id: screenshotSaveWindow
        visible: root.visible && root.screenshotSaveDialogVisible
        parentWindow: root
        sourcePath: root.screenshotSaveSourcePath
        saveName: root.screenshotSaveName
        saveFolderText: String(root.screenshotSaveFolder || ScreenshotSaveController.defaultSaveFolder(root))
        typeIndex: root.screenshotSaveTypeIndex
        errorCode: root.screenshotSaveErrorCode
        errorHint: root.screenshotSaveErrorHint
        saveDisabledReason: root.screenshotSaveBlockedByNoSpace
                            ? "No space left in destination. Choose another folder."
                            : ""
        outputTypes: root.screenshotOutputTypes
        onSaveNameEdited: function(value) {
            root.screenshotSaveName = value
            root.screenshotSaveBlockedByNoSpace = false
            if (String(root.screenshotSaveErrorCode || "") === "no-space") {
                root.screenshotSaveErrorCode = ""
                root.screenshotSaveErrorHint = ""
            }
        }
        onTypeIndexChangedByUser: function(indexValue) {
            root.screenshotSaveTypeIndex = indexValue
            var typeInfo = ScreenshotSaveController.outputTypeAt(root, indexValue)
            var ext = String(typeInfo && typeInfo.ext ? typeInfo.ext : "png")
            if (typeof ScreenshotSaveHelper !== "undefined"
                    && ScreenshotSaveHelper
                    && ScreenshotSaveHelper.ensureFileNameForExtension) {
                root.screenshotSaveName = String(
                            ScreenshotSaveHelper.ensureFileNameForExtension(
                                String(root.screenshotSaveName || ""), ext))
            }
            root.screenshotSaveBlockedByNoSpace = false
            if (String(root.screenshotSaveErrorCode || "") === "no-space") {
                root.screenshotSaveErrorCode = ""
                root.screenshotSaveErrorHint = ""
            }
        }
        saveEnabled: !root.screenshotSaveBlockedByNoSpace
        onChooseFolderRequested: ScreenshotSaveController.chooseSaveFolder(root)
        onCancelRequested: ScreenshotSaveController.cancelSaveDialog(root)
        onSaveRequested: ScreenshotSaveController.submitSaveDialog(root)
    }

    ScreenshotComp.ScreenshotOverwriteDialog {
        id: screenshotOverwriteWindow
        visible: root.visible && root.screenshotOverwriteDialogVisible
        parentWindow: root
        transientParentWindow: screenshotSaveWindow.visible ? screenshotSaveWindow : root
        targetPath: String(root.screenshotOverwriteTargetPath || "")
        alwaysReplaceInSession: root.portalChooserOverwriteAlwaysThisSession
        onAlwaysReplaceToggled: function(checked) {
            root.portalChooserOverwriteAlwaysThisSession = checked
        }
        onCancelRequested: ScreenshotSaveController.cancelOverwrite(root)
        onReplaceRequested: ScreenshotSaveController.confirmOverwrite(root)
    }

    PrintComp.PrintDialog {
        id: printDialogWindow
        visible: root.visible && root.printDialogVisible
        parentWindow: root
        printManager: (typeof PrintManager !== "undefined") ? PrintManager : null
        printSession: (typeof PrintSession !== "undefined") ? PrintSession : null
        printPreviewModel: (typeof PrintPreviewModel !== "undefined") ? PrintPreviewModel : null
        printJobSubmitter: (typeof PrintJobSubmitter !== "undefined") ? PrintJobSubmitter : null
        documentUri: root.printDocumentUri
        documentTitle: root.printDocumentTitle
        preferPdfOutput: root.printPreferPdfOutput
        onCloseRequested: root.printDialogVisible = false
    }

    OverlayComp.PortalFileChooserWindow {
        id: portalFileChooserWindow
        rootWindow: root
        chooserApi: portalChooserApi
        entriesModel: portalChooserEntries
        placesModel: portalChooserPlacesModel
    }

    PortalChooserComp.PortalChooserOverwriteDialog {
        id: portalChooserOverwriteWindow
        visible: root.visible && root.portalChooserOverwriteDialogVisible
        parentWindow: root
        transientParentWindow: portalFileChooserWindow.visible ? portalFileChooserWindow : root
        targetPath: String(root.portalChooserOverwriteTargetPath || "")
        escapeEnabled: root.portalChooserOverwriteDialogVisible
        onEscapeRequested: root.portalChooserOverwriteDialogVisible = false
        onCancelRequested: root.portalChooserOverwriteDialogVisible = false
        onReplaceRequested: {
            root.portalChooserOverwriteDialogVisible = false
            portalChooserApi.finishPortalFileChooser(true, false, "", true)
        }
    }

    OverlayComp.PortalConsentDialogWindow {
        id: portalConsentDialogWindow
        hostRoot: root
        bridge: shellServiceBridge
        visible: root.visible && shellServiceBridge.consentDialogVisible
        requestPath: String(shellServiceBridge.consentRequestPath || "")
        payload: shellServiceBridge.consentPayload
        onAllowOnceRequested: shellServiceBridge.acceptConsentOnce()
        onAllowAlwaysRequested: shellServiceBridge.acceptConsentAlways()
        onDenyRequested: shellServiceBridge.denyConsent()
        onCancelRequested: shellServiceBridge.cancelConsent()
        onOpenSettingsRequested: shellServiceBridge.openConsentSettings()
    }

    OverlayComp.DetachedFileManagerWindow {
        id: detachedFileManagerWindow
        rootWindow: root
        shellApi: root
        panelHeight: desktopScene.panelHeight
        fileModel: (typeof FileManagerModel !== "undefined") ? FileManagerModel : null
        onPrintRequested: function(documentUri, documentTitle, preferPdfOutput) {
            root.printDocumentUri = String(documentUri || "")
            root.printDocumentTitle = String(documentTitle || "Document")
            root.printPreferPdfOutput = !!preferPdfOutput
            root.printDialogVisible = root.printDocumentUri.length > 0
        }
    }

    OverlayComp.TopBarWindow {
        id: topBarWindow
        rootWindow: root
        desktopScene: desktopScene
        shellApi: root
        onLauncherRequested: desktopScene.launchpadVisible = !desktopScene.launchpadVisible
        onStyleGalleryRequested: desktopScene.styleGalleryVisible = !desktopScene.styleGalleryVisible
        onTothespotRequested: {
            if (root.tothespotVisible) {
                root.tothespotVisible = false
                return
            }
            if ((Date.now() - Number(root.tothespotLastCloseMs || 0)) < 220) {
                return
            }
            root.tothespotVisible = true
        }
        onScreenshotCaptureRequested: function(mode, delaySec, grabPointer, concealText) {
            ScreenshotController.startFromTopBar(root, mode, delaySec, grabPointer, concealText)
        }
    }

    OverlayComp.ShellSystemWindows {
        id: shellSystemWindows
        rootWindow: root
        desktopScene: desktopScene
        shellApi: root
    }

    OverlayComp.TopBarPopupStateController {
        id: topBarPopupStateController
        shellContextMenuOpen: root.shellContextMenuOpen
        anyPopupOpen: !!(topBarWindow && topBarWindow.anyPopupOpen)
        onExpandedChanged: root.topBarPopupExpanded = expanded
    }

    OverlayComp.ShellEventBridge {
        id: shellEventBridge
        shellApi: root
        desktopScene: desktopScene
        globalMenuActionRouter: globalMenuActionRouter
        globalMenuManager: GlobalMenuManager
    }

    OverlayComp.LaunchpadWindow {
        id: launchpadWindow
        rootWindow: root
        desktopScene: desktopScene
        appsModel: AppModel
        dockModel: DockModel
        onAppChosen: LaunchpadActions.handleAppChosen(appData)
        onAddToDockRequested: LaunchpadActions.handleAddToDock(appData)
        onAddToDesktopRequested: LaunchpadActions.handleAddToDesktop(appData, desktopScene)
    }

    TothespotDismissWindow {
        id: tothespotDismissWindow
        rootWindow: root
        overlayVisible: root.tothespotVisible
        onDismissRequested: root.tothespotVisible = false
    }

    OverlayComp.ClipboardOverlayWindow {
        id: clipboardOverlayWindow
        rootWindow: root
        panelHeight: desktopScene.panelHeight
        overlayVisible: root.clipboardOverlayVisible && !root.lockScreenVisible
        client: (typeof ClipboardServiceClient !== "undefined") ? ClipboardServiceClient : null
        onCloseRequested: root.clipboardOverlayVisible = false
    }

    TothespotWindow {
        id: tothespotWindow
        rootWindow: root
        panelHeight: desktopScene.panelHeight
        overlayVisible: root.tothespotVisible && !root.lockScreenVisible
        showDebugInfo: root.tothespotShowDebug
        searchProfileMeta: root.tothespotProfileMeta
        telemetryMeta: root.tothespotTelemetryMeta
        telemetryLast: root.tothespotTelemetryLast
        providerStats: root.tothespotProviderStats
        previewData: root.tothespotPreviewData
        queryText: root.tothespotQuery
        resultsModel: tothespotResultsModel
        selectedIndex: root.tothespotSelectedIndex
        onGeometryEdited: ShellUtils.saveTothespotGeometry(root)
        onOpeningRequested: ShellUtils.restoreTothespotGeometry(root)
        onDismissRequested: root.tothespotVisible = false
        onQueryTextChangedRequest: function(text) {
            root.tothespotQuery = String(text || "")
            root.tothespotQueryGeneration = Number(root.tothespotQueryGeneration || 0) + 1
            tothespotDebounce.restart()
        }
        onSelectedIndexChangedByUser: function(indexValue) {
            root.tothespotSelectedIndex = indexValue
            TothespotController.refreshPreview(root, tothespotResultsModel)
        }
        onResultActivated: function(indexValue) {
            TothespotController.activateResult(root, tothespotResultsModel, indexValue)
        }
        onResultContextAction: function(indexValue, action) {
            TothespotController.activateContextAction(root, tothespotResultsModel, indexValue, action)
        }
    }

    OverlayComp.WorkspaceWindow {
        id: workspaceWindow
        rootWindow: root
        desktopScene: desktopScene
    }

    OverlayComp.DockWindow {
        id: dockWindow
        rootWindow: root
        desktopScene: desktopScene
        appsModel: DockModel
    }

    Connections {
        target: (typeof SessionStateClient !== "undefined") ? SessionStateClient : null
        ignoreUnknownSignals: true
        function onLockedChanged() {
            root.lockScreenVisible = !!(SessionStateClient && SessionStateClient.locked)
        }
    }

    Connections {
        target: (typeof AppModel !== "undefined") ? AppModel : null
        ignoreUnknownSignals: true
        function onModelReset() {
            if (!root.tothespotVisible) {
                return
            }
            if (String(root.tothespotQuery || "").trim().length > 0) {
                return
            }
            if (tothespotResultsModel && Number(tothespotResultsModel.count || 0) > 0) {
                return
            }
            TothespotController.refreshResults(root, tothespotResultsModel, true)
        }
    }

    OverlayComp.LockScreenWindow {
        id: lockScreenWindow
        rootWindow: root
        overlayVisible: root.lockScreenVisible
        userName: (typeof SessionStateClient !== "undefined" && SessionStateClient && SessionStateClient.userName)
                  ? String(SessionStateClient.userName)
                  : "User"
        onVisibleChanged: {
            if (visible) {
                unlockErrorCode = ""
            }
        }
        onUnlockRequested: function(password) {
            if (lockScreenWindow.lockoutActive) {
                return
            }
            if (typeof SessionStateClient !== "undefined" && SessionStateClient && SessionStateClient.requestUnlock) {
                if (SessionStateClient.requestUnlock(password)) {
                    root.lockScreenVisible = false
                    lockScreenWindow.lockFailed = false
                    lockScreenWindow.failedAttempts = 0
                    lockScreenWindow.lockoutLevel = 0
                    lockScreenWindow.unlockErrorCode = ""
                } else {
                    lockScreenWindow.lockFailed = true
                    lockScreenWindow.unlockErrorCode = String(SessionStateClient.lastUnlockError || "")
                    var backendRetrySec = Number(SessionStateClient.lastRetryAfterSec || 0)
                    if (backendRetrySec > 0) {
                        lockScreenWindow.lockoutActive = true
                        lockScreenWindow.lockoutDurationSec = backendRetrySec
                        lockScreenWindow.lockoutRemainingSec = backendRetrySec
                        lockScreenWindow.failedAttempts = 0
                        if (lockScreenWindow.lockoutTimer) {
                            lockScreenWindow.lockoutTimer.restart()
                        }
                        if (lockScreenWindow.lockoutTick) {
                            lockScreenWindow.lockoutTick.restart()
                        }
                    } else {
                        lockScreenWindow.failedAttempts = Number(lockScreenWindow.failedAttempts || 0) + 1
                        if (lockScreenWindow.failedAttempts >= 5) {
                            lockScreenWindow.lockoutLevel = Number(lockScreenWindow.lockoutLevel || 0) + 1
                            var lockoutSeconds = 10
                            if (lockScreenWindow.lockoutLevel >= 2) {
                                lockoutSeconds = 30
                            }
                            if (lockScreenWindow.lockoutLevel >= 3) {
                                lockoutSeconds = 60
                            }
                            if (lockScreenWindow.lockoutLevel >= 4) {
                                lockoutSeconds = 120
                            }
                            if (lockScreenWindow.lockoutLevel >= 5) {
                                lockoutSeconds = 300
                            }
                            lockScreenWindow.lockoutActive = true
                            lockScreenWindow.lockoutDurationSec = lockoutSeconds
                            lockScreenWindow.lockoutRemainingSec = lockoutSeconds
                            lockScreenWindow.failedAttempts = 0
                            if (lockScreenWindow.lockoutTimer) {
                                lockScreenWindow.lockoutTimer.restart()
                            }
                            if (lockScreenWindow.lockoutTick) {
                                lockScreenWindow.lockoutTick.restart()
                            }
                        }
                    }
                }
                return
            }
            if (String(password || "").trim().length > 0) {
                root.lockScreenVisible = false
                lockScreenWindow.lockFailed = false
                lockScreenWindow.failedAttempts = 0
                lockScreenWindow.lockoutLevel = 0
                lockScreenWindow.unlockErrorCode = ""
            } else {
                lockScreenWindow.lockFailed = true
                lockScreenWindow.unlockErrorCode = "empty-password"
                lockScreenWindow.failedAttempts = Number(lockScreenWindow.failedAttempts || 0) + 1
            }
        }
    }

    OverlayComp.MotionDebugOverlay {
        id: motionDebugOverlay
        anchors.fill: parent
        enabled: root.motionDebugOverlayEnabled
        rows: root.motionDebugRows
        panelHeight: desktopScene.panelHeight
        frameDt: (typeof MotionController !== "undefined" && MotionController)
                 ? MotionController.lastFrameDt : 0
        timeScale: (typeof MotionController !== "undefined" && MotionController)
                   ? MotionController.timeScale : 1
        droppedFrameCount: (typeof MotionController !== "undefined" && MotionController)
                           ? MotionController.droppedFrameCount : 0
    }
}
