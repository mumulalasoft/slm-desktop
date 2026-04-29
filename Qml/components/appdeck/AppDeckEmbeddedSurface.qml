import QtQuick 2.15
import QtQuick.Effects
import Slm_Desktop
import "." as AppDeckComp
import "../overlay/AppHubActions.js" as AppHubActions
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
    signal requestOpenAppHub()

    readonly property var dockItem: collapsedView.dockItem
    readonly property bool rootWindowVisible: !!rootWindow && !!rootWindow.visible
    readonly property bool rootWindowLocked: !!rootWindow && !!rootWindow.lockScreenVisible
    readonly property string appdeckState: {
        if (!root.enabled || root.rootWindowLocked) {
            return "hidden"
        }
        if (root.rootWindow && root.rootWindow.searchVisible === true) {
            return "context"
        }
        if (typeof ShellStateController !== "undefined"
                && ShellStateController
                && ShellStateController.apphubVisible === true) {
            return "expanded"
        }
        if (root.desktopScene && root.desktopScene.apphubVisible === true) {
            return "expanded"
        }
        return "collapsed"
    }

    readonly property bool collapsedMode: appdeckState === "collapsed"
    readonly property bool expandedMode: appdeckState === "expanded"
    readonly property bool contextMode: appdeckState === "context"
    readonly property bool hiddenMode: appdeckState === "hidden"
    readonly property bool immersiveMode: expandedMode || contextMode
    readonly property real surfaceTransition: immersiveMode ? 1.0 : 0.0
    property real expandedTransition: expandedMode ? 1.0 : 0.0
    property real contextTransition: contextMode ? 1.0 : 0.0
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
    readonly property real collapsedDockBottomMargin: Math.max(
                                                         8,
                                                         Number(root.desktopScene ? root.desktopScene.dockBottomMargin : 0))
    readonly property real sharedPanelBottomInset: Math.max(
                                                       24,
                                                       Math.round(
                                                           (collapsedView.dockItem
                                                            ? Number(collapsedView.dockItem.height || 120)
                                                            : 120)
                                                           + root.collapsedDockBottomMargin
                                                           + 20))
    readonly property real sharedDockWidth: Math.max(320, Number(root.collapsedInputWidth || 1) + 20)
    readonly property real expandedContentW: Math.min(1180, Math.max(320, root.width - (sharedPanelMarginX * 2)))
    readonly property real expandedContentH: Math.min(
                                                940,
                                                Math.max(360,
                                                         root.height - sharedPanelTopInset - sharedPanelBottomInset))
    readonly property real expandedContentX: Math.round((root.width - expandedContentW) * 0.5)
    readonly property real expandedContentY: sharedPanelTopInset
    readonly property real expandedSurfaceW: Math.max(expandedContentW, sharedDockWidth)
    readonly property real expandedSurfaceH: Math.max(expandedContentH, root.height - expandedContentY - 8)
    readonly property real expandedSurfaceX: Math.round((root.width - expandedSurfaceW) * 0.5)
    readonly property real expandedSurfaceY: expandedContentY
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
    readonly property int collapsedInputX: Math.max(
                                           0,
                                           Math.round(
                                               (collapsedView ? Number(collapsedView.x || 0) : 0)
                                               + (collapsedView.dockItem
                                                  ? Number(collapsedView.dockItem.x || 0)
                                                    + Number(collapsedView.dockItem.inputRegionX || 0)
                                                  : 0)))
    readonly property int collapsedInputY: Math.max(
                                           0,
                                           Math.round(
                                               (collapsedView ? Number(collapsedView.y || 0) : 0)
                                               + (collapsedView.dockItem
                                                  ? Number(collapsedView.dockItem.y || 0)
                                                    + Number(collapsedView.dockItem.inputRegionY || 0)
                                                  : 0)))
    readonly property int collapsedInputWidth: Math.max(
                                               1,
                                               Math.round(
                                                   collapsedView.dockItem
                                                   ? Number(collapsedView.dockItem.inputRegionWidth
                                                            || collapsedView.dockItem.width || 1)
                                                   : 1))
    readonly property int collapsedInputHeight: Math.max(
                                                1,
                                                Math.round(
                                                    collapsedView.dockItem
                                                    ? Number(collapsedView.dockItem.inputRegionHeight
                                                             || collapsedView.dockItem.height || 1)
                                                    : 1))

    visible: root.enabled
             && root.dockHostVisible
             && root.rootWindowVisible
             && !root.hiddenMode

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

    function setAppHubVisibility(visible) {
        var v = !!visible
        if (typeof ShellStateController !== "undefined"
                && ShellStateController
                && ShellStateController.setAppHubVisible) {
            ShellStateController.setAppHubVisible(v)
            if (!v && ShellStateController.setAppHubSearchSeed) {
                ShellStateController.setAppHubSearchSeed("")
            }
            return
        }
        if (root.desktopScene && root.desktopScene.setAppHubVisible) {
            root.desktopScene.setAppHubVisible(v)
        }
    }

    function enterCollapsedMode() {
        setAppHubVisibility(false)
        if (root.rootWindow && root.rootWindow.setSearchVisible) {
            root.rootWindow.setSearchVisible(false)
        }
    }

    function enterExpandedMode() {
        if (root.rootWindow && root.rootWindow.setSearchVisible) {
            root.rootWindow.setSearchVisible(false)
        }
        setAppHubVisibility(true)
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
        visible: root.immersiveMode && root.surfaceTransition > 0.001
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
        layer.enabled: visible && !root.safeRendering
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Qt.rgba(0, 0, 0, Theme.darkMode ? 0.42 : 0.24)
            shadowBlur: 0.54
            shadowVerticalOffset: 10
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
        bottomSafeInset: root.sharedPanelBottomInset
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
        id: collapsedView
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.collapsedMode ? root.collapsedDockBottomMargin : 0
        visible: !root.hiddenMode
                 && (root.collapsedMode || root.expandedMode || root.contextMode || opacity > 0.01)
        opacity: 1.0
        scale: root.collapsedMode ? 1.0 : (1.0 - (0.03 * root.surfaceTransition))
        transform: Translate { y: root.collapsedMode ? 0 : root.surfaceTransition * 10 }
        z: 3
        hostName: "appdeck-embedded"
        hideBorder: root.immersiveMode
        transparentBackground: root.immersiveMode
        acceptsInput: root.dockAcceptsInput && root.collapsedMode
        rendererActive: root.visible && !root.hiddenMode
        renderEffectsEnabled: !root.safeRendering
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

    onAppdeckStateChanged: {
        root.beginAppDeckLifecycle(appdeckState)
        expandedTransition = expandedMode ? 1.0 : 0.0
        contextTransition = contextMode ? 1.0 : 0.0
        if (contextMode) {
            forceActiveFocus()
            Qt.callLater(function() {
                if (contextView && contextView.forceActiveFocus) {
                    contextView.forceActiveFocus()
                }
            })
        } else if (expandedMode) {
            forceActiveFocus()
        }
    }

    onVisibleChanged: {
        if (!visible) {
            root.endAppDeckLifecycle()
        }
    }

    Component.onCompleted: {
        console.info("[AppDeckEmbeddedSurface] DOCK_CREATED ptr=" + root)
        expandedTransition = expandedMode ? 1.0 : 0.0
        contextTransition = contextMode ? 1.0 : 0.0
    }

    Component.onDestruction: root.endAppDeckLifecycle()
}
