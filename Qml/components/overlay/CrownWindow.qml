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

    // Crown is a persistent layer-shell surface. It owns the top-bar and popup
    // input region independently from the desktop window, matching AppDeck.
    visible: !!rootWindow && rootWindow.visible && root._layerPrepared
    x: 0
    y: 0
    width: rootWindow ? rootWindow.width : 0
    height: root.surfaceHeight

    function syncLayerSurfaceSize() {
        if (!root.layerShellSupported || typeof CrownLayerShell === "undefined" || !CrownLayerShell) {
            return
        }
        CrownLayerShell.setTopBar(root,
                                  Math.max(1, Math.round(root.width)),
                                  Math.max(1, Math.round(root.height)),
                                  0,
                                  0,
                                  Math.max(1, Math.round(root.width)),
                                  Math.max(1, Math.round(root.height)),
                                  Math.max(1, Math.round(root.safePanelHeight)))
    }

    onVisibleChanged: root.syncLayerSurfaceSize()
    onWidthChanged: root.syncLayerSurfaceSize()
    onHeightChanged: root.syncLayerSurfaceSize()
    onPanelHeightChanged: root.syncLayerSurfaceSize()
    onAnyPopupOpenChanged: Qt.callLater(function() { root.syncLayerSurfaceSize() })
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
        opacity: ShellState.topBarOpacity

        Behavior on opacity {
            NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingDefault }
        }

        CrownComp.Crown {
            id: topBarSurface
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: root.safePanelHeight
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
    }

    Timer {
        id: layerShellRetryTimer
        interval: 300
        repeat: true
        running: root.visible && root.layerShellSupported
        onTriggered: root.syncLayerSurfaceSize()
    }

    Component.onCompleted: {
        if (root.layerShellSupported && typeof CrownLayerShell !== "undefined" && CrownLayerShell) {
            CrownLayerShell.prepareTopBarWindow(root)
        }
        root._layerPrepared = true
        Qt.callLater(function() { root.syncLayerSurfaceSize() })
    }
}
