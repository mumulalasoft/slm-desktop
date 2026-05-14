import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Effects
import Slm_Desktop
import "../appdeck" as AppDeckComp
import "./AppDeckActions.js" as AppDeckActions
import "../shell/PulseController.js" as PulseController

Window {
    id: root

    required property var rootWindow
    required property var desktopScene
    required property var appsModel
    required property var pulseResultsModel
    property int zoomHeadroom: 18

    signal requestOpenApp(string appId)
    signal requestCollapse()
    signal requestOpenAppDeck()

    readonly property var shellPolicy: (typeof ShellPolicyController !== "undefined")
                                       ? ShellPolicyController
                                       : null
    readonly property bool policySurfaceVisible: !root.shellPolicy
                                                 || root.shellPolicy.appDeckSurfaceVisible
    readonly property bool policyContentVisible: !root.shellPolicy
                                                 || root.shellPolicy.appDeckContentVisible
    readonly property bool policyEdgeRevealEnabled: !!root.shellPolicy
                                                    && root.shellPolicy.edgeRevealEnabled
    readonly property bool policyAllowsExpandedModes: !root.shellPolicy
                                                      || root.shellPolicy.visibilityPolicyName === "Normal"
    readonly property int revealActivationHeight: root.shellPolicy
                                                 ? Math.max(1, Math.round(root.shellPolicy.edgeRevealSize || 6))
                                                 : 6
    readonly property bool appDeckSuppressed: !rootWindow
                                              || rootWindow.lockScreenVisible
                                              || !root.policySurfaceVisible
    readonly property bool policySensorOnly: !root.appDeckSuppressed
                                             && root.policyEdgeRevealEnabled
                                             && !root.policyContentVisible
                                             && root.dockActive
    property bool dockHostVisible: Number(ShellState.dockOpacity || 1.0) > 0.01
                                   && root.policyContentVisible
    property bool dockAcceptsInput: root.policyContentVisible
    property bool bottomRevealHeld: false

    readonly property var dockItem: dockView.dockItem
    readonly property var bootstrap: (typeof AppDeckBootstrapState !== "undefined") ? AppDeckBootstrapState : null
    readonly property bool layerShellSupported: (typeof AppDeckLayerShell !== "undefined")
                                                && !!AppDeckLayerShell
                                                && !!AppDeckLayerShell.supported
    readonly property bool dockLayerReady: !root.layerShellSupported
                                           || (bootstrap ? !!bootstrap.readyToRender : true)
    readonly property bool bootstrapSurfaceVisible: root.layerShellSupported && !root.dockLayerReady

    readonly property bool appDeckHidden: root.appDeckSuppressed
    readonly property bool appDeckGridRequested: {
        if (root.appDeckHidden) {
            return false
        }
        if (!root.policyAllowsExpandedModes) {
            return false
        }
        if (rootWindow.searchVisible === true) {
            return true
        }
        if (typeof ShellStateController !== "undefined" && ShellStateController) {
            return ShellStateController.appdeckVisible === true
        }
        return desktopScene ? desktopScene.appdeckVisible === true : false
    }
    readonly property bool appDeckContextRequested: !root.appDeckHidden
                                                    && root.policyAllowsExpandedModes
                                                    && typeof AppDeckController !== "undefined"
                                                    && AppDeckController
                                                    && String(AppDeckController.contextItemId || "").length > 0
    property string state: root.appDeckGridRequested || root.appDeckContextRequested ? "grid" : "dock"
    property string mode: {
        if (root.state === "dock") {
            return "apps"
        }
        if (rootWindow.searchVisible === true) {
            return "pulse"
        }
        if (root.appDeckContextRequested) {
            return "context"
        }
        return "apps"
    }

    readonly property bool dockActive: root.state === "dock"
    readonly property bool gridActive: root.state === "grid"
    readonly property bool appsMode: root.mode === "apps"
    readonly property bool gridAppsMode: root.gridActive && root.appsMode
    readonly property bool pulseMode: root.gridActive && root.mode === "pulse"
    readonly property bool contextMode: root.gridActive && root.mode === "context"
    readonly property bool immersiveMode: root.gridActive
    readonly property int motionSurfaceDuration: Theme.durationNormal
    readonly property int motionCrossfadeDuration: Theme.durationFast
    readonly property int motionQuickDuration: Theme.durationSm
    readonly property real sharedPanelMarginX: Math.max(28, root.width * 0.07)
    readonly property real sharedPanelTopInset: Math.max(18, Number(root.desktopScene ? root.desktopScene.panelHeight : 0) + 14)
    readonly property real sharedPanelBottomInset: Math.max(
                                                        24,
                                                        Math.round(
                                                            (dockView.dockItem ? Number(dockView.dockItem.height || 120) : 120)
                                                            + (root.desktopScene ? Number(root.desktopScene.dockBottomMargin || 0) : 0)
                                                            + 20
                                                        )
                                                    )
    readonly property real sharedDockWidth: Math.max(320, Number(root.dockInputWidth || 1) + 20)
    readonly property real gridContentW: Math.min(1180, Math.max(320, root.width - (sharedPanelMarginX * 2)))
    readonly property real gridContentH: Math.min(940, Math.max(360, root.height - sharedPanelTopInset - sharedPanelBottomInset))
    readonly property real gridContentX: Math.round((root.width - gridContentW) * 0.5)
    readonly property real gridContentY: sharedPanelTopInset
    readonly property real gridSurfaceW: Math.max(gridContentW, sharedDockWidth)
    readonly property real gridSurfaceH: Math.max(gridContentH, root.height - gridContentY - 8)
    readonly property real gridSurfaceX: Math.round((root.width - gridSurfaceW) * 0.5)
    readonly property real gridSurfaceY: gridContentY
    readonly property real contextContentW: Math.min(980, Math.max(620, root.width - 80))
    readonly property real contextContentY: Math.max(16, Number(root.desktopScene ? root.desktopScene.panelHeight : 0) + 16)
    readonly property real contextContentH: Math.max(420, root.height - (contextContentY + 40))
    readonly property real contextContentX: Math.round((root.width - contextContentW) * 0.5)
    readonly property real contextSurfaceW: Math.max(contextContentW, sharedDockWidth)
    readonly property real contextSurfaceH: Math.max(contextContentH, root.height - contextContentY - 8)
    readonly property real contextSurfaceX: Math.round((root.width - contextSurfaceW) * 0.5)
    readonly property real contextSurfaceY: contextContentY
    readonly property real dockBottomMargin: Math.max(
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
    // Shared transition progress: 0 = dock surface, 1 = grid/pulse surface.
    readonly property real engineSurfaceTransition: motionEngineReady
                                               ? Math.max(0.0, Math.min(1.0, Number(MotionController.value || 0)))
                                               : fallbackSurfaceTransition
    readonly property real surfaceTransition: immersiveMode
                                               ? Math.max(engineSurfaceTransition, fallbackSurfaceTransition)
                                               : fallbackSurfaceTransition
    property real appsTransition: gridAppsMode ? 1.0 : 0.0
    property real pulseTransition: (pulseMode || contextMode) ? 1.0 : 0.0
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

    function setAppDeckVisibility(visible) {
        var v = !!visible
        var handledController = false
        var handledDesktopScene = false

        if (typeof ShellStateController !== "undefined"
                && ShellStateController
                && ShellStateController.setAppDeckVisible) {
            ShellStateController.setAppDeckVisible(v)
            if (!v && ShellStateController.setAppDeckSearchSeed) {
                ShellStateController.setAppDeckSearchSeed("")
            }
            handledController = true
        }

        // Keep desktopScene fallback for local-only mode (without controller).
        if (desktopScene && desktopScene.setAppDeckVisible && !handledController) {
            desktopScene.setAppDeckVisible(v)
            handledDesktopScene = true
        }
        void handledDesktopScene
    }

    function enterDock() {
        setAppDeckVisibility(false)
        if (rootWindow && rootWindow.setSearchVisible) {
            rootWindow.setSearchVisible(false)
        }
    }

    function enterGrid() {
        if (rootWindow && rootWindow.setSearchVisible) {
            rootWindow.setSearchVisible(false)
        }
        setAppDeckVisibility(true)
    }

    function enterPulse() {
        setAppDeckVisibility(false)
        if (rootWindow && rootWindow.setSearchVisible) {
            rootWindow.setSearchVisible(true)
        }
    }

    function hideAppDeck() {
        if (rootWindow && rootWindow.setSearchVisible) {
            rootWindow.setSearchVisible(false)
        }
        setAppDeckVisibility(false)
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
    flags: Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint

    property bool _layerPrepared: !layerShellSupported

    visible: !!rootWindow
             && !!rootWindow.visible
             && root.policySurfaceVisible
             && root.layerShellSupported
             && root._layerPrepared
    // NOTE: opacity must NOT be set on Window (Wayland layer-shell warns).
    // Visual fade is delegated to the inner content Item (see anchors-fill Item below).
    readonly property real contentOpacity: root.appDeckHidden || !root.policyContentVisible
                                           ? 0.0
                                           : (root.dockLayerReady ? 1.0 : 0.0)

    readonly property int dockContentHeight: Math.max(
                                                 108,
                                                 Math.ceil(
                                                     (dockView.dockItem ? dockView.dockItem.height : 120)
                                                     + Number(root.zoomHeadroom || 56)
                                                     + 8
                                                 )
                                             )
    readonly property bool compactDockSurface: false
    readonly property int dockSurfaceWidth: Math.max(
                                                     1,
                                                     Math.ceil(
                                                         compactDockSurface
                                                         ? (dockInputWidth + 10)
                                                         : Number(root.targetScreenGeometry.width
                                                                  || (rootWindow ? rootWindow.width : Screen.width))
                                                     )
                                                 )
    readonly property int dockSurfaceHeight: Math.max(
                                                      1,
                                                      Math.ceil(
                                                          compactDockSurface
                                                          ? (dockInputHeight + 10)
                                                          : root.dockContentHeight
                                                      )
                                                  )

    width: Number(root.targetScreenGeometry.width || (rootWindow ? rootWindow.width : Screen.width))
    x: Number(root.targetScreenGeometry.x || 0)
    y: Number(root.targetScreenGeometry.y || 0)
    height: appDeckHidden
            ? 1
            : Number(root.targetScreenGeometry.height || (rootWindow ? rootWindow.height : Screen.height))
    Behavior on fallbackSurfaceTransition {
        NumberAnimation {
            duration: root.motionSurfaceDuration
            easing.type: Theme.easingDefault
        }
    }

    Behavior on appsTransition {
        NumberAnimation {
            duration: root.motionSurfaceDuration
            easing.type: Theme.easingDefault
        }
    }

    Behavior on pulseTransition {
        NumberAnimation {
            duration: root.motionSurfaceDuration
            easing.type: Theme.easingDefault
        }
    }

    // No Behavior on width/height/x/y for the layer-shell Window root.
    // Animating those triggers QWindow::setGeometry every frame, which Qt's
    // Wayland plugin translates into per-tick wl_surface_set_size + commit on
    // the layer surface. KWin replies to each set_size with a new configure
    // event; if commits race ahead of acks (very likely at animation rate),
    // the protocol violation drops the wl_display connection.
    // Visual transitions between dock/grid/pulse already animate via inner
    // content (surfaceMorph, gridAppsView/contextView opacity+transform,
    // dockView scale+transform) — those are pure client-side render and are
    // unaffected.

    readonly property int exclusiveZone: dockActive && root.policyContentVisible
                                         ? Math.max(0, Math.ceil(dockView.dockItem ? dockView.dockItem.height : 0))
                                         : 0
    readonly property int dockInputX: Math.max(
                                               0,
                                               Math.round(
                                                   (dockView ? Number(dockView.x || 0) : 0)
                                                   + (dockView.dockItem
                                                      ? Number(dockView.dockItem.x || 0)
                                                        + Number(dockView.dockItem.inputRegionX || 0)
                                                      : 0)
                                               )
                                           )
    readonly property int dockInputY: Math.max(
                                               0,
                                               Math.round(
                                                   (dockView ? Number(dockView.y || 0) : 0)
                                                   + (dockView.dockItem
                                                      ? Number(dockView.dockItem.y || 0)
                                                        + Number(dockView.dockItem.inputRegionY || 0)
                                                      : 0)
                                               )
                                           )
    readonly property int dockInputWidth: Math.max(
                                                   1,
                                                   Math.round(
                                                       dockView.dockItem
                                                       ? Number(dockView.dockItem.inputRegionWidth || dockView.dockItem.width || 1)
                                                       : 1
                                                   )
                                               )
    readonly property int dockInputHeight: Math.max(
                                                    1,
                                                    Math.round(
                                                        dockView.dockItem
                                                        ? Number(dockView.dockItem.inputRegionHeight || dockView.dockItem.height || 1)
                                                        : 1
                                                    )
                                                )

    function syncLayerSurfaceSize() {
        if (typeof AppDeckLayerShell === "undefined" || !AppDeckLayerShell) {
            return
        }
        var w = Math.max(1, Math.round(root.width))
        var h = Math.max(1, Math.round(root.appDeckHidden ? 1 : root.height))
        var surfaceX = root.dockActive ? root.dockInputX
                                          : (root.pulseMode ? root.contextSurfaceX : root.gridSurfaceX)
        var surfaceY = root.dockActive ? root.dockInputY
                                          : (root.pulseMode ? root.contextSurfaceY : root.gridSurfaceY)
        var surfaceW = root.dockActive ? root.dockInputWidth
                                          : (root.pulseMode ? root.contextSurfaceW : root.gridSurfaceW)
        var surfaceH = root.dockActive ? root.dockInputHeight
                                          : (root.pulseMode ? root.contextSurfaceH : root.gridSurfaceH)
        if (root.policySensorOnly) {
            surfaceX = 0
            surfaceY = Math.max(0, h - root.revealActivationHeight)
            surfaceW = w
            surfaceH = root.revealActivationHeight
        }
        if (root.immersiveMode) {
            AppDeckLayerShell.setGrid(root, w, h,
                                          Math.max(0, Math.round(surfaceX)),
                                          Math.max(0, Math.round(surfaceY)),
                                          Math.max(1, Math.round(surfaceW)),
                                          Math.max(1, Math.round(surfaceH)))
        } else {
            AppDeckLayerShell.setDock(root, w, h,
                                           Math.max(0, Math.round(root.appDeckHidden ? 0 : surfaceX)),
                                           Math.max(0, Math.round(root.appDeckHidden ? 0 : surfaceY)),
                                           Math.max(1, Math.round(root.appDeckHidden ? 1 : surfaceW)),
                                           Math.max(1, Math.round(root.appDeckHidden ? 1 : surfaceH)))
        }
    }

    // The timer must keep running while the window is visible: configure()
    // refuses calls before QWindow::isExposed() is true, so the very first
    // syncLayerSurfaceSize triggered by onVisibleChanged / property handlers
    // can return without applying anything. The timer guarantees a retry
    // after KWin's configure/ack populates exposure. Once geometry is
    // actually applied, the C++ idempotency cache turns subsequent fires
    // into a cheap no-op (Q_INVOKABLE call + arg compare + bail).
    Timer {
        id: layerShellRetryTimer
        interval: 300
        repeat: true
        running: root.visible && root.layerShellSupported
        onTriggered: root.syncLayerSurfaceSize()
    }

    onSceneGraphInitialized: root.syncLayerSurfaceSize()
    onAppDeckHiddenChanged: root.syncLayerSurfaceSize()
    onImmersiveModeChanged: root.syncLayerSurfaceSize()
    onPulseModeChanged: root.syncLayerSurfaceSize()
    onDockActiveChanged: root.syncLayerSurfaceSize()
    onDockLayerReadyChanged: {
        if (root.dockLayerReady) {
            root.syncLayerSurfaceSize()
        }
    }

    Item {
        anchors.fill: parent
        opacity: root.contentOpacity

        Behavior on opacity {
            NumberAnimation {
                duration: Theme.durationFast
                easing.type: Theme.easingDefault
            }
        }

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
            visible: !root.appDeckHidden && root.immersiveMode && root.surfaceTransition > 0.001
            readonly property real sourceX: Number(root.dockInputX || 0)
            readonly property real sourceY: Number(root.dockInputY || 0)
            readonly property real sourceW: Number(root.dockInputWidth || 1)
            readonly property real sourceH: Number(root.dockInputHeight || 1)
            readonly property real targetX: pulseMode ? root.contextSurfaceX : root.gridSurfaceX
            readonly property real targetY: pulseMode ? root.contextSurfaceY : root.gridSurfaceY
            readonly property real targetW: pulseMode ? root.contextSurfaceW : root.gridSurfaceW
            readonly property real targetH: pulseMode ? root.contextSurfaceH : root.gridSurfaceH

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
            x: root.pulseMode ? root.contextSurfaceX : root.gridSurfaceX
            y: root.pulseMode ? root.contextSurfaceY : root.gridSurfaceY
            width: root.pulseMode ? root.contextSurfaceW : root.gridSurfaceW
            height: root.pulseMode ? root.contextSurfaceH : root.gridSurfaceH
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

        AppDeckComp.AppDeckGridAppsView {
            id: gridAppsView
            anchors.fill: parent
            visible: root.gridAppsMode || opacity > 0.01
            opacity: root.appsTransition
            transform: Translate { y: (1.0 - root.appsTransition) * 14 }
            z: 1
            appsModel: root.appsModel
            desktopScene: root.desktopScene
            panelHeight: root.desktopScene ? root.desktopScene.panelHeight : 0
            preferredPanelX: root.gridContentX
            preferredPanelY: root.gridContentY
            preferredPanelWidth: root.gridContentW
            preferredPanelHeight: root.gridContentH
            appdeckSearchSeed: root.desktopScene
                              ? String(root.desktopScene.appdeckSearchSeed || "")
                              : ""
            bottomSafeInset: Math.max(
                24,
                Math.round(
                    (dockView.dockItem ? Number(dockView.dockItem.height || 120) : 120)
                    + (root.desktopScene ? Number(root.desktopScene.dockBottomMargin || 0) : 0)
                    + 20
                )
            )
            onDismissRequested: {
                root.enterDock()
                root.requestCollapse()
            }
            onAppChosen: function(appData) {
                root.requestOpenApp(String(appData && (appData.desktopId || appData.desktopFile || appData.name) || ""))
                AppDeckActions.handleAppChosen(appData)
                root.enterDock()
            }
            onAddToDockRequested: function(appData) {
                AppDeckActions.handleAddToDock(appData, root.appsModel)
            }
            onAddToDesktopRequested: function(appData) {
                AppDeckActions.handleAddToDesktop(appData, root.desktopScene)
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
            active: root.pulseMode || root.contextMode || root.pulseTransition > 0.01
            enabled: root.pulseMode || root.contextMode
            opacity: root.pulseTransition
            transform: Translate { y: (1.0 - root.pulseTransition) * 12 }
            panelHeight: root.desktopScene ? root.desktopScene.panelHeight : 0
            preferredSurfaceX: root.contextContentX
            preferredSurfaceY: root.contextContentY
            preferredSurfaceWidth: root.contextContentW
            preferredSurfaceHeight: root.contextContentH
            currentQuery: root.rootWindow ? String(root.rootWindow.pulseQuery || "") : ""
            pulseResultsModel: root.pulseResultsModel
            // Keep icon lookup source aligned with AppDeck: prefer global AppModel context.
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
                root.enterDock()
                root.requestCollapse()
            }
            onDismissRequested: {
                root.enterDock()
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
            id: dockView
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            // Keep dock geometry stable across dock/grid states so pinned icons
            // do not visually shift when surface mode changes.
            anchors.bottomMargin: root.dockBottomMargin
            visible: !root.appDeckHidden && (root.dockActive || root.gridAppsMode || root.pulseMode || opacity > 0.01)
            opacity: 1.0
            scale: 1.0
            transform: Translate { y: 0 }
            z: 3
            hostName: "appdeck"
            hideBorder: root.immersiveMode
            transparentBackground: root.immersiveMode
            acceptsInput: root.dockAcceptsInput && root.dockActive
            rendererActive: root.visible && root.dockLayerReady && root.dockHostVisible
                            && !root.appDeckHidden
            appsModel: root.appsModel
            onAppActivated: function(appName) {
                root.requestOpenApp(String(appName || ""))
                root.enterDock()
            }
            onAppdeckRequested: {
                root.requestOpenAppDeck()
                if (root.gridAppsMode) {
                    root.enterDock()
                } else {
                    root.enterGrid()
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

        HoverHandler {
            id: bottomRevealHoldHandler
            target: dockView
            enabled: root.policyEdgeRevealEnabled && root.policyContentVisible && root.dockActive
            onHoveredChanged: {
                root.bottomRevealHeld = hovered
                if (hovered && root.shellPolicy && root.shellPolicy.requestBottomEdgeReveal) {
                    root.shellPolicy.requestBottomEdgeReveal("bottom-dock-hover")
                }
            }
        }
    }

    MouseArea {
        id: bottomRevealSensor
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: root.revealActivationHeight
        z: 50
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        enabled: root.policySensorOnly
        onEntered: {
            if (root.shellPolicy && root.shellPolicy.requestBottomEdgeReveal) {
                root.shellPolicy.requestBottomEdgeReveal("bottom-edge")
            }
        }
        onPositionChanged: {
            if (root.shellPolicy && root.shellPolicy.requestBottomEdgeReveal) {
                root.shellPolicy.requestBottomEdgeReveal("bottom-edge-motion")
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

    Timer {
        id: bottomRevealHoldTimer
        interval: 800
        repeat: true
        running: root.bottomRevealHeld
                 && root.policyEdgeRevealEnabled
                 && root.policyContentVisible
                 && root.dockActive
        onTriggered: {
            if (root.shellPolicy && root.shellPolicy.requestBottomEdgeReveal) {
                root.shellPolicy.requestBottomEdgeReveal("bottom-dock-hover-hold")
            }
        }
    }

    function syncAppDeckStateMode() {
        root.beginAppDeckLifecycle(root.state + ":" + root.mode)
        root.driveSurfaceTransition(root.surfaceTarget, false)
        root.syncLayerSurfaceSize()
        if (pulseMode || contextMode) {
            Qt.callLater(function() {
                root.focusSearchField()
            })
        }
    }

    onStateChanged: root.syncAppDeckStateMode()
    onModeChanged: root.syncAppDeckStateMode()

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
    onPolicySurfaceVisibleChanged: root.syncLayerSurfaceSize()
    onPolicyContentVisibleChanged: {
        console.info("[APPDECK] [SHELL_POLICY] contentVisible=" + root.policyContentVisible
                     + " sensorOnly=" + root.policySensorOnly
                     + " policy=" + (root.shellPolicy ? root.shellPolicy.visibilityPolicyName : "Normal"))
        root.syncAppDeckStateMode()
    }
    onPolicySensorOnlyChanged: root.syncLayerSurfaceSize()

    Connections {
        target: root.desktopScene ? root.desktopScene : null
        ignoreUnknownSignals: true
        function onAppDeckVisibleChanged() {
            Qt.callLater(function() { root.syncLayerSurfaceSize() })
        }
    }

    Connections {
        target: dockView && dockView.dockItem ? dockView.dockItem : null
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

    // Fallback: if LayerShellQt's sendExpose (and therefore the C++ eventFilter)
    // never fires within 1 s (e.g. headless or non-wlr compositor), mark the
    // bootstrap ready anyway so the dock doesn't stay invisible forever.
    Timer {
        id: layerShellFallbackTimer
        interval: 1000
        repeat: false
        onTriggered: {
            if (root.bootstrap && !root.bootstrap.readyToRender) {
                console.warn("[AppDeckWindow] layer-shell expose not received within 1 s, using fallback")
                root.bootstrap.markFirstConfigureReceived(0)
                root.bootstrap.markConfigureAcked(0)
            }
        }
    }

    Component.onCompleted: {
        console.info("[AppDeckWindow] DOCK_CREATED ptr=" + root)
        if (layerShellSupported && typeof AppDeckLayerShell !== "undefined" && AppDeckLayerShell) {
            // prepareWindow sets layer-shell properties and installs an event filter
            // that fires markFirstConfigureReceived/markConfigureAcked when
            // LayerShellQt calls sendExpose() after receiving the real configure.
            // Setting _layerPrepared immediately lets the surface be created so
            // that configure/ack cycle can proceed; opacity=0 hides content until
            // bootstrap.readyToRender becomes true.
            AppDeckLayerShell.prepareWindow(root)
            layerShellFallbackTimer.start()
        }
        _layerPrepared = true
        console.info("[APPDECK] [LAYER_STATE] policySurface=" + root.policySurfaceVisible
                     + " policyContent=" + root.policyContentVisible
                     + " layer=TopLayer")
        root.driveSurfaceTransition(root.surfaceTarget, true)
        Qt.callLater(function() { root.syncLayerSurfaceSize() })
    }

    Component.onDestruction: {
        root.endAppDeckLifecycle()
    }
}
