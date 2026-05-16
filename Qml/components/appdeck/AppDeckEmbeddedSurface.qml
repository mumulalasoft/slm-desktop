import QtQuick 2.15
import QtQuick.Effects
import Slm_Desktop
import "." as AppDeckComp
import "../overlay/AppDeckActions.js" as AppDeckActions
import "../shell/PulseController.js" as PulseController

Item {
    id: root

    required property var rootWindow
    required property var desktopScene
    required property var appsModel
    property var pulseResultsModel: null
    property bool dockHostVisible: true
    property bool dockAcceptsInput: true
    property bool safeRendering: false

    signal requestCollapse()
    signal requestOpenApp(string appId)
    signal requestOpenAppDeck()

    readonly property var dockItem: dockView.dockItem
    readonly property bool rootWindowVisible: !!rootWindow && !!rootWindow.visible
    readonly property bool rootWindowLocked: !!rootWindow && !!rootWindow.lockScreenVisible
    readonly property bool appDeckHidden: !root.enabled || root.rootWindowLocked
    readonly property bool appDeckGridRequested: {
        if (root.appDeckHidden) {
            return false
        }
        if (root.rootWindow && root.rootWindow.searchVisible === true) {
            return true
        }
        if (typeof ShellStateController !== "undefined"
                && ShellStateController) {
            return ShellStateController.appdeckVisible === true
        }
        return root.desktopScene ? root.desktopScene.appdeckVisible === true : false
    }
    readonly property bool appDeckContextRequested: !root.appDeckHidden
                                                    && typeof AppDeckController !== "undefined"
                                                    && AppDeckController
                                                    && String(AppDeckController.contextItemId || "").length > 0
    state: root.appDeckGridRequested || root.appDeckContextRequested ? "grid" : "dock"
    property string mode: {
        if (root.state === "dock") {
            return "apps"
        }
        if (root.rootWindow && root.rootWindow.searchVisible === true) {
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
    readonly property real surfaceTransition: immersiveMode ? 1.0 : 0.0
    property real appsTransition: gridAppsMode ? 1.0 : 0.0
    property real pulseTransition: (pulseMode || contextMode) ? 1.0 : 0.0
    property bool appdeckLifecycleActive: false
    property bool appdeckProfilePinned: false
    property string appdeckPrevChannel: ""
    property string appdeckPrevPreset: ""

    readonly property int motionSurfaceDuration: Theme.durationNormal
    readonly property int motionCrossfadeDuration: Theme.durationFast
    readonly property real sharedPanelMarginX: Math.max(28, root.width * 0.07)
    readonly property real sharedPanelTopInset: Math.max(
                                                    18,
                                                    Number(root.desktopScene ? root.desktopScene.panelHeight : 0) + 14)
    readonly property real dockBottomMargin: Math.max(
                                                         8,
                                                         Number(root.desktopScene ? root.desktopScene.dockBottomMargin : 0))
    readonly property real sharedPanelBottomInset: Math.max(
                                                       24,
                                                       Math.round(
                                                           (dockView.dockItem
                                                            ? Number(dockView.dockItem.height || 120)
                                                            : 120)
                                                           + root.dockBottomMargin
                                                           + 20))
    readonly property real sharedDockWidth: Math.max(320, Number(root.dockInputWidth || 1) + 20)
    readonly property real gridContentW: Math.min(1180, Math.max(320, root.width - (sharedPanelMarginX * 2)))
    readonly property real gridContentH: Math.min(
                                                940,
                                                Math.max(360,
                                                         root.height - sharedPanelTopInset - sharedPanelBottomInset))
    readonly property real gridContentX: Math.round((root.width - gridContentW) * 0.5)
    readonly property real gridContentY: sharedPanelTopInset
    readonly property real gridSurfaceW: Math.max(gridContentW, sharedDockWidth)
    readonly property real gridSurfaceH: Math.max(gridContentH, root.height - gridContentY - 8)
    readonly property real gridSurfaceX: Math.round((root.width - gridSurfaceW) * 0.5)
    readonly property real gridSurfaceY: gridContentY
    readonly property real contextContentW: Math.min(980, Math.max(620, root.width - 80))
    readonly property real contextContentY: Math.max(
                                                16,
                                                Number(root.desktopScene ? root.desktopScene.panelHeight : 0) + 16)
    readonly property real contextContentH: Math.max(420, root.height - (contextContentY + 40))
    readonly property real contextContentX: Math.round((root.width - contextContentW) * 0.5)
    readonly property real contextSurfaceW: Math.max(contextContentW, sharedDockWidth)
    readonly property real contextSurfaceH: Math.max(contextContentH, root.height - contextContentY - 8)
    readonly property real contextSurfaceX: Math.round((root.width - contextSurfaceW) * 0.5)
    readonly property real contextSurfaceY: contextContentY
    readonly property int dockInputX: Math.max(
                                           0,
                                           Math.round(
                                               (dockView ? Number(dockView.x || 0) : 0)
                                               + (dockView.dockItem
                                                  ? Number(dockView.dockItem.x || 0)
                                                    + Number(dockView.dockItem.inputRegionX || 0)
                                                  : 0)))
    readonly property int dockInputY: Math.max(
                                           0,
                                           Math.round(
                                               (dockView ? Number(dockView.y || 0) : 0)
                                               + (dockView.dockItem
                                                  ? Number(dockView.dockItem.y || 0)
                                                    + Number(dockView.dockItem.inputRegionY || 0)
                                                  : 0)))
    readonly property int dockInputWidth: Math.max(
                                               1,
                                               Math.round(
                                                   dockView.dockItem
                                                   ? Number(dockView.dockItem.inputRegionWidth
                                                            || dockView.dockItem.width || 1)
                                                   : 1))
    readonly property int dockInputHeight: Math.max(
                                                1,
                                                Math.round(
                                                    dockView.dockItem
                                                    ? Number(dockView.dockItem.inputRegionHeight
                                                             || dockView.dockItem.height || 1)
                                                    : 1))

    visible: root.enabled
             && root.dockHostVisible
             && root.rootWindowVisible
             && !root.appDeckHidden

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

    property real morphProgress: root.immersiveMode ? 1.0 : 0.0
    Behavior on morphProgress {
        NumberAnimation {
            duration: root.motionSurfaceDuration
            easing.type: Theme.easingDefault
        }
    }

    function setAppDeckVisibility(visible) {
        var v = !!visible
        if (typeof ShellStateController !== "undefined"
                && ShellStateController
                && ShellStateController.setAppDeckVisible) {
            ShellStateController.setAppDeckVisible(v)
            if (!v && ShellStateController.setAppDeckSearchSeed) {
                ShellStateController.setAppDeckSearchSeed("")
            }
            return
        }
        if (root.desktopScene && root.desktopScene.setAppDeckVisible) {
            root.desktopScene.setAppDeckVisible(v)
        }
    }

    function enterDock() {
        setAppDeckVisibility(false)
        if (root.rootWindow && root.rootWindow.setSearchVisible) {
            root.rootWindow.setSearchVisible(false)
        }
    }

    function enterGrid() {
        if (root.rootWindow && root.rootWindow.setSearchVisible) {
            root.rootWindow.setSearchVisible(false)
        }
        setAppDeckVisibility(true)
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
        if (typeof MotionController === "undefined" || !MotionController
                || !MotionController.endLifecycleTransition) {
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

    Rectangle {
        anchors.fill: parent
        visible: root.morphProgress > 0.001
        color: Theme.color("screenshotScrim")
        opacity: root.morphProgress
        z: 0
    }

    Rectangle {
        id: morphPanel
        z: 0.5
        visible: true

        readonly property real t: root.morphProgress
        readonly property real dockH: dockView.dockItem ? Number(dockView.dockItem.baseHeight || 72) : 72
        readonly property real dockW: root.dockInputWidth
        readonly property real dockX: Math.round((root.width - dockW) / 2)
        readonly property real dockY: root.height - root.dockBottomMargin - dockH
        readonly property real panelX: root.pulseMode ? root.contextSurfaceX : root.gridSurfaceX
        readonly property real panelY: root.pulseMode ? root.contextSurfaceY : root.gridSurfaceY
        readonly property real panelW: root.pulseMode ? root.contextSurfaceW : root.gridSurfaceW
        readonly property real panelH: root.pulseMode ? root.contextSurfaceH : root.gridSurfaceH

        x: Math.round(dockX + (panelX - dockX) * t)
        y: Math.round(dockY + (panelY - dockY) * t)
        width: Math.round(dockW + (panelW - dockW) * t)
        height: Math.round(dockH + (panelH - dockH) * t)
        radius: Math.round(Theme.radiusWindow + Math.min(8, Theme.radiusWindow) * t)
        color: Theme.color("windowCard")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("panelBorder")
        layer.enabled: visible && !root.safeRendering
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Qt.rgba(0, 0, 0, Theme.darkMode ? 0.42 : 0.24)
            shadowBlur: 0.54
            shadowVerticalOffset: 10
            shadowHorizontalOffset: 0
        }

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            opacity: 1.0 - morphPanel.t
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.color("dockGlassTop") }
                GradientStop { position: 1.0; color: Theme.color("dockGlassBottom") }
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
            opacity: morphPanel.t
        }
    }

    AppDeckComp.AppDeckGridAppsView {
        id: gridAppsView
        anchors.fill: parent
        visible: root.gridAppsMode || opacity > 0.01
        opacity: root.appsTransition
        transform: Translate { y: (1.0 - root.appsTransition) * 14 }
        revealProgress: root.morphProgress
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
        bottomSafeInset: root.sharedPanelBottomInset
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
        appModelRef: (typeof AppModel !== "undefined" && AppModel)
                     ? AppModel
                     : (root.rootWindow ? root.rootWindow.appModelRef : null)
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
            if (!root.rootWindow || !root.pulseResultsModel || !root.pulseResultsModel.get) {
                return
            }
            var rid = String(resultId || "")
            if (rid.length <= 0) {
                return
            }
            var indexValue = -1
            var count = Number(root.pulseResultsModel.count || 0)
            for (var i = 0; i < count; ++i) {
                var row = root.pulseResultsModel.get(i)
                if (!row) {
                    continue
                }
                var rowId = String(row.resultId || row.desktopId || row.desktopFile
                                   || row.path || row.actionId || row.name || ("row-" + i))
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
            if (root.rootWindow) {
                PulseController.activateResult(root.rootWindow, root.pulseResultsModel, indexValue)
            }
        }
        onResultContextAction: function(indexValue, action) {
            if (root.rootWindow) {
                PulseController.activateContextAction(root.rootWindow,
                                                      root.pulseResultsModel,
                                                      indexValue,
                                                      action)
            }
        }
    }

    AppDeckComp.AppDeckCollapsedView {
        id: dockView
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.dockBottomMargin
        visible: !root.appDeckHidden
        opacity: 1.0
        scale: 1.0 - (0.03 * root.morphProgress)
        transform: Translate { y: root.morphProgress * 10 }
        z: 3
        hostName: "appdeck-embedded"
        hideBorder: true
        transparentBackground: true
        ignoreDesktopTransparentSetting: true
        acceptsInput: root.dockAcceptsInput && root.dockActive
        rendererActive: root.visible && !root.appDeckHidden
        renderEffectsEnabled: !root.safeRendering
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
            if (root.rootWindow) {
                PulseController.refreshResults(root.rootWindow, root.pulseResultsModel, false)
            }
        }
    }

    function syncAppDeckStateMode() {
        root.beginAppDeckLifecycle(root.state + ":" + root.mode)
        appsTransition = gridAppsMode ? 1.0 : 0.0
        pulseTransition = (pulseMode || contextMode) ? 1.0 : 0.0
        if (pulseMode || contextMode) {
            forceActiveFocus()
            Qt.callLater(function() {
                if (contextView && contextView.forceActiveFocus) {
                    contextView.forceActiveFocus()
                }
            })
        } else if (gridAppsMode) {
            forceActiveFocus()
        }
    }

    onStateChanged: root.syncAppDeckStateMode()
    onModeChanged: root.syncAppDeckStateMode()

    onVisibleChanged: {
        if (!visible) {
            root.endAppDeckLifecycle()
        }
    }

    Component.onCompleted: {
        console.info("[AppDeckEmbeddedSurface] DOCK_CREATED ptr=" + root)
        appsTransition = gridAppsMode ? 1.0 : 0.0
        pulseTransition = (pulseMode || contextMode) ? 1.0 : 0.0
    }

    Component.onDestruction: root.endAppDeckLifecycle()
}
