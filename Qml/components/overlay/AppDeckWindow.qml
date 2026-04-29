import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Effects
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
    readonly property bool layerShellSupported: (typeof AppDeckLayerShell !== "undefined")
                                                && !!AppDeckLayerShell
                                                && !!AppDeckLayerShell.supported
    readonly property bool dockLayerReady: !root.layerShellSupported
                                           || (bootstrap ? !!bootstrap.readyToRender : true)
    readonly property bool bootstrapSurfaceVisible: root.layerShellSupported && !root.dockLayerReady

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
    readonly property real sharedPanelMarginX: Math.max(28, root.width * 0.07)
    readonly property real sharedPanelTopInset: Math.max(18, Number(root.desktopScene ? root.desktopScene.panelHeight : 0) + 14)
    readonly property real sharedPanelBottomInset: Math.max(
                                                        24,
                                                        Math.round(
                                                            (collapsedView.dockItem ? Number(collapsedView.dockItem.height || 120) : 120)
                                                            + (root.desktopScene ? Number(root.desktopScene.dockBottomMargin || 0) : 0)
                                                            + 20
                                                        )
                                                    )
    readonly property real sharedDockWidth: Math.max(320, Number(root.collapsedInputWidth || 1) + 20)
    readonly property real expandedContentW: Math.min(1180, Math.max(320, root.width - (sharedPanelMarginX * 2)))
    readonly property real expandedContentH: Math.min(940, Math.max(360, root.height - sharedPanelTopInset - sharedPanelBottomInset))
    readonly property real expandedContentX: Math.round((root.width - expandedContentW) * 0.5)
    readonly property real expandedContentY: sharedPanelTopInset
    readonly property real expandedSurfaceW: Math.max(expandedContentW, sharedDockWidth)
    readonly property real expandedSurfaceH: Math.max(expandedContentH, root.height - expandedContentY - 8)
    readonly property real expandedSurfaceX: Math.round((root.width - expandedSurfaceW) * 0.5)
    readonly property real expandedSurfaceY: expandedContentY
    readonly property real contextContentW: Math.min(980, Math.max(620, root.width - 80))
    readonly property real contextContentY: Math.max(16, Number(root.desktopScene ? root.desktopScene.panelHeight : 0) + 16)
    readonly property real contextContentH: Math.max(420, root.height - (contextContentY + 40))
    readonly property real contextContentX: Math.round((root.width - contextContentW) * 0.5)
    readonly property real contextSurfaceW: Math.max(contextContentW, sharedDockWidth)
    readonly property real contextSurfaceH: Math.max(contextContentH, root.height - contextContentY - 8)
    readonly property real contextSurfaceX: Math.round((root.width - contextSurfaceW) * 0.5)
    readonly property real contextSurfaceY: contextContentY
    readonly property real collapsedDockBottomMargin: Math.max(
                                                      8,
                                                      Number(root.desktopScene ? root.desktopScene.dockBottomMargin : 0)
                                                  )

    readonly property bool motionEngineReady: typeof MotionController !== "undefined"
                                                && MotionController
                                                && typeof MotionController.startFromCurrent === "function"
                                                && typeof MotionController.retarget === "function"
                                                && typeof MotionController.cancelAndSettle === "function"
    readonly property real surfaceTarget: immersiveMode ? 1.0 : 0.0
    property real fallbackSurfaceTransition: surfaceTarget
    // Shared transition progress: 0 = collapsed surface, 1 = expanded/context surface.
    readonly property real engineSurfaceTransition: motionEngineReady
                                               ? Math.max(0.0, Math.min(1.0, Number(MotionController.value || 0)))
                                               : fallbackSurfaceTransition
    readonly property real surfaceTransition: immersiveMode
                                               ? Math.max(engineSurfaceTransition, fallbackSurfaceTransition)
                                               : fallbackSurfaceTransition
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

    function syncAppDeckMotionChannel() {
        if (!root.motionEngineReady) {
            return false
        }
        if (MotionController.channel !== "appdeck.surface") {
            MotionController.channel = "appdeck.surface"
        }
        if (MotionController.preset !== "launcher") {
            MotionController.preset = "launcher"
        }
        if (MotionController.ensureRunning) {
            MotionController.ensureRunning()
        }
        return true
    }

    function driveSurfaceTransition(target, immediate) {
        var t = Math.max(0.0, Math.min(1.0, Number(target || 0)))
        fallbackSurfaceTransition = t
        if (!root.syncAppDeckMotionChannel()) {
            return
        }
        if (immediate) {
            MotionController.cancelAndSettle(t)
            return
        }
        if (t <= 0.0) {
            MotionController.cancelAndSettle(0.0)
            return
        }
        if (MotionController.shouldCoalesceEvent
                && MotionController.shouldCoalesceEvent("appdeck.surface.retarget", 48)) {
            MotionController.retarget(t)
            return
        }
        MotionController.startFromCurrent(t)
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
        if (MotionController.preset !== "launcher") {
            MotionController.preset = "launcher"
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
    transientParent: null
    title: "SLM AppDeck Surface"

    visible: !!rootWindow
             && !!rootWindow.visible
             && root.layerShellSupported
    opacity: root.hiddenMode ? 0.0 : (root.dockLayerReady ? 1.0 : 0.0)

    readonly property int dockSurfaceHeight: Math.max(
                                                 108,
                                                 Math.ceil(
                                                     (collapsedView.dockItem ? collapsedView.dockItem.height : 120)
                                                     + Number(root.zoomHeadroom || 56)
                                                     + 8
                                                 )
                                             )
    readonly property bool compactCollapsedSurface: false
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

    width: Number(root.targetScreenGeometry.width || (rootWindow ? rootWindow.width : Screen.width))
    height: hiddenMode
            ? 1
            : (collapsedMode
               ? collapsedSurfaceHeight
               : Number(root.targetScreenGeometry.height || (rootWindow ? rootWindow.height : Screen.height)))
    Behavior on fallbackSurfaceTransition {
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

    function syncLayerSurfaceSize() {
        if (typeof AppDeckLayerShell === "undefined" || !AppDeckLayerShell) {
            return
        }
        var surfaceX = root.collapsedMode ? root.collapsedInputX
                                          : (root.contextMode ? root.contextSurfaceX : root.expandedSurfaceX)
        var surfaceY = root.collapsedMode ? root.collapsedInputY
                                          : (root.contextMode ? root.contextSurfaceY : root.expandedSurfaceY)
        var surfaceW = root.collapsedMode ? root.collapsedInputWidth
                                          : (root.contextMode ? root.contextSurfaceW : root.expandedSurfaceW)
        var surfaceH = root.collapsedMode ? root.collapsedInputHeight
                                          : (root.contextMode ? root.contextSurfaceH : root.expandedSurfaceH)
        var w = Math.max(1, Math.round(root.width))
        var h = Math.max(1, Math.round(root.hiddenMode ? 1 : root.height))
        if (root.immersiveMode) {
            AppDeckLayerShell.setExpanded(root, w, h,
                                          Math.max(0, Math.round(surfaceX)),
                                          Math.max(0, Math.round(surfaceY)),
                                          Math.max(1, Math.round(surfaceW)),
                                          Math.max(1, Math.round(surfaceH)))
        } else {
            AppDeckLayerShell.setCollapsed(root, w, h,
                                           Math.max(0, Math.round(root.hiddenMode ? 0 : surfaceX)),
                                           Math.max(0, Math.round(root.hiddenMode ? 0 : surfaceY)),
                                           Math.max(1, Math.round(root.hiddenMode ? 1 : surfaceW)),
                                           Math.max(1, Math.round(root.hiddenMode ? 1 : surfaceH)))
        }
    }

    Timer {
        id: layerShellRetryTimer
        interval: 300
        repeat: true
        running: root.visible && root.layerShellSupported
        onTriggered: root.syncLayerSurfaceSize()
    }

    onSceneGraphInitialized: root.syncLayerSurfaceSize()

    Item {
        anchors.fill: parent

        Rectangle {
            anchors.fill: parent
            visible: false
            color: Theme.color("screenshotScrim")
            opacity: root.surfaceTransition
            z: 0

            Behavior on opacity {
                enabled: !root.motionEngineReady
                NumberAnimation {
                    duration: root.motionCrossfadeDuration
                    easing.type: Theme.easingDecelerate
                }
            }
        }

        Rectangle {
            id: surfaceMorph
            z: 0
            visible: !root.hiddenMode && root.immersiveMode && root.surfaceTransition > 0.001
            readonly property real sourceX: Number(root.collapsedInputX || 0)
            readonly property real sourceY: Number(root.collapsedInputY || 0)
            readonly property real sourceW: Number(root.collapsedInputWidth || 1)
            readonly property real sourceH: Number(root.collapsedInputHeight || 1)
            readonly property real targetX: contextMode ? root.contextSurfaceX : root.expandedSurfaceX
            readonly property real targetY: contextMode ? root.contextSurfaceY : root.expandedSurfaceY
            readonly property real targetW: contextMode ? root.contextSurfaceW : root.expandedSurfaceW
            readonly property real targetH: contextMode ? root.contextSurfaceH : root.expandedSurfaceH

            x: sourceX + (targetX - sourceX) * root.surfaceTransition
            y: sourceY + (targetY - sourceY) * root.surfaceTransition
            width: sourceW + (targetW - sourceW) * root.surfaceTransition
            height: sourceH + (targetH - sourceH) * root.surfaceTransition
            radius: Theme.radiusWindow + (Math.min(8, Theme.radiusWindow) * root.surfaceTransition)
            color: root.immersiveMode ? Theme.color("windowCard") : Theme.color("dockBg")
            border.width: Theme.borderWidthThin
            border.color: Theme.color("panelBorder")
            opacity: root.immersiveMode ? 1.0 : 0.0
            layer.enabled: visible
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, Theme.darkMode ? 0.42 : 0.24)
                shadowBlur: 0.54
                shadowVerticalOffset: 10 + (6 * root.surfaceTransition)
                shadowHorizontalOffset: 0
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                anchors.topMargin: 1
                height: 1
                radius: Theme.radiusHairline
                color: Qt.rgba(1, 1, 1, Theme.darkMode ? 0.16 : 0.58)
                opacity: root.surfaceTransition
            }

            Behavior on opacity {
                enabled: !root.motionEngineReady
                NumberAnimation { duration: root.motionCrossfadeDuration; easing.type: Theme.easingDecelerate }
            }
            Behavior on x {
                enabled: !root.motionEngineReady
                NumberAnimation { duration: root.motionSurfaceDuration; easing.type: Theme.easingDefault }
            }
            Behavior on y {
                enabled: !root.motionEngineReady
                NumberAnimation { duration: root.motionSurfaceDuration; easing.type: Theme.easingDefault }
            }
            Behavior on width {
                enabled: !root.motionEngineReady
                NumberAnimation { duration: root.motionSurfaceDuration; easing.type: Theme.easingDefault }
            }
            Behavior on height {
                enabled: !root.motionEngineReady
                NumberAnimation { duration: root.motionSurfaceDuration; easing.type: Theme.easingDefault }
            }
            Behavior on radius {
                enabled: !root.motionEngineReady
                NumberAnimation { duration: root.motionSurfaceDuration; easing.type: Theme.easingDefault }
            }
            Behavior on color {
                ColorAnimation { duration: root.motionCrossfadeDuration; easing.type: Theme.easingStandard }
            }
        }

        Rectangle {
            id: steadyImmersiveSurface
            z: 0.5
            visible: root.immersiveMode
            x: root.contextMode ? root.contextSurfaceX : root.expandedSurfaceX
            y: root.contextMode ? root.contextSurfaceY : root.expandedSurfaceY
            width: root.contextMode ? root.contextSurfaceW : root.expandedSurfaceW
            height: root.contextMode ? root.contextSurfaceH : root.expandedSurfaceH
            radius: Theme.radiusWindow + Math.min(8, Theme.radiusWindow)
            color: Theme.color("windowCard")
            border.width: Theme.borderWidthThin
            border.color: Theme.color("panelBorder")
            opacity: root.immersiveMode ? 1.0 : 0.0

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                anchors.topMargin: 1
                height: 1
                radius: Theme.radiusHairline
                color: Qt.rgba(1, 1, 1, Theme.darkMode ? 0.16 : 0.58)
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
            preferredPanelX: root.expandedContentX
            preferredPanelY: root.expandedContentY
            preferredPanelWidth: root.expandedContentW
            preferredPanelHeight: root.expandedContentH
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
                AppHubActions.handleAddToDock(appData, root.appsModel)
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
            preferredSurfaceX: root.contextContentX
            preferredSurfaceY: root.contextContentY
            preferredSurfaceWidth: root.contextContentW
            preferredSurfaceHeight: root.contextContentH
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
            anchors.bottomMargin: root.collapsedMode ? root.collapsedDockBottomMargin : 0
            visible: !root.hiddenMode && (root.collapsedMode || root.expandedMode || root.contextMode || opacity > 0.01)
            opacity: 1.0
            scale: root.collapsedMode ? 1.0 : (1.0 - (0.03 * root.surfaceTransition))
            transform: Translate { y: root.collapsedMode ? 0 : root.surfaceTransition * 10 }
            z: 3
            hostName: "appdeck"
            hideBorder: root.immersiveMode
            transparentBackground: root.immersiveMode
            acceptsInput: root.dockAcceptsInput && root.collapsedMode
            rendererActive: root.visible && root.dockLayerReady && root.dockHostVisible
                            && !root.hiddenMode
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
                enabled: !root.motionEngineReady
                NumberAnimation {
                    duration: root.motionCrossfadeDuration
                    easing.type: Theme.easingDecelerate
                }
            }
            Behavior on scale {
                enabled: !root.motionEngineReady
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
        root.driveSurfaceTransition(root.surfaceTarget, false)
        root.syncLayerSurfaceSize()
        if (contextMode) {
            Qt.callLater(function() {
                root.focusSearchField()
            })
        }
    }

    onVisibleChanged: {
        if (bootstrap && bootstrap.setVisibleToUser) {
            bootstrap.setVisibleToUser(visible && root.dockLayerReady)
        }
        if (!visible) {
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
            Qt.callLater(function() { root.syncLayerSurfaceSize() })
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
        root.driveSurfaceTransition(root.surfaceTarget, true)
        Qt.callLater(function() { root.syncLayerSurfaceSize() })
    }

    Component.onDestruction: {
        root.endAppDeckLifecycle()
    }
}
