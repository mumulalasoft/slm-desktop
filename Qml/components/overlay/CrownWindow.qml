import QtQuick 2.15
import QtQuick.Window 2.15
import Slm_Desktop
import "../crown" as CrownComp

Window {
    id: root

    required property var rootWindow
    required property var desktopScene
    property var shellApi: null
    property var desktopMenuProvider: null
    // Hybrid mode: defer applet bootstrap off critical load path, but start quickly.
    readonly property bool deferredReady: !!(shellApi && shellApi.startupTopbarBootstrapReady)
    readonly property bool startupItemsReady: !!topBarSurface && !!topBarSurface.startupItemsReady
    readonly property bool layerShellSupported: (typeof CrownLayerShell !== "undefined")
                                                && !!CrownLayerShell
                                                && !!CrownLayerShell.supported
    property bool _layerPrepared: !layerShellSupported

    readonly property bool anyPopupOpen: !!topBarSurface && !!topBarSurface.anyPopupOpen
    readonly property int popupHeadroom: Math.round(Math.min((rootWindow ? rootWindow.height : 0) * 0.45, 420))
    readonly property int panelHeight: desktopScene ? desktopScene.panelHeight : 0
    readonly property int safePanelHeight: Math.max(1, root.panelHeight > 0 ? root.panelHeight : 36)
    readonly property int surfaceHeight: root.safePanelHeight + (anyPopupOpen ? popupHeadroom : 0)
    readonly property var shellPolicy: (typeof ShellPolicyController !== "undefined")
                                       ? ShellPolicyController
                                       : null
    readonly property bool policySurfaceVisible: !root.shellPolicy
                                                 || root.shellPolicy.crownSurfaceVisible
    readonly property bool policyContentVisible: !root.shellPolicy
                                                 || root.shellPolicy.crownContentVisible
    readonly property bool effectiveContentVisible: root.policyContentVisible || root.anyPopupOpen
    readonly property bool policyEdgeRevealEnabled: !!root.shellPolicy
                                                    && root.shellPolicy.edgeRevealEnabled
    readonly property int revealActivationHeight: root.shellPolicy
                                                 ? Math.max(1, Math.round(root.shellPolicy.edgeRevealSize || 6))
                                                 : 6
    readonly property int policySurfaceHeight: root.effectiveContentVisible
                                             ? root.surfaceHeight
                                             : root.revealActivationHeight
    property bool topRevealHeld: false

    signal launcherRequested()
    signal pulseRequested()
    signal screenshotCaptureRequested(string mode, int delaySec, bool grabPointer, bool concealText)
    signal startupItemsReadyReached()

    function openScreenshotPopup() {
        if (topBarSurface && topBarSurface.openScreenshotPopup) {
            topBarSurface.openScreenshotPopup()
        }
    }

    color: "transparent"
    transientParent: null
    title: "SLM Crown Surface"
    flags: Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint

    // Crown is a persistent layer-shell surface. It owns the top-bar and popup
    // input region independently from the desktop window, matching AppDeck.
    visible: !!rootWindow
             && rootWindow.visible
             && !rootWindow.lockScreenVisible
             && root.policySurfaceVisible
             && root.layerShellSupported
             && root._layerPrepared
    x: 0
    y: 0
    width: rootWindow ? rootWindow.width : 0
    height: root.policySurfaceHeight

    function syncLayerSurfaceSize() {
        if (!root.layerShellSupported || typeof CrownLayerShell === "undefined" || !CrownLayerShell) {
            return
        }
        CrownLayerShell.setTopBar(root,
                                  Math.max(1, Math.round(root.width)),
                                  Math.max(1, Math.round(root.policySurfaceHeight)),
                                  0,
                                  0,
                                  Math.max(1, Math.round(root.width)),
                                  Math.max(1, Math.round(root.effectiveContentVisible
                                                         ? root.surfaceHeight
                                                         : root.revealActivationHeight)),
                                  Math.max(0, Math.round(root.effectiveContentVisible
                                                        ? root.safePanelHeight
                                                        : 0)))
    }

    onVisibleChanged: root.syncLayerSurfaceSize()
    onWidthChanged: root.syncLayerSurfaceSize()
    onHeightChanged: root.syncLayerSurfaceSize()
    onPanelHeightChanged: root.syncLayerSurfaceSize()
    onAnyPopupOpenChanged: Qt.callLater(function() { root.syncLayerSurfaceSize() })
    onPolicySurfaceVisibleChanged: root.syncLayerSurfaceSize()
    onPolicyContentVisibleChanged: {
        console.info("[CROWN] [SHELL_POLICY] contentVisible=" + root.policyContentVisible
                     + " policy=" + (root.shellPolicy ? root.shellPolicy.visibilityPolicyName : "Normal"))
        root.syncLayerSurfaceSize()
    }
    onEffectiveContentVisibleChanged: root.syncLayerSurfaceSize()
    onSceneGraphInitialized: root.syncLayerSurfaceSize()

    PopupHostLayer {
        id: popupHostLayer
        anchors.fill: parent
        rootWindow: root.rootWindow
        z: 1
    }

    Item {
        anchors.fill: parent
        clip: !root.anyPopupOpen
        opacity: ShellState.topBarOpacity * (root.effectiveContentVisible ? 1.0 : 0.0)

        Behavior on opacity {
            NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingDefault }
        }

        CrownComp.Crown {
            id: topBarSurface
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: root.safePanelHeight
            enabled: root.effectiveContentVisible
            rootWindow: root.rootWindow
            deferredReady: root.deferredReady
            popupHost: popupHostLayer
            fileManagerContent: (root.shellApi
                                 && root.shellApi.detachedFileManagerVisible
                                 && root.shellApi.fileManagerContent)
                                ? root.shellApi.fileManagerContent
                                : ((root.desktopScene && root.desktopScene.desktopFileManagerContent)
                                   ? root.desktopScene.desktopFileManagerContent : null)
            desktopMenuProvider: root.desktopMenuProvider

            onLauncherRequested: root.launcherRequested()
            onPulseRequested: root.pulseRequested()
            onScreenshotCaptureRequested: function(mode, delaySec, grabPointer, concealText) {
                root.screenshotCaptureRequested(mode, delaySec, grabPointer, concealText)
            }
            onStartupItemsReadyReached: root.startupItemsReadyReached()
        }

        HoverHandler {
            id: topRevealHoldHandler
            target: topBarSurface
            enabled: root.policyEdgeRevealEnabled && root.effectiveContentVisible
            onHoveredChanged: {
                root.topRevealHeld = hovered
                if (hovered && root.shellPolicy && root.shellPolicy.requestTopEdgeReveal) {
                    root.shellPolicy.requestTopEdgeReveal("topbar-hover")
                }
            }
        }
    }

    MouseArea {
        id: topRevealSensor
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.revealActivationHeight
        z: 50
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        enabled: root.policyEdgeRevealEnabled && !root.effectiveContentVisible
        onEntered: {
            if (root.shellPolicy && root.shellPolicy.requestTopEdgeReveal) {
                root.shellPolicy.requestTopEdgeReveal("top-edge")
            }
        }
        onPositionChanged: {
            if (root.shellPolicy && root.shellPolicy.requestTopEdgeReveal) {
                root.shellPolicy.requestTopEdgeReveal("top-edge-motion")
            }
        }
    }

    Timer {
        id: layerShellRetryTimer
        interval: 300
        repeat: true
        running: root.visible && root.layerShellSupported
        onTriggered: root.syncLayerSurfaceSize()
    }

    Timer {
        id: topRevealHoldTimer
        interval: 800
        repeat: true
        running: root.topRevealHeld && root.policyEdgeRevealEnabled && root.effectiveContentVisible
        onTriggered: {
            if (root.shellPolicy && root.shellPolicy.requestTopEdgeReveal) {
                root.shellPolicy.requestTopEdgeReveal("topbar-hover-hold")
            }
        }
    }

    Component.onCompleted: {
        if (root.layerShellSupported && typeof CrownLayerShell !== "undefined" && CrownLayerShell) {
            CrownLayerShell.prepareTopBarWindow(root)
        }
        root._layerPrepared = true
        console.info("[CROWN] [LAYER_STATE] policySurface=" + root.policySurfaceVisible
                     + " policyContent=" + root.effectiveContentVisible
                     + " layer=OverlayLayer")
        Qt.callLater(function() { root.syncLayerSurfaceSize() })
    }
}
