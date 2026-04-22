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
    property int zoomHeadroom: 76

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
        if (desktopScene && desktopScene.apphubVisible === true) {
            return "expanded"
        }
        return "collapsed"
    }

    readonly property bool collapsedMode: appdeckState === "collapsed"
    readonly property bool expandedMode: appdeckState === "expanded"
    readonly property bool contextMode: appdeckState === "context"
    readonly property bool hiddenMode: appdeckState === "hidden"

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

    function enterCollapsedMode() {
        if (desktopScene && desktopScene.setAppHubVisible) {
            desktopScene.setAppHubVisible(false)
        }
        if (rootWindow && rootWindow.setSearchVisible) {
            rootWindow.setSearchVisible(false)
        }
    }

    function enterExpandedMode() {
        if (rootWindow && rootWindow.setSearchVisible) {
            rootWindow.setSearchVisible(false)
        }
        if (desktopScene && desktopScene.setAppHubVisible) {
            desktopScene.setAppHubVisible(true)
        }
    }

    function enterContextMode() {
        if (desktopScene && desktopScene.setAppHubVisible) {
            desktopScene.setAppHubVisible(false)
        }
        if (rootWindow && rootWindow.setSearchVisible) {
            rootWindow.setSearchVisible(true)
        }
    }

    function enterHiddenMode() {
        if (rootWindow && rootWindow.setSearchVisible) {
            rootWindow.setSearchVisible(false)
        }
        if (desktopScene && desktopScene.setAppHubVisible) {
            desktopScene.setAppHubVisible(false)
        }
    }

    function focusSearchField() {
        if (contextView && contextView.focusSearchField) {
            contextView.focusSearchField()
        }
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
                                                 140,
                                                 Math.ceil(
                                                     (collapsedView.dockItem ? collapsedView.dockItem.height : 120)
                                                     + Number(root.zoomHeadroom || 56)
                                                     + 16
                                                 )
                                             )

    width: Number(root.targetScreenGeometry.width || (rootWindow ? rootWindow.width : Screen.width))
    height: collapsedMode
            ? dockSurfaceHeight
            : Number(root.targetScreenGeometry.height || (rootWindow ? rootWindow.height : Screen.height))
    x: Number(root.targetScreenGeometry.x || 0)
    y: collapsedMode
       ? (Number(root.targetScreenGeometry.y || 0)
          + Number(root.targetScreenGeometry.height || (rootWindow ? rootWindow.height : Screen.height))
          - height)
       : Number(root.targetScreenGeometry.y || 0)

    opacity: root.dockLayerReady ? 1.0 : 0.0

    Behavior on height {
        NumberAnimation {
            duration: Theme.durationNormal
            easing.type: Theme.easingDefault
        }
    }

    Behavior on y {
        NumberAnimation {
            duration: Theme.durationNormal
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
            visible: root.expandedMode || root.contextMode
            color: Theme.color("screenshotScrim")
            opacity: visible ? 1.0 : 0.0
            z: 0

            Behavior on opacity {
                NumberAnimation {
                    duration: Theme.durationFast
                    easing.type: Theme.easingLight
                }
            }
        }

        AppDeckComp.AppDeckExpandedView {
            id: expandedView
            anchors.fill: parent
            visible: root.expandedMode
            opacity: visible ? 1.0 : 0.0
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
                    duration: Theme.durationFast
                    easing.type: Theme.easingLight
                }
            }
        }

        AppDeckComp.AppDeckContextView {
            id: contextView
            anchors.fill: parent
            z: 2
            active: root.contextMode
            panelHeight: root.desktopScene ? root.desktopScene.panelHeight : 0
            queryText: root.rootWindow ? String(root.rootWindow.pulseQuery || "") : ""
            resultsModel: root.pulseResultsModel
            selectedIndex: root.rootWindow ? Number(root.rootWindow.pulseSelectedIndex || -1) : -1
            showDebugInfo: root.rootWindow ? !!root.rootWindow.pulseShowDebug : false
            searchProfileMeta: root.rootWindow ? root.rootWindow.pulseProfileMeta : ({})
            telemetryMeta: root.rootWindow ? root.rootWindow.pulseTelemetryMeta : ({})
            telemetryLast: root.rootWindow ? root.rootWindow.pulseTelemetryLast : ({})
            providerStats: root.rootWindow ? root.rootWindow.pulseProviderStats : ({})
            previewData: root.rootWindow ? root.rootWindow.pulsePreviewData : ({})
            onDismissRequested: {
                root.enterCollapsedMode()
                root.requestCollapse()
            }
            onQueryTextChangedRequest: function(text) {
                if (!root.rootWindow) {
                    return
                }
                root.rootWindow.setSearchQuery(text)
                root.rootWindow.pulseQueryGeneration = Number(root.rootWindow.pulseQueryGeneration || 0) + 1
                pulseDebounce.restart()
            }
            onSelectedIndexChangedByUser: function(indexValue) {
                if (!root.rootWindow) {
                    return
                }
                root.rootWindow.pulseSelectedIndex = indexValue
                PulseController.refreshPreview(root.rootWindow, root.pulseResultsModel)
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
        }

        AppDeckComp.AppDeckCollapsedView {
            id: collapsedView
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            visible: !root.hiddenMode
            z: 3
            hostName: "appdeck"
            acceptsInput: root.dockAcceptsInput
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

    onAppdeckStateChanged: {
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
        }
    }

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
        root._sendOverlayCommand("overlay unregister appdeck")
    }
}
