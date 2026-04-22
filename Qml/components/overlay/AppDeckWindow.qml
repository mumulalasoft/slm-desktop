import QtQuick 2.15
import QtQuick.Window 2.15
import Slm_Desktop
import "../appdeck" as AppDeckComp
import "./AppHubActions.js" as AppHubActions
import "../shell/PulseController.js" as PulseController

Window {
    id: root

    required property var rootWindow
    required property var desktopScene
    required property var appsModel
    required property var pulseResultsModel
    property bool dockHostVisible: Number(ShellState.dockOpacity || 1.0) > 0.01
    property bool dockAcceptsInput: true
    property int zoomHeadroom: 18

    signal requestOpenApp(string appId)
    signal requestCollapse()
    signal requestOpenAppHub()

    readonly property var dockItem: collapsedView.dockItem
    readonly property var bootstrap: (typeof AppDeckBootstrapState !== "undefined") ? AppDeckBootstrapState : null
    readonly property bool layerShellAvailable: (typeof WlrLayerShell !== "undefined") && !!WlrLayerShell
    readonly property bool layerShellSupported: layerShellAvailable && !!WlrLayerShell.isSupported()
    readonly property bool dockLayerReady: !layerShellAvailable
                                           || !layerShellSupported
                                           || (bootstrap ? !!bootstrap.readyToRender : true)
    readonly property bool bootstrapSurfaceVisible: layerShellSupported && !root.dockLayerReady

    readonly property string appdeckState: {
        if (!rootWindow || rootWindow.lockScreenVisible) {
            return "hidden"
        }
        if (rootWindow.searchVisible === true) {
            return "context"
        }
        var apphubOn = false
        if (typeof ShellStateController !== "undefined" && ShellStateController) {
            apphubOn = (ShellStateController.apphubVisible === true)
        } else if (desktopScene) {
            apphubOn = (desktopScene.apphubVisible === true)
        }
        if (apphubOn) {
            return "expanded"
        }
        return "collapsed"
    }

    readonly property bool collapsedMode: appdeckState === "collapsed"
    readonly property bool expandedMode: appdeckState === "expanded"
    readonly property bool contextMode: appdeckState === "context"
    readonly property bool hiddenMode: appdeckState === "hidden"
    readonly property bool immersiveMode: expandedMode || contextMode
    readonly property int motionSurfaceDuration: Theme.durationNormal
    readonly property int motionCrossfadeDuration: Theme.durationFast
    readonly property int motionQuickDuration: Theme.durationSm

    // Shared transition progress: 0 = collapsed surface, 1 = expanded/context surface.
    property real surfaceTransition: immersiveMode ? 1.0 : 0.0
    property real expandedTransition: expandedMode ? 1.0 : 0.0
    property real contextTransition: contextMode ? 1.0 : 0.0
    property bool appdeckLifecycleActive: false
    property bool appdeckProfilePinned: false
    property string appdeckPrevChannel: ""
    property string appdeckPrevPreset: ""

    readonly property var targetScreenGeometry: {
        if (rootWindow && rootWindow.screen) {
            return {
                "x": Number(rootWindow.screen.virtualX || 0),
                "y": Number(rootWindow.screen.virtualY || 0),
                "width": Number(rootWindow.screen.width || (rootWindow ? rootWindow.width : Screen.width)),
                "height": Number(rootWindow.screen.height || (rootWindow ? rootWindow.height : Screen.height))
            }
        }
        return {
            "x": Number(rootWindow ? rootWindow.x : 0),
            "y": Number(rootWindow ? rootWindow.y : 0),
            "width": Number(rootWindow ? rootWindow.width : Screen.width),
            "height": Number(rootWindow ? rootWindow.height : Screen.height)
        }
    }

    function setAppHubVisibility(visible) {
        var v = !!visible
        var handledController = false
        var handledDesktopScene = false

        if (typeof ShellStateController !== "undefined"
                && ShellStateController
                && ShellStateController.setAppHubVisible) {
            ShellStateController.setAppHubVisible(v)
            if (!v && ShellStateController.setAppHubSearchSeed) {
                ShellStateController.setAppHubSearchSeed("")
            }
            handledController = true
        }

        // Keep desktopScene fallback for local-only mode (without controller).
        if (desktopScene && desktopScene.setAppHubVisible && !handledController) {
            desktopScene.setAppHubVisible(v)
            handledDesktopScene = true
        }
        void handledDesktopScene
    }

    function enterCollapsedMode() {
        setAppHubVisibility(false)
        if (rootWindow && rootWindow.setSearchVisible) {
            rootWindow.setSearchVisible(false)
        }
    }

    function enterExpandedMode() {
        if (rootWindow && rootWindow.setSearchVisible) {
            rootWindow.setSearchVisible(false)
        }
        setAppHubVisibility(true)
    }

    function enterContextMode() {
        setAppHubVisibility(false)
        if (rootWindow && rootWindow.setSearchVisible) {
            rootWindow.setSearchVisible(true)
        }
    }

    function enterHiddenMode() {
        if (rootWindow && rootWindow.setSearchVisible) {
            rootWindow.setSearchVisible(false)
        }
        setAppHubVisibility(false)
    }

    function focusSearchField() {
        if (contextView && contextView.focusSearchField) {
            contextView.focusSearchField()
        }
    }

    function beginAppDeckLifecycle(reason) {
        void reason
        if (typeof MotionController === "undefined" || !MotionController) {
            return
        }
        if (!MotionController.beginLifecycleTransition || !MotionController.endLifecycleTransition) {
            return
        }
        if (!root.appdeckProfilePinned) {
            root.appdeckPrevChannel = String(MotionController.channel || "")
            root.appdeckPrevPreset = String(MotionController.preset || "")
            root.appdeckProfilePinned = true
        }
        if (MotionController.channel !== "appdeck.surface") {
            MotionController.channel = "appdeck.surface"
        }
        if (MotionController.preset !== "smooth") {
            MotionController.preset = "smooth"
        }
        MotionController.beginLifecycleTransition("appdeck.surface", MotionController.MediumPriority)
        root.appdeckLifecycleActive = true
        appdeckLifecycleReleaseTimer.restart()
    }

    function endAppDeckLifecycle() {
        if (typeof MotionController === "undefined" || !MotionController ||
                !MotionController.endLifecycleTransition) {
            return
        }
        if (root.appdeckLifecycleActive) {
            MotionController.endLifecycleTransition("appdeck.surface")
            root.appdeckLifecycleActive = false
        }
        if (root.appdeckProfilePinned) {
            if (root.appdeckPrevChannel.length > 0 && MotionController.channel !== root.appdeckPrevChannel) {
                MotionController.channel = root.appdeckPrevChannel
            }
            if (root.appdeckPrevPreset.length > 0 && MotionController.preset !== root.appdeckPrevPreset) {
                MotionController.preset = root.appdeckPrevPreset
            }
            root.appdeckProfilePinned = false
            root.appdeckPrevChannel = ""
            root.appdeckPrevPreset = ""
        }
    }

    function applyPulseQuery(text) {
        if (!root.rootWindow) {
            return
        }
        var normalized = String(text || "")
        var current = String(root.rootWindow.pulseQuery || "")
        if (normalized !== current) {
            root.rootWindow.setSearchQuery(normalized)
            root.rootWindow.pulseQueryGeneration = Number(root.rootWindow.pulseQueryGeneration || 0) + 1
        }
        pulseDebounce.restart()
    }

    color: "transparent"
    flags: Qt.FramelessWindowHint
           | Qt.WindowStaysOnTopHint
           | ((collapsedMode || hiddenMode) ? Qt.WindowDoesNotAcceptFocus : 0)
    transientParent: null
    title: "SLM AppDeck Surface"

    visible: root.dockHostVisible
             && !!rootWindow
             && !!rootWindow.visible
             && !hiddenMode
             && (root.dockLayerReady || root.bootstrapSurfaceVisible)

    readonly property int dockSurfaceHeight: Math.max(
                                                 108,
                                                 Math.ceil(
                                                     (collapsedView.dockItem ? collapsedView.dockItem.height : 120)
                                                     + Number(root.zoomHeadroom || 56)
                                                     + 8
                                                 )
                                             )
    readonly property bool compactCollapsedSurface: collapsedMode && !root.layerShellSupported
    readonly property int collapsedSurfaceWidth: Math.max(
                                                     1,
                                                     Math.ceil(
                                                         compactCollapsedSurface
                                                         ? (collapsedInputWidth + 10)
                                                         : Number(root.targetScreenGeometry.width
                                                                  || (rootWindow ? rootWindow.width : Screen.width))
                                                     )
                                                 )
    readonly property int collapsedSurfaceHeight: Math.max(
                                                      1,
                                                      Math.ceil(
                                                          compactCollapsedSurface
                                                          ? (collapsedInputHeight + 10)
                                                          : dockSurfaceHeight
                                                      )
                                                  )

    width: collapsedMode
           ? collapsedSurfaceWidth
           : Number(root.targetScreenGeometry.width || (rootWindow ? rootWindow.width : Screen.width))
    height: collapsedMode
            ? collapsedSurfaceHeight
            : Number(root.targetScreenGeometry.height || (rootWindow ? rootWindow.height : Screen.height))
    x: collapsedMode
       ? (compactCollapsedSurface
          ? (Number(root.targetScreenGeometry.x || 0)
             + Math.round((Number(root.targetScreenGeometry.width || (rootWindow ? rootWindow.width : Screen.width)) - width) * 0.5))
          : Number(root.targetScreenGeometry.x || 0))
       : Number(root.targetScreenGeometry.x || 0)
    y: collapsedMode
       ? (Number(root.targetScreenGeometry.y || 0)
          + Number(root.targetScreenGeometry.height || (rootWindow ? rootWindow.height : Screen.height))
          - height)
       : Number(root.targetScreenGeometry.y || 0)

    opacity: root.dockLayerReady ? 1.0 : 0.0

    Behavior on surfaceTransition {
        NumberAnimation {
            duration: root.motionSurfaceDuration
            easing.type: Theme.easingDefault
        }
    }

    Behavior on expandedTransition {
        NumberAnimation {
            duration: root.motionSurfaceDuration
            easing.type: Theme.easingDefault
        }
    }

    Behavior on contextTransition {
        NumberAnimation {
            duration: root.motionSurfaceDuration
            easing.type: Theme.easingDefault
        }
    }

    Behavior on width {
        NumberAnimation {
            duration: root.motionSurfaceDuration
            easing.type: Theme.easingDefault
        }
    }

    Behavior on height {
        NumberAnimation {
            duration: root.motionSurfaceDuration
            easing.type: Theme.easingDefault
        }
    }

    Behavior on x {
        NumberAnimation {
            duration: root.motionSurfaceDuration
            easing.type: Theme.easingDefault
        }
    }

    Behavior on y {
        NumberAnimation {
            duration: root.motionSurfaceDuration
            easing.type: Theme.easingDefault
        }
    }

    property bool layerConfigured: false

    function _sendOverlayCommand(cmd) {
        if (typeof WindowingBackend === "undefined" || !WindowingBackend || !WindowingBackend.sendCommand) {
            return false
        }
        return !!WindowingBackend.sendCommand(String(cmd || ""))
    }

    readonly property int exclusiveZone: collapsedMode
                                         ? Math.max(0, Math.ceil(collapsedView.dockItem ? collapsedView.dockItem.height : 0))
                                         : 0
    readonly property int collapsedInputX: Math.max(
                                               0,
                                               Math.round(
                                                   (collapsedView ? Number(collapsedView.x || 0) : 0)
                                                   + (collapsedView.dockItem
                                                      ? Number(collapsedView.dockItem.x || 0)
                                                        + Number(collapsedView.dockItem.inputRegionX || 0)
                                                      : 0)
                                               )
                                           )
    readonly property int collapsedInputY: Math.max(
                                               0,
                                               Math.round(
                                                   (collapsedView ? Number(collapsedView.y || 0) : 0)
                                                   + (collapsedView.dockItem
                                                      ? Number(collapsedView.dockItem.y || 0)
                                                        + Number(collapsedView.dockItem.inputRegionY || 0)
                                                      : 0)
                                               )
                                           )
    readonly property int collapsedInputWidth: Math.max(
                                                   1,
                                                   Math.round(
                                                       collapsedView.dockItem
                                                       ? Number(collapsedView.dockItem.inputRegionWidth || collapsedView.dockItem.width || 1)
                                                       : 1
                                                   )
                                               )
    readonly property int collapsedInputHeight: Math.max(
                                                    1,
                                                    Math.round(
                                                        collapsedView.dockItem
                                                        ? Number(collapsedView.dockItem.inputRegionHeight || collapsedView.dockItem.height || 1)
                                                        : 1
                                                    )
                                                )

    function tryConfigureLayerShell() {
        if (root.layerConfigured) return
        if (typeof WlrLayerShell === "undefined" || !WlrLayerShell) return
        if (!WlrLayerShell.isSupported()) return

        const ok = WlrLayerShell.configureAsLayerSurface(
            root,
            2,
            2|4|8,
            root.exclusiveZone,
            "slm-appdeck"
        )
        if (ok) {
            root.layerConfigured = true
            layerShellRetryTimer.stop()
            console.info("[AppDeckWindow] layer surface configured at LayerTop"
                         + " exclusiveZone=" + root.exclusiveZone)
        }
    }

    onExclusiveZoneChanged: {
        if (root.layerConfigured
                && typeof WlrLayerShell !== "undefined" && WlrLayerShell) {
            WlrLayerShell.setExclusiveZone(root, root.exclusiveZone)
        }
    }

    function syncLayerSurfaceSize() {
        if (!root.layerConfigured
                || typeof WlrLayerShell === "undefined"
                || !WlrLayerShell) {
            return
        }
        WlrLayerShell.setLayerSurfaceSize(root,
                                          Math.max(1, Math.round(root.width)),
                                          Math.max(1, Math.round(root.height)))
        if (root.collapsedMode) {
            WlrLayerShell.setLayerSurfaceInputRegion(root,
                                                     root.collapsedInputX,
                                                     root.collapsedInputY,
                                                     root.collapsedInputWidth,
                                                     root.collapsedInputHeight)
        } else {
            WlrLayerShell.setLayerSurfaceInputRegion(root,
                                                     0,
                                                     0,
                                                     Math.max(1, Math.round(root.width)),
                                                     Math.max(1, Math.round(root.height)))
        }
    }

    Timer {
        id: layerShellRetryTimer
        interval: 50
        repeat: true
        running: !root.layerConfigured
        onTriggered: root.tryConfigureLayerShell()
    }

    onSceneGraphInitialized: root.tryConfigureLayerShell()

    Item {
        anchors.fill: parent

        Rectangle {
            anchors.fill: parent
            visible: root.surfaceTransition > 0.001
            color: Theme.color("screenshotScrim")
            opacity: root.surfaceTransition
            z: 0

            Behavior on opacity {
                NumberAnimation {
                    duration: root.motionCrossfadeDuration
                    easing.type: Theme.easingDecelerate
                }
            }
        }

        Rectangle {
            id: surfaceMorph
            z: 0
            visible: root.surfaceTransition > 0.001
            readonly property real sourceX: Number(root.collapsedInputX || 0)
            readonly property real sourceY: Number(root.collapsedInputY || 0)
            readonly property real sourceW: Number(root.collapsedInputWidth || 1)
            readonly property real sourceH: Number(root.collapsedInputHeight || 1)
            readonly property real targetMarginX: Math.max(28, root.width * 0.06)
            readonly property real targetX: contextMode ? Math.round((root.width - targetW) * 0.5) : targetMarginX
            readonly property real targetY: contextMode
                                            ? Math.max(16, Number(root.desktopScene ? root.desktopScene.panelHeight : 0) + 16)
                                            : Math.max(12, Number(root.desktopScene ? root.desktopScene.panelHeight : 0) + 8)
            readonly property real targetW: contextMode
                                            ? Math.min(980, Math.max(620, root.width - 80))
                                            : Math.max(640, root.width - (targetMarginX * 2))
            readonly property real targetH: contextMode
                                            ? Math.max(420, root.height - (targetY + 40))
                                            : Math.max(460, root.height - (targetY + 24))

            x: sourceX + (targetX - sourceX) * root.surfaceTransition
            y: sourceY + (targetY - sourceY) * root.surfaceTransition
            width: sourceW + (targetW - sourceW) * root.surfaceTransition
            height: sourceH + (targetH - sourceH) * root.surfaceTransition
            radius: Theme.radiusWindow + (10 * root.surfaceTransition)
            color: Theme.color("panel")
            border.width: 0
            opacity: 0.04 + (0.18 * root.surfaceTransition)

            Behavior on opacity {
                NumberAnimation { duration: root.motionCrossfadeDuration; easing.type: Theme.easingDecelerate }
            }
        }

        AppDeckComp.AppDeckExpandedView {
            id: expandedView
            anchors.fill: parent
            visible: root.expandedMode || opacity > 0.01
            opacity: root.expandedTransition
            transform: Translate { y: (1.0 - root.expandedTransition) * 14 }
            z: 1
            appsModel: root.appsModel
            desktopScene: root.desktopScene
            panelHeight: root.desktopScene ? root.desktopScene.panelHeight : 0
            apphubSearchSeed: root.desktopScene
                              ? String(root.desktopScene.apphubSearchSeed || "")
                              : ""
            bottomSafeInset: Math.max(
                24,
                Math.round(
                    (collapsedView.dockItem ? Number(collapsedView.dockItem.height || 120) : 120)
                    + (root.desktopScene ? Number(root.desktopScene.dockBottomMargin || 0) : 0)
                    + 20
                )
            )
            onDismissRequested: {
                root.enterCollapsedMode()
                root.requestCollapse()
            }
            onAppChosen: function(appData) {
                root.requestOpenApp(String(appData && (appData.desktopId || appData.desktopFile || appData.name) || ""))
                AppHubActions.handleAppChosen(appData)
                root.enterCollapsedMode()
            }
            onAddToDockRequested: function(appData) {
                AppHubActions.handleAddToDock(appData)
            }
            onAddToDesktopRequested: function(appData) {
                AppHubActions.handleAddToDesktop(appData, root.desktopScene)
            }

            Behavior on opacity {
                NumberAnimation {
                    duration: root.motionCrossfadeDuration
                    easing.type: Theme.easingDecelerate
                }
            }
        }

        AppDeckComp.AppDeckContextView {
            id: contextView
            anchors.fill: parent
            z: 2
            active: root.contextMode || root.contextTransition > 0.01
            enabled: root.contextMode
            opacity: root.contextTransition
            transform: Translate { y: (1.0 - root.contextTransition) * 12 }
            panelHeight: root.desktopScene ? root.desktopScene.panelHeight : 0
            currentQuery: root.rootWindow ? String(root.rootWindow.pulseQuery || "") : ""
            pulseResultsModel: root.pulseResultsModel
            // Keep icon lookup source aligned with AppHub: prefer global AppModel context.
            appModelRef: (typeof AppModel !== "undefined" && AppModel)
                         ? AppModel
                         : (root.rootWindow ? root.rootWindow.appModelRef : null)
            // Dock model is already provided to AppDeckWindow as appsModel.
            appDeckModelRef: root.appsModel
            resultsGeneration: root.rootWindow ? Number(root.rootWindow.pulseAppliedGeneration || -1) : -1
            selectedIndex: root.rootWindow ? Number(root.rootWindow.pulseSelectedIndex || -1) : -1
            showDebugInfo: root.rootWindow ? !!root.rootWindow.pulseShowDebug : false
            searchProfileMeta: root.rootWindow ? root.rootWindow.pulseProfileMeta : ({})
            telemetryMeta: root.rootWindow ? root.rootWindow.pulseTelemetryMeta : ({})
            telemetryLast: root.rootWindow ? root.rootWindow.pulseTelemetryLast : ({})
            providerStats: root.rootWindow ? root.rootWindow.pulseProviderStats : ({})
            previewData: root.rootWindow ? root.rootWindow.pulsePreviewData : ({})
            onCollapseRequested: {
                root.enterCollapsedMode()
                root.requestCollapse()
            }
            onDismissRequested: {
                root.enterCollapsedMode()
                root.requestCollapse()
            }
            onQueryChanged: function(text) {
                root.applyPulseQuery(text)
            }
            onQueryTextChangedRequest: function(text) {
                root.applyPulseQuery(text)
            }
            onSelectedIndexChangedByUser: function(indexValue) {
                if (!root.rootWindow) {
                    return
                }
                root.rootWindow.pulseSelectedIndex = indexValue
                PulseController.refreshPreview(root.rootWindow, root.pulseResultsModel)
            }
            onOpenResultRequested: function(resultId) {
                if (!root.rootWindow) {
                    return
                }
                var rid = String(resultId || "")
                if (rid.length <= 0 || !root.pulseResultsModel || !root.pulseResultsModel.get) {
                    return
                }
                var indexValue = -1
                var count = Number(root.pulseResultsModel.count || 0)
                for (var i = 0; i < count; ++i) {
                    var row = root.pulseResultsModel.get(i)
                    if (!row) {
                        continue
                    }
                    var rowId = String(row.resultId || row.desktopId || row.desktopFile || row.path || row.actionId || row.name || ("row-" + i))
                    if (rowId === rid) {
                        indexValue = i
                        break
                    }
                }
                if (indexValue >= 0) {
                    root.rootWindow.pulseSelectedIndex = indexValue
                    PulseController.activateResult(root.rootWindow, root.pulseResultsModel, indexValue)
                }
            }
            onResultActivated: function(indexValue) {
                if (!root.rootWindow) {
                    return
                }
                PulseController.activateResult(root.rootWindow, root.pulseResultsModel, indexValue)
            }
            onResultContextAction: function(indexValue, action) {
                if (!root.rootWindow) {
                    return
                }
                PulseController.activateContextAction(root.rootWindow, root.pulseResultsModel, indexValue, action)
            }

            Behavior on opacity {
                NumberAnimation {
                    duration: root.motionCrossfadeDuration
                    easing.type: Theme.easingDecelerate
                }
            }
        }

        AppDeckComp.AppDeckCollapsedView {
            id: collapsedView
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            visible: (!root.hiddenMode && root.collapsedMode) || opacity > 0.01
            opacity: 1.0 - root.surfaceTransition
            scale: 1.0 - (0.03 * root.surfaceTransition)
            transform: Translate { y: root.surfaceTransition * 10 }
            z: 3
            hostName: "appdeck"
            acceptsInput: root.dockAcceptsInput && root.collapsedMode && root.surfaceTransition < 0.05
            rendererActive: root.visible && root.dockLayerReady && root.dockHostVisible
            appsModel: root.appsModel
            onAppActivated: function(appName) {
                root.requestOpenApp(String(appName || ""))
                root.enterCollapsedMode()
            }
            onApphubRequested: {
                root.requestOpenAppHub()
                if (root.expandedMode) {
                    root.enterCollapsedMode()
                } else {
                    root.enterExpandedMode()
                }
            }

            Behavior on opacity {
                NumberAnimation {
                    duration: root.motionCrossfadeDuration
                    easing.type: Theme.easingDecelerate
                }
            }
            Behavior on scale {
                NumberAnimation {
                    duration: root.motionCrossfadeDuration
                    easing.type: Theme.easingDecelerate
                }
            }
        }
    }

    Timer {
        id: appdeckLifecycleReleaseTimer
        interval: Math.max(1, Number(root.motionSurfaceDuration || Theme.durationNormal))
        repeat: false
        onTriggered: root.endAppDeckLifecycle()
    }

    Timer {
        id: pulseDebounce
        interval: 150
        repeat: false
        onTriggered: {
            if (!root.rootWindow) {
                return
            }
            PulseController.refreshResults(root.rootWindow, root.pulseResultsModel, false)
        }
    }

    onAppdeckStateChanged: {
        root.beginAppDeckLifecycle(appdeckState)
        root.syncLayerSurfaceSize()
        if (contextMode) {
            requestActivate()
            Qt.callLater(function() {
                root.focusSearchField()
            })
        } else if (expandedMode) {
            requestActivate()
        }
    }

    onVisibleChanged: {
        if (bootstrap && bootstrap.setVisibleToUser) {
            bootstrap.setVisibleToUser(visible && root.dockLayerReady)
        }
        if (visible && !root.layerShellSupported) {
            Qt.callLater(function() { root.raise() })
        } else if (!visible) {
            root.endAppDeckLifecycle()
        }
        root.syncLayerSurfaceSize()
    }
    onWidthChanged: root.syncLayerSurfaceSize()
    onHeightChanged: root.syncLayerSurfaceSize()

    Connections {
        target: root.desktopScene ? root.desktopScene : null
        ignoreUnknownSignals: true
        function onAppHubVisibleChanged() {
            if (!root.visible || root.layerShellSupported) {
                return
            }
            Qt.callLater(function() { root.raise() })
        }
    }

    Connections {
        target: collapsedView && collapsedView.dockItem ? collapsedView.dockItem : null
        ignoreUnknownSignals: true
        function onXChanged() { root.syncLayerSurfaceSize() }
        function onYChanged() { root.syncLayerSurfaceSize() }
        function onWidthChanged() { root.syncLayerSurfaceSize() }
        function onHeightChanged() { root.syncLayerSurfaceSize() }
        function onInputRegionXChanged() { root.syncLayerSurfaceSize() }
        function onInputRegionYChanged() { root.syncLayerSurfaceSize() }
        function onInputRegionWidthChanged() { root.syncLayerSurfaceSize() }
        function onInputRegionHeightChanged() { root.syncLayerSurfaceSize() }
    }

    Component.onCompleted: {
        console.info("[AppDeckWindow] DOCK_CREATED ptr=" + root)
        root.tryConfigureLayerShell()
        root._sendOverlayCommand("overlay register appdeck slm-appdeck")
        root._sendOverlayCommand("overlay restack")
        if (!root.layerShellSupported) {
            Qt.callLater(function() { root.raise() })
        }
    }

    Component.onDestruction: {
        root.endAppDeckLifecycle()
        root._sendOverlayCommand("overlay unregister appdeck")
    }
}
