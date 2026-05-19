import QtQuick 2.15
import QtQuick.Window 2.15
import Slm_Desktop
import "../appdeck" as AppDeckComp
import "../appdeck/v2" as AppDeckV2
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
                                                      || !root.shellPolicy.visibilityPolicyName
                                                      || root.shellPolicy.visibilityPolicyName === "Normal"
    readonly property int revealActivationHeight: root.shellPolicy
                                                 ? Math.max(1, Math.round(root.shellPolicy.edgeRevealSize || 6))
                                                 : 6
    readonly property bool appDeckSuppressed: !rootWindow
                                              || rootWindow.lockScreenVisible
                                              || (typeof ShellStateController !== "undefined"
                                                  && ShellStateController
                                                  && ShellStateController.topOverlayActive)
                                              || !root.policySurfaceVisible
    readonly property bool policySensorOnly: !root.appDeckSuppressed
                                             && root.policyEdgeRevealEnabled
                                             && !root.policyContentVisible
                                             && root.dockActive
    property bool dockHostVisible: Number(ShellState.dockOpacity || 1.0) > 0.01
                                   && root.policyContentVisible
    property bool dockAcceptsInput: root.policyContentVisible
                                    && !(typeof ShellStateController !== "undefined"
                                         && ShellStateController
                                         && ShellStateController.topOverlayActive)
    readonly property bool powerOverlayActive: typeof ShellStateController !== "undefined"
                                               && ShellStateController
                                               && ShellStateController.topOverlayActive
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
    readonly property int motionSurfaceDuration: root.morphDuration
    readonly property int motionCrossfadeDuration: Theme.durationFast
    readonly property int motionQuickDuration: Theme.durationSm
    readonly property real sharedPanelMarginX: Math.max(28, root.width * 0.07)
    readonly property int safeCrownHeight: Math.max(36, Number(root.desktopScene ? root.desktopScene.panelHeight : 0))
    readonly property real sharedPanelTopInset: safeCrownHeight + 16
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
    readonly property color dockMorphSourceColor: {
        var c = Theme.color("windowCard")
        return Qt.rgba(c.r, c.g, c.b, Math.max(0.94, c.a))
    }

    function mixColor(a, b, t) {
        var p = Math.max(0.0, Math.min(1.0, Number(t || 0)))
        return Qt.rgba(a.r + ((b.r - a.r) * p),
                       a.g + ((b.g - a.g) * p),
                       a.b + ((b.b - a.b) * p),
                       a.a + ((b.a - a.a) * p))
    }

    readonly property bool motionEngineReady: typeof MotionController !== "undefined"
                                                && MotionController
                                                && typeof MotionController.startFromCurrent === "function"
                                                && typeof MotionController.retarget === "function"
                                                && typeof MotionController.cancelAndSettle === "function"
    readonly property real surfaceTarget: root.state === "grid" ? 1.0 : 0.0
    property real fallbackSurfaceTransition: surfaceTarget
    property real _surfaceAnimFrom: surfaceTarget
    property real _surfaceAnimTo: surfaceTarget
    property double _surfaceAnimStartMs: 0
    property int _surfaceAnimDuration: root.motionSurfaceDuration
    // Shared transition progress: 0 = dock surface, 1 = grid/pulse surface.
    readonly property real engineSurfaceTransition: motionEngineReady
                                               ? Math.max(0.0, Math.min(1.0, Number(MotionController.value || 0)))
                                               : fallbackSurfaceTransition
    readonly property real surfaceTransition: immersiveMode
                                               ? Math.max(engineSurfaceTransition,
                                                          fallbackSurfaceTransition)
                                               : fallbackSurfaceTransition
    // §28 APPDECK.md – Content Reveal Timeline stagger
    // dock→grid: 0.00–0.40 morph only, 0.40–0.70 content reveal, 0.70–1.00 settle
    // grid→dock: 0.00–0.30 content fade out, 0.30–1.00 collapse morph
    property bool _deckGoingToGrid: false
    readonly property real contentRevealOpacity: {
        var t = root.surfaceTransition
        if (root._deckGoingToGrid) {
            if (t <= 0.40) return 0.0
            if (t >= 0.70) return 1.0
            return (t - 0.40) / 0.30
        }
        if (t >= 0.70) return (t - 0.70) / 0.30
        return 0.0
    }
    readonly property real appsTransition: root.gridAppsMode
                                           ? Math.max(root.contentRevealOpacity,
                                                      root._gridContentSettled ? 1.0 : 0.0)
                                           : (root.dockActive ? root.contentRevealOpacity : 0.0)
    property real pulseTransition: (pulseMode || contextMode) ? 1.0 : 0.0
    property bool appdeckLifecycleActive: false
    property bool appdeckProfilePinned: false

    // §2 APPDECK.md – Internal Runtime State (animation locking, race-condition prevention)
    property bool transitioning: false

    // P0 — opt-in debug logs (per fix prompt §19). Forwarded into
    // AppDeckRoot.debugLogsEnabled so the v2 components share the same gate.
    // Sources: (a) `--appdeck-debug` command-line flag, (b) optional context
    // property `AppDeckDebugLogsEnabled` set by C++ from SLM_APPDECK_DEBUG
    // env var. Neither is required; both fall through cleanly to false.
    readonly property bool appDeckDebugLogsEnabled: {
        if (typeof Qt !== "undefined" && Qt.application && Qt.application.arguments) {
            var argv = Qt.application.arguments
            for (var i = 0; i < argv.length; ++i) {
                if (String(argv[i]) === "--appdeck-debug") return true
            }
        }
        if (typeof AppDeckDebugLogsEnabled !== "undefined") {
            return !!AppDeckDebugLogsEnabled
        }
        return false
    }

    // §4 APPDECK.md – Intent System
    readonly property string intent: {
        switch (root.mode) {
        case "pulse":   return "search"
        case "context": return "contextual"
        default:        return "browse"
        }
    }

    // §11 APPDECK.md – Appearance Mode
    property string appearanceMode: "always_show"

    // §25 APPDECK.md – Motion Tokens
    readonly property int morphDuration: 380
    readonly property int modeDuration: 240
    // Collapse needs to feel substantive — the previous 280 ms snapped the
    // panel down too quickly. 420 ms gives the user-perceived "settle" that
    // matches the spec §11 single-source-of-motion intent (morph + collapse
    // sharing the same curve family).
    readonly property int collapseDuration: 420
    readonly property int autoHideDelay: 2200
    readonly property int autoShowDuration: 220
    readonly property int autoHideDuration: 340
    readonly property real restingAmplitude: 0.012
    readonly property real restingFrequency: 0.48

    // §22 APPDECK.md – Spatial Memory
    property int spatialLastGridPage: 0
    property string spatialLastFocusedIconId: ""
    property string spatialLastPulseCursor: ""
    property real spatialLastGridScrollY: 0.0
    property real spatialGravityCenterX: -1.0

    // §13–15 APPDECK.md – Auto Hide Presence
    // 1.0 = fully revealed, autoHideMinPresence = low-presence (edge glow only)
    readonly property real autoHideMinPresence: 0.08
    property real autoHidePresence: 1.0

    // §27 APPDECK.md – morphProgress + dockVisible (official API surface)
    readonly property real morphProgress: root.surfaceTransition
    readonly property bool dockVisible: root.dockActive || root.surfaceTransition > 0.01

    // Derived auto-hide slide: pixels to translate dockView downward (low-presence state)
    readonly property real autoHideSlideY: {
        if (root.appearanceMode !== "auto_hide" || !root.dockActive) return 0
        var h = (dockView && dockView.dockItem) ? Number(dockView.dockItem.height || 60) : 60
        return Math.round((1.0 - root.autoHidePresence) * h * 0.88)
    }
    // Set to true in enterDock() so syncLayerSurfaceSize() applies the dock
    // input mask immediately, without waiting for the async DBus round-trip
    // that propagates dockActive = true.
    property bool _eagerDockMask: false
    // Grid input must not depend on the async ShellStateController binding.
    // Keep it explicit so the layer-shell mask expands before the first grid click.
    property bool _forceGridInputRegion: false
    // Activation/focus signals often fire as a side effect of opening the grid
    // layer itself. Arm collapse handling only after the grid has settled.
    property bool _gridCollapseGuardsArmed: false
    // Pointer dismiss must ignore the click/release that opened the grid.
    property bool _gridPointerDismissArmed: false
    // Layer-shell surfaces may never become the active window. Treat focus loss
    // as meaningful only after this surface was active while grid was open.
    property bool _gridHadActiveFocus: false
    // Safety net: the grid must become visible even if the shared motion driver
    // stalls or is temporarily owned by another shell transition.
    property bool _gridContentSettled: false
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
        root._eagerDockMask = true
        root._forceGridInputRegion = false
        root._gridCollapseGuardsArmed = false
        root._gridPointerDismissArmed = false
        root._gridHadActiveFocus = false
        root._gridContentSettled = false
        gridContentSettleTimer.stop()
        setAppDeckVisibility(false)
        if (rootWindow && rootWindow.setSearchVisible) {
            rootWindow.setSearchVisible(false)
        }
        root.syncLayerSurfaceSize()
    }

    // P0 — pointer-release one-shot: replaces the legacy ~1.8–2s
    // gridPointerDismissTimer. The click that opens the grid happens on the
    // dock button (different sub-tree from the grid's outer MouseArea), so the
    // first onClicked on the grid surface is by definition a NEW user
    // interaction. We arm on the next event-loop pass to absorb any stale
    // touch-event tail without an artificial 2-second wait.
    function _armPointerDismissOnNextTick() {
        Qt.callLater(function() {
            if (root.gridActive) {
                root._gridPointerDismissArmed = true
            }
        })
    }

    function enterGrid() {
        root._eagerDockMask = false
        root._forceGridInputRegion = true
        root._gridCollapseGuardsArmed = false
        root._gridPointerDismissArmed = false
        root._gridHadActiveFocus = !!root.active
        root._gridContentSettled = false
        if (rootWindow && rootWindow.setSearchVisible) {
            rootWindow.setSearchVisible(false)
        }
        setAppDeckVisibility(true)
        root.syncLayerSurfaceSize()
        Qt.callLater(function() { root.syncLayerSurfaceSize() })
        gridCollapseGuardTimer.restart()
        root._armPointerDismissOnNextTick()
        gridContentSettleTimer.restart()
    }

    function enterPulse() {
        root._eagerDockMask = false
        root._forceGridInputRegion = true
        root._gridCollapseGuardsArmed = false
        root._gridPointerDismissArmed = false
        root._gridHadActiveFocus = !!root.active
        root._gridContentSettled = false
        setAppDeckVisibility(false)
        if (rootWindow && rootWindow.setSearchVisible) {
            rootWindow.setSearchVisible(true)
        }
        root.syncLayerSurfaceSize()
        Qt.callLater(function() { root.syncLayerSurfaceSize() })
        gridCollapseGuardTimer.restart()
        root._armPointerDismissOnNextTick()
        gridContentSettleTimer.restart()
    }

    function shouldCollapseForActivation() {
        return root.gridActive && root._gridCollapseGuardsArmed && !root.transitioning
    }

    function canDismissGridByPointer() {
        return root.gridActive && root._gridPointerDismissArmed
    }

    function dismissGridByPointer(reason) {
        if (root.canDismissGridByPointer()) {
            root.collapseToDock(reason)
            return
        }
        console.info("[APPDECK] ignore pointer-dismiss reason=" + String(reason || "unknown")
                     + " gridActive=" + root.gridActive
                     + " pointerArmed=" + root._gridPointerDismissArmed
                     + " guards=" + root._gridCollapseGuardsArmed
                     + " transition=" + root.transitioning)
    }

    function collapseToDock(reason) {
        console.info("[APPDECK] collapseToDock reason=" + String(reason || "unknown")
                     + " active=" + root.active
                     + " gridActive=" + root.gridActive
                     + " transition=" + root.transitioning
                     + " guards=" + root._gridCollapseGuardsArmed
                     + " hadFocus=" + root._gridHadActiveFocus)
        root.enterDock()
        root.requestCollapse()
        root.syncLayerSurfaceSize()
        Qt.callLater(function() {
            root.syncLayerSurfaceSize()
        })
    }

    function hideAppDeck() {
        if (rootWindow && rootWindow.setSearchVisible) {
            rootWindow.setSearchVisible(false)
        }
        setAppDeckVisibility(false)
    }

    // §2 APPDECK.md – Transition safety lock
    function _startTransition() {
        root.transitioning = true
        transitionLockTimer.restart()
    }

    // §13–15 APPDECK.md – Auto Hide control
    function _revealDock() {
        autoHideIdleTimer.stop()
        root.autoHidePresence = 1.0
    }

    function _scheduleDockAutoHide() {
        if (root.appearanceMode !== "auto_hide") return
        autoHideIdleTimer.restart()
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

    function easeOutCubic(t) {
        var p = Math.max(0.0, Math.min(1.0, Number(t || 0)))
        var inv = 1.0 - p
        return 1.0 - (inv * inv * inv)
    }

    function startFallbackSurfaceTransition(target, immediate) {
        var t = Math.max(0.0, Math.min(1.0, Number(target || 0)))
        surfaceFallbackTimer.stop()
        root._surfaceAnimFrom = root.fallbackSurfaceTransition
        root._surfaceAnimTo = t
        root._surfaceAnimDuration = Math.max(1, Number(immediate ? 1 : root.motionSurfaceDuration))
        root._surfaceAnimStartMs = Date.now()
        if (immediate || Math.abs(root._surfaceAnimFrom - root._surfaceAnimTo) < 0.001) {
            root.fallbackSurfaceTransition = t
            return
        }
        surfaceFallbackTimer.start()
    }

    function driveSurfaceTransition(target, immediate) {
        var t = Math.max(0.0, Math.min(1.0, Number(target || 0)))
        root.startFallbackSurfaceTransition(t, immediate)
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
    readonly property bool compactDockSurface: !root.appDeckHidden
                                               && !root._forceGridInputRegion
                                               && !root.appDeckGridRequested
                                               && !root.appDeckContextRequested
    readonly property int dockSurfaceWidth: Math.max(
                                                     1,
                                                     Math.ceil(
                                                         compactDockSurface
                                                         ? (dockInputWidth + 10)
                                                         : Number(root.targetScreenGeometry.width
                                                                  || (rootWindow ? rootWindow.width : Screen.width))
                                                     )
                                                 )
    readonly property real dockTooltipHeadroom: dockView.dockItem
                                                ? Number(dockView.dockItem.tooltipHeadroom || 0)
                                                : 0
    readonly property int dockSurfaceHeight: Math.max(
                                                      1,
                                                      Math.ceil(
                                                          compactDockSurface
                                                          ? (dockInputHeight + 10 + dockTooltipHeadroom)
                                                          : root.dockContentHeight
                                                      )
                                                  )

    width: root.compactDockSurface
           ? root.dockSurfaceWidth
           : Number(root.targetScreenGeometry.width || (rootWindow ? rootWindow.width : Screen.width))
    x: root.compactDockSurface
       ? Number(root.targetScreenGeometry.x || 0)
         + Math.round((Number(root.targetScreenGeometry.width || (rootWindow ? rootWindow.width : Screen.width))
                       - root.dockSurfaceWidth) * 0.5)
       : Number(root.targetScreenGeometry.x || 0)
    y: root.compactDockSurface
       ? Number(root.targetScreenGeometry.y || 0)
         + Math.max(0,
                    Number(root.targetScreenGeometry.height || (rootWindow ? rootWindow.height : Screen.height))
                    - root.dockSurfaceHeight
                    - root.dockBottomMargin)
       : Number(root.targetScreenGeometry.y || 0)
    height: appDeckHidden
            ? 1
            : (root.compactDockSurface
               ? root.dockSurfaceHeight
               : Number(root.targetScreenGeometry.height || (rootWindow ? rootWindow.height : Screen.height)))
    Behavior on pulseTransition {
        enabled: !root.powerOverlayActive
        NumberAnimation {
            duration: root.motionSurfaceDuration
            easing.type: Theme.easingDefault
        }
    }

    Behavior on autoHidePresence {
        enabled: !root.powerOverlayActive
        NumberAnimation { duration: root.autoShowDuration; easing.type: Theme.easingDecelerate }
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

    readonly property int exclusiveZone: root.state === "dock" && root.policyContentVisible
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
    readonly property real dockLiftMax: dockView.dockItem
                                        ? Number(dockView.dockItem.liftMax || 0)
                                        : 0
    readonly property int dockVisualY: Math.max(0, dockInputY + Math.round(dockLiftMax))
    readonly property int dockVisualHeight: Math.max(1, dockInputHeight - Math.round(dockLiftMax))

    function syncLayerSurfaceSize() {
        if (typeof AppDeckLayerShell === "undefined" || !AppDeckLayerShell) {
            return
        }
        // _eagerDockMask is set by enterDock() so we immediately restrict input
        // to the dock footprint while the async DBus state catches up.
        var applyDockMask = root.compactDockSurface
                || (!root._forceGridInputRegion && (root.state === "dock" || root._eagerDockMask))
        var w = Math.max(1, Math.round(root.width))
        var h = Math.max(1, Math.round(root.appDeckHidden ? 1 : root.height))
        var surfaceX = applyDockMask ? root.dockInputX
                                     : (root.pulseMode ? root.contextSurfaceX : root.gridSurfaceX)
        var surfaceY = applyDockMask ? root.dockInputY
                                     : (root.pulseMode ? root.contextSurfaceY : root.gridSurfaceY)
        var surfaceW = applyDockMask ? root.dockInputWidth
                                     : (root.pulseMode ? root.contextSurfaceW : root.gridSurfaceW)
        var surfaceH = applyDockMask ? root.dockInputHeight
                                     : (root.pulseMode ? root.contextSurfaceH : root.gridSurfaceH)
        var dockMarginLeft = 0
        var dockMarginBottom = 0
        if (applyDockMask) {
            w = Math.max(1, Math.round(root.appDeckHidden ? 1 : root.dockSurfaceWidth))
            h = Math.max(1, Math.round(root.appDeckHidden ? 1 : root.dockSurfaceHeight))
            surfaceX = Math.max(0, Math.round((w - surfaceW) * 0.5))
            // Input region must hug the BOTTOM of the surface buffer because
            // dockBackground is anchors.bottom; the dockSurfaceHeight padding
            // (tooltipHeadroom + 10) sits ABOVE the dock pill, not below it.
            // The previous Math.min(2, ...) clamp pinned the region to the top
            // of the surface, so mouse/hover events never reached the dock
            // icons — that was Issue 2 (hover dead) in the pre-existing bug
            // list. Also fixes Issue 3 by ensuring the enlarged buffer keeps
            // input alignment with the dock pill.
            surfaceY = Math.max(0, h - surfaceH)
            var screenW = Number(root.targetScreenGeometry.width || (rootWindow ? rootWindow.width : Screen.width))
            dockMarginLeft = Math.max(0, Math.round((screenW - w) * 0.5))
            dockMarginBottom = Math.max(0, Math.round(root.dockBottomMargin))
        }
        if (root.policySensorOnly) {
            surfaceX = 0
            surfaceY = Math.max(0, h - root.revealActivationHeight)
            surfaceW = w
            surfaceH = root.revealActivationHeight
        }
        if (root.immersiveMode && !applyDockMask) {
            // §10 APPDECK.md – Outside click collapse: full-screen input so clicks on
            // wallpaper/desktop/other windows reach the background MouseArea and collapse.
            AppDeckLayerShell.setGrid(root, w, h, 0, 0, w, h)
        } else if (applyDockMask && AppDeckLayerShell.setDockAt) {
            AppDeckLayerShell.setDockAt(root, w, h,
                                        dockMarginLeft,
                                        dockMarginBottom,
                                        Math.max(0, Math.round(root.appDeckHidden ? 0 : surfaceX)),
                                        Math.max(0, Math.round(root.appDeckHidden ? 0 : surfaceY)),
                                        Math.max(1, Math.round(root.appDeckHidden ? 1 : surfaceW)),
                                        Math.max(1, Math.round(root.appDeckHidden ? 1 : surfaceH)))
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
        interval: 160
        repeat: true
        running: root.visible && root.layerShellSupported
        onTriggered: root.syncLayerSurfaceSize()
    }

    // Fire once after the C++ 50ms geometry-grace period so the input mask
    // is applied before the user can plausibly click the dock.
    Timer {
        id: postGraceSyncTimer
        interval: 60
        repeat: false
        onTriggered: root.syncLayerSurfaceSize()
    }

    Timer {
        id: surfaceFallbackTimer
        interval: 16
        repeat: true
        onTriggered: {
            var elapsed = Date.now() - root._surfaceAnimStartMs
            var progress = Math.max(0.0, Math.min(1.0, elapsed / Math.max(1, root._surfaceAnimDuration)))
            var eased = root.easeOutCubic(progress)
            root.fallbackSurfaceTransition = root._surfaceAnimFrom
                    + ((root._surfaceAnimTo - root._surfaceAnimFrom) * eased)
            if (progress >= 1.0) {
                root.fallbackSurfaceTransition = root._surfaceAnimTo
                root.logRuntimeState("surface-fallback-complete")
                stop()
            }
        }
    }

    onSceneGraphInitialized: root.syncLayerSurfaceSize()
    onAppDeckHiddenChanged: root.syncLayerSurfaceSize()
    onImmersiveModeChanged: root.syncLayerSurfaceSize()
    onPulseModeChanged: root.syncLayerSurfaceSize()
    onDockActiveChanged: {
        if (root.dockActive) {
            root._eagerDockMask = false
        }
        root.syncLayerSurfaceSize()
    }
    onDockLayerReadyChanged: {
        if (root.dockLayerReady) {
            root.syncLayerSurfaceSize()
            postGraceSyncTimer.restart()
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

        // AppDeck v2 root (docs/APPDECK.md §4, §16). Tahap 1: present as the
        // SSOT for deckState / deckMode / morphProgress / transitioning, but
        // for now mirrored from the legacy inline state so behavior is
        // unchanged. Later Tahap will invert the dependency so children read
        // from appDeckRoot directly.
        AppDeckV2.AppDeckRoot {
            id: appDeckRoot
            deckState:      root.state
            deckMode:       root.mode
            appearanceMode: root.appearanceMode
            transitioning:  root.transitioning
            morphProgress:  root.surfaceTransition
            dockPresence:   root.autoHidePresence
            geometry:       appDeckGeometry
            // P0 — propagate debug gate. Until C++ wires SLM_APPDECK_DEBUG into
            // a context property, this stays driven from the legacy probe
            // (set by AppDeckWindow.qml externally — currently a constant
            // false; flip via the runtime override below).
            debugLogsEnabled: root.appDeckDebugLogsEnabled
        }

        // P0 — react to AppDeckRoot signals. `hoverRecomputeRequested` is
        // emitted whenever a transition ends (real or watchdog-forced) so a
        // stationary pointer still resolves to the right item. `watchdogReset`
        // clears `root.transitioning` if it's somehow still latched (the
        // SSOT direction is currently outside-in, so the v2 watchdog signals
        // upward instead of writing directly).
        Connections {
            target: appDeckRoot
            ignoreUnknownSignals: true
            function onHoverRecomputeRequested() {
                Qt.callLater(function() {
                    if (appDeckController && appDeckController.recomputeHover) {
                        appDeckController.recomputeHover()
                    }
                })
            }
            function onWatchdogReset() {
                if (root.transitioning) {
                    console.warn("[APPDECK] watchdog reset from AppDeckRoot")
                    root.transitioning = false
                }
            }
            function onMorphFinished(finalState) {
                root.syncLayerSurfaceSize()
                Qt.callLater(function() { root.syncLayerSurfaceSize() })
            }
        }

        // P0 — future v2-API call sites (keyboard shortcut, etc.) route
        // through AppDeckMotion.toggleAppDeck() / interruptOrReverseTransition,
        // which emit these signals. The legacy state machine on AppDeckWindow
        // dispatches them into enterGrid / enterDock. Once AppDeckRoot owns
        // morphProgress directly, this bridge can be removed.
        Connections {
            target: appDeckMotion
            ignoreUnknownSignals: true
            function onToggleRequested(reason) {
                if (root.transitioning) {
                    var reverseTarget = root.surfaceTransition >= 0.5 ? "dock" : "grid"
                    if (reverseTarget === "dock") root.enterDock(); else root.enterGrid()
                    return
                }
                if (root.gridAppsMode) root.enterDock(); else root.enterGrid()
            }
            function onReverseRequested(targetState) {
                if (targetState === "dock") root.enterDock(); else root.enterGrid()
            }
        }

        AppDeckV2.AppDeckController {
            id: appDeckController
            root:         appDeckRoot
            rootWindow:   root.rootWindow
            desktopScene: root.desktopScene
        }

        // docs/APPDECK.md §10 — Motion API. Legacy collapseToDock() / enterGrid()
        // on AppDeckWindow remain the active drivers for now; AppDeckMotion is
        // available for future call sites that want the spec API.
        AppDeckV2.AppDeckMotion {
            id: appDeckMotion
            root:     appDeckRoot
            geometry: appDeckGeometry
        }

        // docs/APPDECK.md §17 — Ctrl+Alt+D debug HUD. Default hidden; opt-in
        // toggle keeps prod surfaces clean while giving agents/humans a quick
        // visual readout that morphProgress is genuinely ticking 0→1.
        AppDeckV2.AppDeckDebugOverlay {
            id: appDeckDebugOverlay
            appDeckRoot: appDeckRoot
            geometry:    appDeckGeometry
            controller:  appDeckController
            inputX:      root.dockInputX
            inputY:      root.dockInputY
            inputW:      root.dockInputWidth
            inputH:      root.dockInputHeight
        }

        // docs/APPDECK.md §5 — geometry model (dock/grid rects + per-icon
        // anchors). Wired in Tahap 2; consumed by IconMorphLayer in Tahap 4.
        // dockHostRef / gridHostRef are forward-referenced to siblings below.
        AppDeckV2.AppDeckGeometry {
            id: appDeckGeometry
            dockHostRef:    dockView.dockItem
            gridHostRef:    gridAppsView
            rootSurfaceRef: appDeckRoot
        }

        Connections {
            // Re-emit dock-icon-rect events into the geometry model. AppDeck.qml
            // already debounces, so this just forwards.
            target: dockView && dockView.dockItem ? dockView.dockItem : null
            ignoreUnknownSignals: true
            function onIconRectsChanged(rects) {
                appDeckGeometry.applyDockIconRects(rects)
            }
        }

        Connections {
            target: gridAppsView
            ignoreUnknownSignals: true
            function onGridLayoutSettled() {
                appDeckGeometry.recompute()
            }
        }

        // Recompute dock/grid rects when the surface resizes.
        Connections {
            target: root
            function onWidthChanged()  { appDeckGeometry.recompute() }
            function onHeightChanged() { appDeckGeometry.recompute() }
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

        // §10 APPDECK.md – Catches outside-panel clicks in pulse/context mode.
        // In gridAppsMode the gridAppsView background MouseArea (z=1) handles this;
        // here we cover the pulse/context case where contextView has no background handler.
        MouseArea {
            anchors.fill: parent
            z: 0.1
            enabled: root.immersiveMode
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: {
                root.dismissGridByPointer("outside-click")
            }
        }

        // docs/APPDECK.md §6, §11 — MorphContainer drives x/y/w/h/radius/color
        // from a single morphProgress, with NO per-property Behavior blocks.
        // Children (gradient highlight + top seam) ride along declaratively.
        AppDeckV2.MorphContainer {
            id: surfaceMorph
            z: 0
            visible: !root.appDeckHidden && root.policyContentVisible
            opacity: (!root.appDeckHidden && root.policyContentVisible) ? 1.0 : 0.0
            border.width: Theme.borderWidthThin
            border.color: Theme.color("panelBorder")

            // pulse/context vs grid choose different target geometry; dock is
            // always the source. morphProgress is the SSOT for blending.
            sourceRect: Qt.rect(Number(root.dockInputX || 0),
                                Number(root.dockVisualY || 0),
                                Number(root.dockInputWidth || 1),
                                Number(root.dockVisualHeight || 1))
            targetRect: pulseMode
                        ? Qt.rect(root.contextSurfaceX, root.contextSurfaceY,
                                  root.contextSurfaceW, root.contextSurfaceH)
                        : Qt.rect(root.gridSurfaceX, root.gridSurfaceY,
                                  root.gridSurfaceW, root.gridSurfaceH)
            sourceRadius: Theme.radiusWindow
            targetRadius: Theme.radiusWindow + Math.min(8, Theme.radiusWindow)
            sourceColor: root.dockMorphSourceColor
            targetColor: Theme.color("windowCard")
            morphProgress: root.surfaceTransition

            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                opacity: (1.0 - root.surfaceTransition) * 0.14
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, Theme.darkMode ? 0.16 : 0.22) }
                    GradientStop { position: 1.0; color: Qt.rgba(1, 1, 1, 0.0) }
                }
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
        }

        // docs/APPDECK.md §7 — IconMorphLayer paints icons in surface space,
        // one delegate per app, flying dockAnchor → gridAnchor under
        // morphProgress. Gated by AppDeckTokens.iconMorphEnabled (default OFF
        // until QEMU validation completes); legacy dock+grid icons keep
        // rendering when the flag is off.
        AppDeckV2.IconMorphLayer {
            id: iconMorphLayer
            z: 0.5
            appsModel:     root.appsModel
            geometry:      appDeckGeometry
            tokens:        appDeckRoot.tokens
            morphProgress: appDeckRoot.morphProgress
            enabledLayer:  appDeckRoot.tokens.iconMorphEnabled
            onAppActivated: function(d) {
                if (d && d.executable) {
                    root.requestOpenApp(String(d.executable))
                }
                root.collapseToDock("icon-morph-launch")
            }
        }

        AppDeckComp.AppDeckGridAppsView {
            id: gridAppsView
            anchors.fill: parent
            // docs/APPDECK.md §17 — Grid stays mounted as a backdrop while
            // Pulse mode is active. The inline backdrop dim/scale lives
            // inside this component (driven by its searchFocused/searchActive
            // state), so the outer opacity stays at 1.0 during pulse and only
            // the inner backdropOpacity contributes the 0.88–0.92 dim.
            visible: root.gridAppsMode || root.pulseMode || opacity > 0.01
            opacity: root.gridAppsMode
                     ? root.appsTransition
                     : (root.pulseMode ? 1.0 : 0.0)
            transform: Translate {
                y: root.gridAppsMode ? (1.0 - root.appsTransition) * 14 : 0
            }
            z: 1
            appsModel: root.appsModel
            desktopScene: root.desktopScene
            iconsRenderedExternally: appDeckRoot.tokens.iconMorphEnabled
            morphProgress: appDeckRoot.morphProgress
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
                root.collapseToDock("grid-dismiss")
            }
            onPointerDismissRequested: {
                root.dismissGridByPointer("grid-pointer-dismiss")
            }
            onAppChosen: function(appData) {
                console.log("[appdeck-launch] window app chosen name="
                            + String(appData && (appData.name || appData.display) || "")
                            + " desktopFile=" + String(appData && appData.desktopFile || "")
                            + " executable=" + String(appData && appData.executable || ""))
                root.collapseToDock("grid-app-chosen")
                root.requestOpenApp(String(appData && (appData.desktopId || appData.desktopFile || appData.name) || ""))
                AppDeckActions.handleAppChosen(appData,
                                               (typeof AppCommandRouter !== "undefined" && AppCommandRouter)
                                                   ? AppCommandRouter : null,
                                               (typeof AppExecutionGate !== "undefined" && AppExecutionGate)
                                                   ? AppExecutionGate : null)
            }
            onAddToDockRequested: function(appData) {
                AppDeckActions.handleAddToDock(appData, root.appsModel)
            }
            onAddToDesktopRequested: function(appData) {
                AppDeckActions.handleAddToDesktop(appData, root.desktopScene)
            }
            // docs/APPDECK.md §17 — inline grid search drives Pulse activation.
            // Non-empty text flips deckMode to pulse and seeds the pulse
            // query; empty text returns to apps mode (still in grid state,
            // not dock — focus stays on the inline search field).
            onPulseQueryChanged: function(query) {
                var trimmed = String(query || "").trim()
                if (!root.rootWindow) {
                    return
                }
                if (trimmed.length > 0) {
                    if (root.rootWindow.setSearchVisible) {
                        root.rootWindow.setSearchVisible(true)
                    }
                    root.applyPulseQuery(query)
                } else {
                    if (root.rootWindow.setSearchVisible) {
                        root.rootWindow.setSearchVisible(false)
                    }
                    root.applyPulseQuery("")
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
                root.collapseToDock("context-collapse")
            }
            onDismissRequested: {
                root.collapseToDock("context-dismiss")
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
            // In compactDockSurface (dock-only mode), the Wayland layer-shell
            // already positions the surface dockBottomMargin pixels above the
            // screen edge via setDockAt margins. Repeating the same value as
            // Qt anchor margin would double-inset the dock — causing it to
            // shift UP relative to its grid-state position. Use 0 here when
            // the wayland-side margin is applied, dockBottomMargin otherwise
            // (grid/full-screen, where Window sits at screen.bottom).
            anchors.bottomMargin: root.compactDockSurface ? 0 : root.dockBottomMargin
            visible: !root.appDeckHidden && (root.dockActive || root.gridAppsMode || root.pulseMode || opacity > 0.01)
            opacity: 1.0
            scale: 1.0
            transform: Translate { y: root.autoHideSlideY }
            z: 3
            hostName: "appdeck"
            // Hide dock pill border when AppDeck is in grid/pulse/context mode
            // so the dock visually merges into the morph panel instead of
            // showing a hard outline against the expanded surface.
            hideBorder: !root.dockActive
            transparentBackground: false
            ignoreDesktopTransparentSetting: true
            backgroundMorphProgress: root.surfaceTransition
            iconsRenderedExternally: appDeckRoot.tokens.iconMorphEnabled
            // Software OpenGL (QEMU smoke) cannot reliably render layer +
            // MultiEffect on the dock pill — DockEffectsEnabled is wired in
            // main.cpp and flipped off automatically under LIBGL_ALWAYS_SOFTWARE
            // so the dock background stays visible.
            renderEffectsEnabled: (typeof DockEffectsEnabled !== "undefined")
                                  ? DockEffectsEnabled : true
            // Tahap 5 hook — passes morphProgress down so future GridContent
            // consumers can stagger reveal off it. Dock chrome itself ignores
            // this; the dock has no spec'd reveal staging.
            acceptsInput: root.dockAcceptsInput && root.dockActive
            rendererActive: root.visible && root.dockLayerReady && root.dockHostVisible
                            && !root.appDeckHidden
            appsModel: root.appsModel
            onAppActivated: function(appName) {
                // AppDeck._focusOrLaunchEntry already handled focus-or-launch
                // via compositorState.preferredViewId / AppCommandRouter.
                // This signal is only for collapsing the deck.
                root.collapseToDock("dock-app-activated")
            }
            onAppdeckRequested: {
                console.info("[APPDECK-LAUNCHER-SIGNAL] reached AppDeckWindow"
                             + " gridAppsMode=" + root.gridAppsMode
                             + " state=" + root.state
                             + " transitioning=" + root.transitioning
                             + " surfaceTransition=" + Number(root.surfaceTransition).toFixed(3)
                             + " active=" + root.active)
                root.requestOpenAppDeck()
                // P0 — spec §4/§5: clicks during a morph reverse direction.
                // Without this, a click mid-expand is silently dropped while
                // `transitioning=true` and the user perceives the deck as
                // unresponsive. Direction inferred from morph progress.
                if (root.transitioning) {
                    var reverseTarget = root.surfaceTransition >= 0.5 ? "dock" : "grid"
                    console.info("[APPDECK-LAUNCHER-SIGNAL] interrupt reverse target=" + reverseTarget)
                    if (reverseTarget === "dock") {
                        root.enterDock()
                    } else {
                        root.enterGrid()
                    }
                    return
                }
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
            enabled: !root.powerOverlayActive
                     && root.policyEdgeRevealEnabled
                     && root.policyContentVisible
                     && root.dockActive
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
        enabled: !root.powerOverlayActive && root.policySensorOnly
        onEntered: {
            if (root.shellPolicy && root.shellPolicy.requestBottomEdgeReveal) {
                root.shellPolicy.requestBottomEdgeReveal("bottom-edge")
            }
            if (root.appearanceMode === "auto_hide" && root.autoHidePresence < 1.0) {
                root._revealDock()
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

    // §2 APPDECK.md – Watchdog/cleanup for `transitioning`. Bumped to
    // morphDuration + 250 ms so it can't fire before morphAnim.onStopped in
    // the v2 path. Restart is driven by `onTransitioningChanged` (rising
    // edge), making it a universal cleanup regardless of which path set
    // transitioning=true.
    Timer {
        id: transitionLockTimer
        interval: root.morphDuration + 250
        repeat: false
        onTriggered: {
            if (root.transitioning) {
                console.warn("[APPDECK] transitionLockTimer reset"
                             + " surfaceTransition=" + Number(root.surfaceTransition).toFixed(3))
                root.transitioning = false
            }
        }
    }

    onTransitioningChanged: {
        if (transitioning) {
            transitionLockTimer.restart()
        } else {
            transitionLockTimer.stop()
            // Falling edge — let the layer-shell input region catch up with
            // the new state, then re-emit hover so a stationary pointer
            // still ends up over the right item.
            root.syncLayerSurfaceSize()
            Qt.callLater(function() { root.syncLayerSurfaceSize() })
        }
    }

    // §13–15 APPDECK.md – Auto hide idle timer
    Timer {
        id: autoHideIdleTimer
        interval: root.autoHideDelay
        repeat: false
        onTriggered: {
            if (root.appearanceMode === "auto_hide"
                    && root.dockActive
                    && !(typeof AppDeckController !== "undefined"
                         && AppDeckController && AppDeckController.dockHovered)) {
                root.autoHidePresence = root.autoHideMinPresence
            }
        }
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
                 && !root.powerOverlayActive
                 && root.policyEdgeRevealEnabled
                 && root.policyContentVisible
                 && root.dockActive
        onTriggered: {
            if (root.shellPolicy && root.shellPolicy.requestBottomEdgeReveal) {
                root.shellPolicy.requestBottomEdgeReveal("bottom-dock-hover-hold")
            }
        }
    }

    Timer {
        id: gridCollapseGuardTimer
        interval: Math.max(root.motionSurfaceDuration, 220)
        repeat: false
        onTriggered: {
            if (root.gridActive) {
                root._gridCollapseGuardsArmed = true
            }
        }
    }

    // P0 — `gridPointerDismissTimer` removed. Replaced by the one-shot
    // `_armPointerDismissOnNextTick()` call inside enterGrid()/enterPulse()
    // (see ~line 320). The 1.8–2 s gate it implemented was the visible cause
    // of the "responsive only on fast click" symptom: any outside-content
    // click during that window was silently dropped.

    Timer {
        id: gridContentSettleTimer
        interval: Math.max(root.morphDuration + 180, 560)
        repeat: false
        onTriggered: {
            if (!root.gridActive) {
                return
            }
            if (root.gridAppsMode) {
                root._gridContentSettled = true
            }
            if (root.motionEngineReady) {
                root.syncAppDeckMotionChannel()
                MotionController.cancelAndSettle(1.0)
            }
            if (root.fallbackSurfaceTransition < 0.98) {
                root.startFallbackSurfaceTransition(1.0, false)
            }
            root.logRuntimeState("settle")
        }
    }

    function logRuntimeState(reason) {
        console.info("[APPDECK] [RUNTIME_STATE]"
                     + " reason=" + String(reason || "unknown")
                     + " state=" + root.state
                     + " mode=" + root.mode
                     + " gridActive=" + root.gridActive
                     + " gridRequested=" + root.appDeckGridRequested
                     + " policy=" + (root.shellPolicy ? root.shellPolicy.visibilityPolicyName : "Normal")
                     + " policyContent=" + root.policyContentVisible
                     + " hidden=" + root.appDeckHidden
                     + " contentOpacity=" + Number(root.contentOpacity).toFixed(2)
                     + " surfaceTarget=" + Number(root._surfaceAnimTo).toFixed(2)
                     + " surfaceTransition=" + Number(root.surfaceTransition).toFixed(2)
                     + " appsTransition=" + Number(root.appsTransition).toFixed(2)
                     + " forceGridInput=" + root._forceGridInputRegion
                     + " dockInput=" + root.dockInputX + "," + root.dockInputY
                     + " " + root.dockInputWidth + "x" + root.dockInputHeight
                     + " guards=" + root._gridCollapseGuardsArmed
                     + " pointerArmed=" + root._gridPointerDismissArmed)
    }

    function syncAppDeckStateMode() {
        var target = root.state === "grid" ? 1.0 : 0.0
        root._startTransition()
        root.beginAppDeckLifecycle(root.state + ":" + root.mode)
        root.driveSurfaceTransition(target, false)
        root.syncLayerSurfaceSize()
        root.logRuntimeState("sync")
        Qt.callLater(function() {
            root.logRuntimeState("post-sync")
        })
        if (pulseMode || contextMode) {
            Qt.callLater(function() {
                root.focusSearchField()
            })
        }
    }

    onStateChanged: {
        root._deckGoingToGrid = (root.state === "grid")
        if (root.state === "grid") {
            root.startFallbackSurfaceTransition(1.0, false)
            root._eagerDockMask = false
            root._forceGridInputRegion = true
            root._gridCollapseGuardsArmed = false
            root._gridPointerDismissArmed = false
            root._gridHadActiveFocus = !!root.active
            root._gridContentSettled = false
            gridCollapseGuardTimer.restart()
            gridPointerDismissTimer.restart()
            gridContentSettleTimer.restart()
        } else if (root.state === "dock") {
            root.startFallbackSurfaceTransition(0.0, false)
            root._forceGridInputRegion = false
            root._gridCollapseGuardsArmed = false
            root._gridPointerDismissArmed = false
            root._gridHadActiveFocus = false
            root._gridContentSettled = false
            gridCollapseGuardTimer.stop()
            gridPointerDismissTimer.stop()
            gridContentSettleTimer.stop()
            root._revealDock()
            root._scheduleDockAutoHide()
        }
        root.syncAppDeckStateMode()
    }
    onModeChanged: root.syncAppDeckStateMode()
    onAppDeckGridRequestedChanged: {
        root._forceGridInputRegion = root.appDeckGridRequested
        if (root.appDeckGridRequested) {
            root._eagerDockMask = false
        }
        root.syncLayerSurfaceSize()
    }

    // §8 APPDECK.md – Focus loss collapse: when the compositor deactivates this
    // layer-shell surface (e.g., user clicks an app window), collapse to dock.
    onActiveChanged: {
        if (root.active && root.gridActive) {
            root._gridHadActiveFocus = true
        }
        if (!root.active && root._gridHadActiveFocus && root.shouldCollapseForActivation()) {
            Qt.callLater(function() {
                if (!root.active && root._gridHadActiveFocus && root.shouldCollapseForActivation()) {
                    root.collapseToDock("focus-lost")
                }
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
        target: (typeof WindowingBackend !== "undefined") ? WindowingBackend : null
        ignoreUnknownSignals: true
        function onEventReceived(event, payload) {
            var eventName = String(event || (payload && payload.event) || "")
            eventName = eventName.toLowerCase().replace(/_/g, "-")
            if (eventName === "window-focused"
                    || eventName === "window-activated"
                    || eventName === "active-window-changed"
                    || eventName === "focus-changed"
                    || eventName === "window-created"
                    || eventName === "window-opened"
                    || eventName === "window-shown") {
                if (root.shouldCollapseForActivation()) {
                    root.collapseToDock("window-event")
                }
            }
        }
    }

    // §13–15 APPDECK.md – Reveal dock when hovered, schedule hide when hover leaves
    Connections {
        target: (typeof AppDeckController !== "undefined") ? AppDeckController : null
        ignoreUnknownSignals: true
        function onDockHoveredChanged() {
            if (AppDeckController.dockHovered) {
                if (root.appearanceMode === "auto_hide") {
                    root._revealDock()
                }
            } else if (root.dockActive) {
                root._scheduleDockAutoHide()
            }
        }
    }

    property int _lastRunningCount: 0
    Connections {
        target: (typeof AppStateClient !== "undefined") ? AppStateClient : null
        ignoreUnknownSignals: true
        function onFocusedAppIdChanged() {
            // Diagnostic for Issue 4 (dock doesn't collapse when Settings is
            // launched from crown menu). When the user reports this, the QEMU
            // log answers whether the path fires at all and what guards block.
            console.info("[APPDECK] focusedAppId="
                         + (AppStateClient ? String(AppStateClient.focusedAppId || "") : "n/a")
                         + " gridActive=" + root.gridActive
                         + " guards=" + root._gridCollapseGuardsArmed
                         + " transitioning=" + root.transitioning
                         + " shouldCollapse=" + root.shouldCollapseForActivation())
            if (root.shouldCollapseForActivation()) {
                root.collapseToDock("focused-app")
            }
        }
        function onRunningAppsChanged() {
            var newCount = (typeof AppStateClient !== "undefined" && AppStateClient)
                           ? Number(AppStateClient.runningAppIds.length || 0) : 0
            if (root.shouldCollapseForActivation() && newCount > root._lastRunningCount) {
                root.collapseToDock("running-app-added")
            }
            root._lastRunningCount = newCount
        }
    }

    // §8/§9 APPDECK.md – collapse on app activation from any source:
    // window click, dock icon, crown, keyboard shortcut.
    Connections {
        target: (typeof GlobalMenuManager !== "undefined") ? GlobalMenuManager : null
        ignoreUnknownSignals: true
        function onAppSwitched() {
            if (root.shouldCollapseForActivation()) {
                root.collapseToDock("global-menu-app-switched")
            }
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
