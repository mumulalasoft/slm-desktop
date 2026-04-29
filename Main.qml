import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import Slm_Desktop
import SlmStyle as DSStyle
import "Qml/components" as Components
import "Qml/components/desktop" as DesktopComp
import "Qml/components/globalmenu" as GlobalMenuComp
import "Qml/components/overlay" as OverlayComp
import "Qml/components/portalchooser" as PortalChooserComp
import "Qml/components/print" as PrintComp
import "Qml/components/screenshot" as ScreenshotComp
import "Qml/apps/filemanager/FileManagerGlobalMenuController.js" as FileManagerGlobalMenuController
import "Qml/components/screenshot/ScreenshotController.js" as ScreenshotController
import "Qml/components/screenshot/ScreenshotSaveController.js" as ScreenshotSaveController
import "Qml/components/shell" as ShellComp
import "Qml/components/shell/ShellUtils.js" as ShellUtils
import "Qml/components/shell/PulseController.js" as PulseController

ApplicationWindow {
    id: root
    function closeDetachedFileManagerContextMenusIfOutside(localX, localY) {
        if (!detachedFileManagerVisible || !detachedFileManagerWindow || !fileManagerContent
                || !fileManagerContent.closeAllContextMenus) {
            return
        }
        var gx = Number(root.x || 0) + Number(localX || 0)
        var gy = Number(root.y || 0) + Number(localY || 0)
        var wx = Number(detachedFileManagerWindow.x || 0)
        var wy = Number(detachedFileManagerWindow.y || 0)
        var ww = Number(detachedFileManagerWindow.width || 0)
        var wh = Number(detachedFileManagerWindow.height || 0)
        var inside = gx >= wx && gx < (wx + ww) && gy >= wy && gy < (wy + wh)
        if (!inside) {
            fileManagerContent.closeAllContextMenus()
        }
    }
    function readSetting(path, fallback) {
        if (typeof DesktopSettings !== "undefined" && DesktopSettings
                && DesktopSettings.settingValue) {
            return DesktopSettings.settingValue(path, fallback)
        }
        return fallback
    }
    function writeSetting(path, value) {
        if (typeof DesktopSettings !== "undefined" && DesktopSettings
                && DesktopSettings.setSettingValue) {
            return DesktopSettings.setSettingValue(path, value)
        }
        return false
    }
    property bool styleDarkMode: Theme.darkMode
    property bool _themeSyncPending: false
    readonly property bool startupTraceEnabled: (typeof StartupTraceEnabled !== "undefined") ? !!StartupTraceEnabled : false
    property real startupT0: 0
    property bool startupNonCriticalWindowsReady: false
    property bool startupTopbarBootstrapReady: false
    property bool startupTopbarItemsReady: false
    property bool dockLoadGateOpen: false
    property bool appDeckAttachReady: false
    readonly property bool appDeckLayerShellSupported: (typeof AppDeckLayerShell !== "undefined")
                                                       && !!AppDeckLayerShell
                                                       && !!AppDeckLayerShell.supported
    readonly property bool appDeckDisabled: (typeof DisableAppDeck !== "undefined")
                                           && !!DisableAppDeck
    readonly property bool embeddedAppDeckDisabled: (typeof DisableEmbeddedAppDeck !== "undefined")
                                                   && !!DisableEmbeddedAppDeck
    readonly property bool externalAppDeckWindowAllowed: !appDeckDisabled
                                                        && appDeckLayerShellSupported
    readonly property bool forceKwinTopLevelOverlays: !!readSetting("shell.forceKwinTopLevelOverlays", false)
    readonly property bool nonCriticalTopLevelWindowsAllowed: appDeckLayerShellSupported || forceKwinTopLevelOverlays
    function markStartupTopbarItemsReady() {
        if (!startupTopbarItemsReady) {
            startupTopbarItemsReady = true
            startupQmlMark("main.crownItems.ready")
        }
        if (!dockLoadGateOpen) {
            dockLoadGateOpen = true
            startupQmlMark("main.dockLoadGate.open")
        }
    }
    property bool startWindowed: (typeof AppStartWindowed !== "undefined") ? !!AppStartWindowed : false
    property int startWindowWidthHint: (typeof AppStartWindowWidth !== "undefined") ? Number(AppStartWindowWidth) : 0
    property int startWindowHeightHint: (typeof AppStartWindowHeight !== "undefined") ? Number(AppStartWindowHeight) : 0
    property bool topBarPopupExpanded: false
    property bool motionDebugOverlayEnabled: !!readSetting("motion.debugOverlay", false)
    property real motionTimeScale: Number(readSetting("motion.timeScale", 1.0))
    property bool motionReducedEnabled: !!readSetting("motion.reduced", false)
    property var motionDebugRows: []
    property bool shellContextMenuOpen: false
    property bool fileManagerVisible: false
    property bool detachedFileManagerVisible: false
    property bool detachedFileManagerPreloadRequested: false
    readonly property bool detachedFileManagerPreloadEnabled: false
    property string detachedFileManagerPath: "~"
    property bool detachedFileManagerLoadFailed: false
    property string pendingDetachedFileManagerPropertiesPath: ""
    property string pendingDetachedFileManagerRenamePath: ""
    property string detachedFileManagerFallbackPath: ""
    property var fileManagerContent: null
    property var desktopMenuProviderRef: null
    property var appModelRef: null
    property var fileManagerApiRef: null
    property var pulseServiceRef: null
    function openFileManagerFromShortcut(pathValue) {
        var targetPath = String(pathValue && String(pathValue).length > 0 ? pathValue : "~")
        ShellUtils.openDetachedFileManager(root, targetPath)
        root.detachedFileManagerFallbackPath = targetPath
        detachedFileManagerFallbackTimer.restart()
    }
    function _syncDesktopMenuOverride() {
        if (!desktopMenuProviderRef || !desktopMenuProviderRef.syncGlobalMenuOverride) {
            return
        }
        var activeApp = (typeof GlobalMenuManager !== "undefined" && GlobalMenuManager)
                ? String(GlobalMenuManager.activeAppId || "") : ""
        desktopMenuProviderRef.syncGlobalMenuOverride(activeApp.length <= 0)
    }
    onDetachedFileManagerVisibleChanged: {
        if (!detachedFileManagerVisible) {
            detachedFileManagerLoadFailed = false
            pendingDetachedFileManagerRenamePath = ""
            detachedFileManagerFallbackPath = ""
            detachedFileManagerFallbackTimer.stop()
            if (detachedFileManagerWindow) {
                detachedFileManagerWindow.setLoaderActive(false)
                detachedFileManagerWindow.stopWatchdog()
            }
            return
        }
        detachedFileManagerLoadFailed = false
        detachedFileManagerPreloadRequested = true
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
    TapHandler {
        id: detachedFileManagerOutsideClickCloser
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        onTapped: function(eventPoint, button) {
            root.closeDetachedFileManagerContextMenusIfOutside(eventPoint.position.x,
                                                               eventPoint.position.y)
        }
    }
    Timer {
        id: detachedFileManagerFallbackTimer
        interval: 1500
        repeat: false
        onTriggered: {
            var targetPath = String(root.detachedFileManagerFallbackPath || "")
            if (targetPath.length <= 0) {
                return
            }
            var detachedUnavailable = !detachedFileManagerWindow
                                     || root.detachedFileManagerLoadFailed
                                     || detachedFileManagerWindow.loaderStatus === Loader.Error
            if (!detachedUnavailable) {
                root.detachedFileManagerFallbackPath = ""
                return
            }
            if (root.fileManagerApiRef && root.fileManagerApiRef.startOpenPathInFileManager) {
                root.fileManagerApiRef.startOpenPathInFileManager(targetPath,
                                                                  "shortcut-open-filemanager")
            }
            root.detachedFileManagerFallbackPath = ""
        }
    }
    Timer {
        id: detachedFileManagerPreloadTimer
        interval: 1800
        repeat: false
        running: root.detachedFileManagerPreloadEnabled
        onTriggered: {
            root.detachedFileManagerPreloadRequested = true
            if (root.detachedFileManagerWindow) {
                root.detachedFileManagerWindow.setLoaderActive(true)
            }
        }
    }
    property bool areaShotSelecting: false
    property bool areaShotFromScreenshotTool: false
    property string areaShotOutputPath: ""
    property real areaShotStartX: 0
    property real areaShotStartY: 0
    property real areaShotEndX: 0
    property real areaShotEndY: 0
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
    property bool _searchVisibleLocal: false
    readonly property bool searchVisible: (typeof ShellStateController !== "undefined" && ShellStateController)
                                          ? !!ShellStateController.searchVisible
                                          : _searchVisibleLocal
    // Compatibility alias while older callsites still use pulse naming.
    readonly property bool pulseVisible: searchVisible
    property bool clipboardOverlayVisible: false
    property bool lockScreenVisible: false
    property double pulseLastCloseMs: 0
    property string _searchQueryLocal: ""
    readonly property string searchQuery: (typeof ShellStateController !== "undefined" && ShellStateController)
                                          ? String(ShellStateController.searchQuery || "")
                                          : _searchQueryLocal
    // Compatibility alias while older callsites still use pulse naming.
    readonly property string pulseQuery: searchQuery
    property string pulseLastLoadedQuery: ""
    property int pulseQueryGeneration: 0
    property int pulseAppliedGeneration: -1
    property int pulseSelectedIndex: -1
    property var pulseProfileMeta: ({})
    property var pulseTelemetryMeta: ({})
    property var pulseTelemetryLast: ({})
    property var pulseProviderStats: ({})
    property var pulsePreviewData: ({})
    property bool pulseShowDebug: !!readSetting("debug.verboseLogging", false)
    property bool pulseNotifyClipboardResolveSuccess: !!readSetting("pulse.notifyClipboardResolveSuccess", true)
    property string pulseBind: String(readSetting("pulse.bindToggle", "Alt+Space"))
    property int pulseSavedX: Number(readSetting("pulse.windowX", -1))
    property int pulseSavedY: Number(readSetting("pulse.windowY", -1))
    property int pulseSavedWidth: Number(readSetting("pulse.windowWidth", -1))
    property int pulseSavedHeight: Number(readSetting("pulse.windowHeight", -1))
    property bool pulseRestoringGeometry: false
    function setSearchVisible(visible) {
        var v = !!visible
        if (v) {
            if (typeof ShellStateController !== "undefined" && ShellStateController
                    && ShellStateController.setAppHubVisible) {
                ShellStateController.setAppHubVisible(false)
            } else if (desktopScene && desktopScene.setAppHubVisible) {
                desktopScene.setAppHubVisible(false)
            }
        }
        if (typeof ShellStateController !== "undefined" && ShellStateController
                && ShellStateController.setPulseVisible) {
            ShellStateController.setPulseVisible(v)
        } else {
            _searchVisibleLocal = v
        }
    }
    function setSearchQuery(query) {
        var text = String(query || "")
        if (typeof ShellStateController !== "undefined" && ShellStateController
                && ShellStateController.setSearchQuery) {
            ShellStateController.setSearchQuery(text)
        } else {
            _searchQueryLocal = text
        }
    }
    onSearchVisibleChanged: {
        if (lockScreenVisible && searchVisible) {
            setSearchVisible(false)
            return
        }
        if (!searchVisible) {
            pulseLastCloseMs = Date.now()
            return
        }
        ShellUtils.restorePulseGeometry(root)
        PulseController.refreshProfileMeta(root)
        PulseController.refreshTelemetryMeta(root)
        PulseController.refreshTelemetryLast(root)
        pulseDebounce.stop()
        PulseController.refreshResults(root, pulseResultsStore, true)
    }
    onSearchQueryChanged: {
        // Keep Pulse results refresh resilient regardless of which UI surface
        // updates the query (context input, shortcut, or programmatic set).
        if (!searchVisible) {
            return
        }
        pulseDebounce.restart()
    }
    onMotionDebugOverlayEnabledChanged: {
        ShellUtils.refreshMotionDebugRows(root)
    }
    onLockScreenVisibleChanged: {
        if (typeof ShellStateController !== "undefined" && ShellStateController) ShellStateController.setLockScreenActive(lockScreenVisible)
        if (!lockScreenVisible) {
            return
        }
        setSearchVisible(false)
        clipboardOverlayVisible = false
        shellContextMenuOpen = false
        if (desktopScene) {
            if (desktopScene.setAppHubVisible) {
                desktopScene.setAppHubVisible(false)
            }
            desktopScene.workspaceVisible = false
            desktopScene.styleGalleryVisible = false
        }
    }
    onMotionReducedEnabledChanged: {
        ShellUtils.applyMotionTimeScale(root)
    }
    visible: true
    onVisibleChanged: {
        if (visible) {
            // Intentionally quiet in runtime; startup diagnostics can be enabled from C++.
            if (topBarWindow && topBarWindow.startupItemsReady && !dockLoadGateOpen) {
                dockLoadGateOpen = true
                startupQmlMark("main.dockLoadGate.open.visible")
            }
        }
    }
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
        if (typeof DesktopSettings === "undefined" || !DesktopSettings) {
            return
        }
        Theme.applyModeString(String(DesktopSettings.themeMode || "system"))
        Theme.userAccentColor = String(DesktopSettings.accentColor || "")
        Theme.userFontScale = Number(DesktopSettings.fontScale || 1.0)
        DSStyle.Theme.applyModeString(String(DesktopSettings.themeMode || "system"))
        DSStyle.Theme.userAccentColor = String(DesktopSettings.accentColor || "")
        DSStyle.Theme.userFontScale = Number(DesktopSettings.fontScale || 1.0)
    }

    function requestStyleThemeSync() {
        if (_themeSyncPending) {
            return
        }
        _themeSyncPending = true
        Qt.callLater(function() {
            _themeSyncPending = false
            syncStyleThemeFromPreferences()
        })
    }
    // Phase-budget map (ms from startupT0). Exceeding these logs a regression warning.
    // Only covers phases that are emitted through Main.qml's startupQmlMark.
    readonly property var startupPhaseBudgets: ({
        "main.deferredInit.begin":      50,
        "main.crownBootstrap.ready":   400,
        "main.dockLoader.activated":    300,
        "main.deferredInit.end":        200,
        "main.nonCriticalWindows.ready": 2000
    })

    function startupQmlMark(phase, detail) {
        var now = Date.now()
        if (phase === "main.onCompleted.begin" && startupT0 <= 0) {
            startupT0 = now
        }
        if (!startupTraceEnabled)
            return
        var elapsed = (startupT0 > 0) ? (now - startupT0) : -1
        var text = "[startup-qml] phase=" + String(phase || "")
        if (detail !== undefined && detail !== null && String(detail).length > 0) {
            text += " detail=" + String(detail)
        }
        if (elapsed >= 0) {
            text += " elapsed=" + elapsed + "ms"
        }
        text += " t=" + now
        console.warn(text)
        if (elapsed >= 0 && startupPhaseBudgets[phase] !== undefined) {
            var budget = Number(startupPhaseBudgets[phase] || 0)
            if (elapsed > budget) {
                console.warn("[startup-qml] BUDGET-WARN phase=" + String(phase || "")
                             + " elapsed=" + elapsed + "ms budget=" + budget + "ms"
                             + " over=" + (elapsed - budget) + "ms")
            }
        }
    }

    function sendOverlayCommand(cmd) {
        if (typeof WindowingBackend === "undefined" || !WindowingBackend || !WindowingBackend.sendCommand) {
            return false
        }
        return !!WindowingBackend.sendCommand(String(cmd || ""))
    }

    onPortalChooserSelectedPathsChanged: {
        if (portalChooserApi) {
            portalChooserApi.portalChooserUpdatePreviewPath()
        }
    }

    Component.onCompleted: {
        startupQmlMark("main.onCompleted.begin")
        root.sendOverlayCommand("overlay register shell slm-shell-main")
        requestStyleThemeSync()
        Qt.callLater(function() {
            startupQmlMark("main.deferredInit.begin")
            // 5-minute threshold — 30 s default is too short for normal apphub browsing.
            if (typeof ShellLayerWatchdog !== "undefined" && ShellLayerWatchdog) {
                ShellLayerWatchdog.overlayStuckThresholdMs = 300000
            }
            if (typeof AppModel !== "undefined" && AppModel) {
                appModelRef = AppModel
            }
            if (typeof FileManagerApi !== "undefined" && FileManagerApi) {
                fileManagerApiRef = FileManagerApi
            }
            if (typeof PulseService !== "undefined" && PulseService) {
                pulseServiceRef = PulseService
            }
            root.desktopMenuProviderRef = desktopMenuProvider
            ShellUtils.applyUserFontScalePreference(root)
            ShellUtils.applyMotionTimeScale(root)
            ShellUtils.refreshMotionDebugRows(root)
            FileManagerGlobalMenuController.syncOverride(root, detachedFileManagerWindow)
            root._syncDesktopMenuOverride()
            if (typeof SessionStateClient !== "undefined" && SessionStateClient) {
                root.lockScreenVisible = !!SessionStateClient.locked
            }
            startupTopbarBootstrapTimer.restart()
            if (root.nonCriticalTopLevelWindowsAllowed) {
                startupNonCriticalWindowsTimer.restart()
            } else {
                console.info("[shell] non-critical top-level windows disabled for KWin runtime")
            }
            startupQmlMark("main.deferredInit.end")
        })
        startupQmlMark("main.onCompleted.end")
    }

    Component.onDestruction: {
        root.sendOverlayCommand("overlay unregister shell")
    }

    Timer {
        id: startupTopbarBootstrapTimer
        interval: 250
        repeat: false
        onTriggered: {
            root.startupTopbarBootstrapReady = true
            root.startupQmlMark("main.crownBootstrap.ready")
        }
    }

    Timer {
        id: startupNonCriticalWindowsTimer
        interval: 1500
        repeat: false
        onTriggered: {
            root.startupNonCriticalWindowsReady = true
            root.startupQmlMark("main.nonCriticalWindows.ready")
        }
    }

    Timer {
        id: appDeckAttachTimer
        interval: 300
        repeat: false
        running: root.dockLoadGateOpen
                 && root.startupTopbarItemsReady
                 && root.appDeckLayerShellSupported
                 && !root.appDeckAttachReady
        onTriggered: {
            root.appDeckAttachReady = true
            root.startupQmlMark("main.appdeck.attach.ready")
        }
    }

    Connections {
        target: typeof DesktopSettings !== "undefined" ? DesktopSettings : null
        function onThemeModeChanged() { root.requestStyleThemeSync() }
        function onAccentColorChanged() { root.requestStyleThemeSync() }
        function onFontScaleChanged() { root.requestStyleThemeSync() }
        function onSettingChanged(path) {
            var p = String(path || "")
            if (p.indexOf("globalAppearance.") === 0 || p.indexOf("globalAppearance/") === 0) {
                root.requestStyleThemeSync()
            }
        }
        function onAnimationModeChanged() {
            // Re-sync motion controller scale when animation mode changes at runtime.
            ShellUtils.applyMotionTimeScale(root)
        }
    }

    Connections {
        target: (typeof GlobalMenuManager !== "undefined") ? GlobalMenuManager : null
        ignoreUnknownSignals: true
        function onAppSwitched() {
            root._syncDesktopMenuOverride()
        }
        function onChanged() {
            root._syncDesktopMenuOverride()
        }
    }

    DesktopComp.DesktopViewController {
        id: desktopViewController
    }

    DesktopComp.DesktopMenuProvider {
        id: desktopMenuProvider
        desktopSurface: desktopScene ? desktopScene.desktopFileManagerContent : null
        selection: desktopViewController.selection
        menuContext: desktopViewController.menuContext
        path: desktopViewController.path
    }

    onDesktopMenuProviderRefChanged: _syncDesktopMenuOverride()

    GlobalMenuComp.GlobalMenuActionRouter {
        id: globalMenuActionRouter
        rootWindow: root
        fileManagerContent: root.fileManagerContent
        detachedFileManagerWindow: detachedFileManagerWindow
        onRequestFocusPulse: PulseController.focusFromMenu(root, dockWindowLoader ? dockWindowLoader.item : null)
        onRequestHelpMessage: function(message) {
            if (typeof NotificationManager !== "undefined" && NotificationManager && NotificationManager.Notify) {
                NotificationManager.Notify("Slm Desktop",
                                           0,
                                           "dialog-information-symbolic",
                                           "Global Menu",
                                           String(message || ""),
                                           [],
                                           { "source": "global-menu" },
                                           2600)
            }
        }
        onRequestMenuNotification: function(ok, summary, body, iconName) {
            if (typeof NotificationManager !== "undefined" && NotificationManager && NotificationManager.Notify) {
                NotificationManager.Notify("Slm Desktop",
                                           0,
                                           String(iconName || (ok ? "dialog-information-symbolic"
                                                                  : "dialog-error-symbolic")),
                                           String(summary || "Global Menu"),
                                           String(body || ""),
                                           [],
                                           {
                                               "source": "global-menu",
                                               "ok": !!ok
                                           },
                                           2600)
            }
        }
    }

    Shortcut {
        sequence: "Alt+Shift+E"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (typeof ShellInputRouter !== "undefined" && !ShellInputRouter.canDispatch("shell.file_manager")) return
            root.openFileManagerFromShortcut("~")
        }
    }

    Shortcut {
        sequence: "Meta+E"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (typeof ShellInputRouter !== "undefined" && !ShellInputRouter.canDispatch("shell.file_manager")) return
            root.openFileManagerFromShortcut("~")
        }
    }

    Shortcut {
        sequence: "Alt+Shift+P"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (typeof ShellInputRouter !== "undefined" && !ShellInputRouter.canDispatch("shell.print")) return
            if (!root.printDocumentUri.length && root.screenshotSaveSourcePath.length) {
                root.printDocumentUri = "file://" + root.screenshotSaveSourcePath
                root.printDocumentTitle = "Captured Image"
            }
            root.printPreferPdfOutput = false
            root.printDialogVisible = true
        }
    }

    Shortcut {
        sequence: root.pulseBind
        onActivated: {
            if (typeof ShellInputRouter !== "undefined" && !ShellInputRouter.canDispatch("shell.pulse")) return
            root.setSearchVisible(!root.pulseVisible)
            if (root.pulseVisible) {
                root.clipboardOverlayVisible = false
            }
            if (root.pulseVisible) {
                Qt.callLater(function() {
                    const tw = dockWindowLoader ? dockWindowLoader.item : null
                    if (tw && tw.focusSearchField) {
                        tw.focusSearchField()
                    }
                })
            }
        }
    }

    Shortcut {
        sequence: "Meta+V"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (typeof ShellInputRouter !== "undefined" && !ShellInputRouter.canDispatch("shell.clipboard")) return
            root.clipboardOverlayVisible = !root.clipboardOverlayVisible
            if (root.clipboardOverlayVisible) {
                root.setSearchVisible(false)
            }
        }
    }

    Shortcut {
        sequence: "Meta+,"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (typeof ShellInputRouter !== "undefined" && !ShellInputRouter.canDispatch("shell.settings")) return
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
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        enabled: !!(desktopScene && desktopScene.workspaceVisible)
                 && (typeof ShellInputRouter === "undefined" || ShellInputRouter.canDispatch("overlay.dismiss"))
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
            root.writeSetting("motion.debugOverlay", root.motionDebugOverlayEnabled)
        }
    }

    Timer {
        id: pulseDebounce
        interval: 150
        repeat: false
        running: false
        onTriggered: PulseController.refreshResults(root, pulseResultsStore, false)
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
        pulseService: (typeof PulseService !== "undefined") ? PulseService : null
        pulseResultsModel: pulseResultsStore
        desktopSettings: (typeof DesktopSettings !== "undefined") ? DesktopSettings : null
    }

    ListModel {
        id: pulseResultsStore
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

    Loader {
        id: globalProgressWindowLoader
        active: false
        asynchronous: false
        sourceComponent: Component {
            OverlayComp.GlobalProgressWindow {
                rootWindow: root
                overlayVisible: false
            }
        }
    }

    DesktopScene {
        id: desktopScene
        anchors.fill: parent
        dockItem: (dockWindowLoader && dockWindowLoader.item) ? dockWindowLoader.item.dockItem : null
        pulseResultsModel: pulseResultsStore
        embeddedAppDeckEnabled: !root.appDeckDisabled
                                && !root.externalAppDeckWindowAllowed
                                && !root.embeddedAppDeckDisabled
        embeddedAppDeckSafeRendering: !root.appDeckLayerShellSupported
        shellApi: root
        desktopViewController: desktopViewController
        desktopMenuProvider: desktopMenuProvider
    }

    ShellComp.MotionMonitor { }

    ShellComp.GlobalShortcutManager {
        shellRoot: root
        topBarWindowRef: topBarWindow
        desktopSceneRef: desktopScene
    }

    Loader {
        id: areaShotWindowLoader
        active: !!root.areaShotSelecting
        asynchronous: false
        sourceComponent: Component {
            OverlayComp.AreaShotSelectorWindow {
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
        }
    }

    Loader {
        id: screenshotSaveWindowLoader
        active: !!root.screenshotSaveDialogVisible
        asynchronous: false
        sourceComponent: Component {
            ScreenshotComp.ScreenshotSaveDialog {
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
        }
    }

    Loader {
        id: screenshotOverwriteWindowLoader
        active: !!root.screenshotOverwriteDialogVisible
        asynchronous: false
        sourceComponent: Component {
            ScreenshotComp.ScreenshotOverwriteDialog {
                visible: root.visible && root.screenshotOverwriteDialogVisible
                parentWindow: root
                transientParentWindow: (screenshotSaveWindowLoader.item
                                        && screenshotSaveWindowLoader.item.visible)
                                       ? screenshotSaveWindowLoader.item
                                       : root
                targetPath: String(root.screenshotOverwriteTargetPath || "")
                alwaysReplaceInSession: root.portalChooserOverwriteAlwaysThisSession
                onAlwaysReplaceToggled: function(checked) {
                    root.portalChooserOverwriteAlwaysThisSession = checked
                }
                onCancelRequested: ScreenshotSaveController.cancelOverwrite(root)
                onReplaceRequested: ScreenshotSaveController.confirmOverwrite(root)
            }
        }
    }

    Loader {
        id: printDialogWindowLoader
        active: !!root.printDialogVisible
        asynchronous: false
        sourceComponent: Component {
            PrintComp.PrintDialog {
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
        }
    }

    Loader {
        id: portalFileChooserWindowLoader
        active: !!root.portalFileChooserVisible
        asynchronous: false
        sourceComponent: Component {
            OverlayComp.PortalFileChooserWindow {
                rootWindow: root
                chooserApi: portalChooserApi
                entriesModel: portalChooserEntries
                placesModel: portalChooserPlacesModel
            }
        }
    }

    Loader {
        id: portalChooserOverwriteWindowLoader
        active: !!root.portalChooserOverwriteDialogVisible
        asynchronous: false
        sourceComponent: Component {
            PortalChooserComp.PortalChooserOverwriteDialog {
                visible: root.visible && root.portalChooserOverwriteDialogVisible
                parentWindow: root
                transientParentWindow: (portalFileChooserWindowLoader.item
                                        && portalFileChooserWindowLoader.item.visible)
                                       ? portalFileChooserWindowLoader.item
                                       : root
                targetPath: String(root.portalChooserOverwriteTargetPath || "")
                escapeEnabled: root.portalChooserOverwriteDialogVisible
                onEscapeRequested: root.portalChooserOverwriteDialogVisible = false
                onCancelRequested: root.portalChooserOverwriteDialogVisible = false
                onReplaceRequested: {
                    root.portalChooserOverwriteDialogVisible = false
                    portalChooserApi.finishPortalFileChooser(true, false, "", true)
                }
            }
        }
    }

    Loader {
        id: portalConsentDialogWindowLoader
        active: root.visible && shellServiceBridge.consentDialogVisible
        asynchronous: false
        sourceComponent: Component {
            OverlayComp.PortalConsentDialogWindow {
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
        }
    }

    Loader {
        id: firewallPromptDialogLoader
        active: !!root.startupNonCriticalWindowsReady
        asynchronous: false
        sourceComponent: Component {
            OverlayComp.FirewallPromptDialog {
                hostRoot: root
            }
        }
    }

    Loader {
        id: detachedFileManagerWindowLoader
        active: !!root.detachedFileManagerVisible || !!root.detachedFileManagerPreloadRequested
        asynchronous: false
        sourceComponent: Component {
            OverlayComp.DetachedFileManagerWindow {
                rootWindow: root
                shellApi: root
                panelHeight: desktopScene.panelHeight
                fileModel: (typeof FileManagerModel !== "undefined") ? FileManagerModel : null
                preloadRequested: !!root.detachedFileManagerPreloadRequested
                onPrintRequested: function(documentUri, documentTitle, preferPdfOutput) {
                    root.printDocumentUri = String(documentUri || "")
                    root.printDocumentTitle = String(documentTitle || "Document")
                    root.printPreferPdfOutput = !!preferPdfOutput
                    root.printDialogVisible = root.printDocumentUri.length > 0
                }
            }
        }
    }
    readonly property var detachedFileManagerWindow: detachedFileManagerWindowLoader.item

    OverlayComp.CrownWindow {
        id: topBarWindow
        rootWindow: root
        desktopScene: desktopScene
        shellApi: root
        desktopMenuProvider: desktopMenuProvider
        onStartupItemsReadyReached: root.markStartupTopbarItemsReady()
        onStartupItemsReadyChanged: {
            if (startupItemsReady) {
                root.markStartupTopbarItemsReady()
            }
        }
        onLauncherRequested: {
            if (desktopScene && desktopScene.setAppHubVisible) {
                desktopScene.setAppHubVisible(!desktopScene.apphubVisible)
            }
        }
        onPulseRequested: {
            if (root.pulseVisible) {
                root.setSearchVisible(false)
                return
            }
            if ((Date.now() - Number(root.pulseLastCloseMs || 0)) < 220) {
                return
            }
            root.setSearchVisible(true)
        }
        onScreenshotCaptureRequested: function(mode, delaySec, grabPointer, concealText) {
            ScreenshotController.startFromCrown(root, mode, delaySec, grabPointer, concealText)
        }
    }

    OverlayComp.ShellSystemWindows {
        id: shellSystemWindows
        rootWindow: root
        desktopScene: desktopScene
        shellApi: root
    }

    OverlayComp.CrownPopupStateController {
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
        desktopMenuProvider: desktopMenuProvider
    }

    // AppDeckWindow — single persistent AppDeck surface (wlr-layer-shell).
    // Deferred out of critical startup path to prioritize first desktop frame.
    Loader {
        id: dockWindowLoader
        active: !!root.appDeckAttachReady
                && !root.appDeckDisabled
        asynchronous: false
        sourceComponent: Component {
            OverlayComp.AppDeckWindow {
                rootWindow: root
                desktopScene: desktopScene
                appsModel: AppDeckModel
                pulseResultsModel: pulseResultsStore
            }
        }
        onActiveChanged: {
            if (active) {
                root.startupQmlMark("main.dockLoader.activated")
            }
        }
    }

    Connections {
        target: desktopScene
        ignoreUnknownSignals: true
        function onAppHubVisibleChanged() {
            void dockWindowLoader
        }
    }

    Loader {
        id: clipboardOverlayWindowLoader
        active: !!root.clipboardOverlayVisible && !root.lockScreenVisible
        asynchronous: false
        sourceComponent: Component {
            OverlayComp.ClipboardOverlayWindow {
                rootWindow: root
                panelHeight: desktopScene.panelHeight
                overlayVisible: root.clipboardOverlayVisible && !root.lockScreenVisible
                client: (typeof ClipboardServiceClient !== "undefined") ? ClipboardServiceClient : null
                onCloseRequested: root.clipboardOverlayVisible = false
            }
        }
    }

    // WorkspaceWindow — deferred Loader with error boundary.
    Loader {
        id: workspaceWindowLoader
        active: !!root.visible
                && !!desktopScene
                && !!desktopScene.workspaceVisible
        asynchronous: false
        sourceComponent: Component {
            OverlayComp.WorkspaceWindow {
                rootWindow: root
                desktopScene: desktopScene
            }
        }
        onStatusChanged: {
            if (status === Loader.Error) {
                console.error("[shell] WorkspaceWindow failed to load:", errorString())
                if (typeof ShellStateController !== "undefined" && ShellStateController) {
                    ShellStateController.setWorkspaceOverviewVisible(false)
                }
                if (typeof ShellLayerWatchdog !== "undefined" && ShellLayerWatchdog) {
                    ShellLayerWatchdog.reportOverlayLoadError("workspace")
                }
            }
        }
    }

    // Persistent-layer health check.
    Connections {
        target: (typeof ShellLayerWatchdog !== "undefined") ? ShellLayerWatchdog : null
        ignoreUnknownSignals: true
        function onPersistentLayerRestored() {
            if (topBarWindow && !topBarWindow.visible) {
                console.warn("[shell] persistent-layer restore: re-showing Crown")
                topBarWindow.visible = Qt.binding(function() { return !!root && root.visible })
            }
        }
        function onOverlayStuckDetected(overlayName, durationMs) {
            // Fires exactly once per occurrence (C++ sets stuckReported after first emit).
            console.warn("[shell] overlay stuck:", overlayName, "visible for", durationMs, "ms — recovery pending")
        }
        function onHealthCheckCompleted(healthy) {
            // Intentionally silent: onOverlayStuckDetected already logged the incident.
            void healthy
        }
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
            if (!root.pulseVisible) {
                return
            }
            if (String(root.pulseQuery || "").trim().length > 0) {
                return
            }
            if (pulseResultsStore && Number(pulseResultsStore.count || 0) > 0) {
                return
            }
            PulseController.refreshResults(root, pulseResultsStore, true)
        }
    }

    Loader {
        id: lockScreenWindowLoader
        active: !!root.lockScreenVisible
                || !!(typeof SessionStateClient !== "undefined" && SessionStateClient && SessionStateClient.locked)
        asynchronous: false
        sourceComponent: Component {
            OverlayComp.LockScreenWindow {
                id: lockScreenWindow
                rootWindow: root
                overlayVisible: root.lockScreenVisible
                userName: (typeof SessionStateClient !== "undefined" && SessionStateClient && SessionStateClient.userName)
                          ? String(SessionStateClient.userName)
                          : "User"
                onVisibleChanged: {
                    if (!visible) {
                        unlockBusy = false
                    } else {
                        unlockErrorCode = ""
                    }
                }
                onUnlockRequested: function(password) {
                    if (lockScreenWindow.lockoutActive || lockScreenWindow.unlockBusy) {
                        return
                    }
                    lockScreenWindow.unlockBusy = true
                    var pw = password
                    Qt.callLater(function() {
                        if (typeof SessionStateClient === "undefined" || !SessionStateClient
                                || !SessionStateClient.requestUnlock) {
                            lockScreenWindow.unlockBusy = false
                            if (String(pw || "").trim().length > 0) {
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
                            return
                        }
                        if (SessionStateClient.requestUnlock(pw)) {
                            lockScreenWindow.unlockBusy = false
                            root.lockScreenVisible = false
                            lockScreenWindow.lockFailed = false
                            lockScreenWindow.failedAttempts = 0
                            lockScreenWindow.lockoutLevel = 0
                            lockScreenWindow.unlockErrorCode = ""
                        } else {
                            lockScreenWindow.unlockBusy = false
                            lockScreenWindow.lockFailed = true
                            lockScreenWindow.unlockErrorCode = String(SessionStateClient.lastUnlockError || "")
                            var backendRetrySec = Number(SessionStateClient.lastRetryAfterSec || 0)
                            if (backendRetrySec > 0) {
                                lockScreenWindow.activateLockout(backendRetrySec)
                            } else {
                                lockScreenWindow.failedAttempts = Number(lockScreenWindow.failedAttempts || 0) + 1
                                if (lockScreenWindow.failedAttempts >= 5) {
                                    lockScreenWindow.lockoutLevel = Number(lockScreenWindow.lockoutLevel || 0) + 1
                                    var lvl = lockScreenWindow.lockoutLevel
                                    var sec = lvl >= 5 ? 300 : lvl === 4 ? 120 : lvl === 3 ? 60 : lvl === 2 ? 30 : 10
                                    lockScreenWindow.activateLockout(sec)
                                }
                            }
                        }
                    })
                }
            }
        }
    }

    Loader {
        id: motionDebugOverlayLoader
        active: !!root.motionDebugOverlayEnabled
        asynchronous: false
        sourceComponent: Component {
            OverlayComp.MotionDebugOverlay {
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
    }
}
