import QtQuick 2.15
import Slm_Desktop
import "../crown" as CrownComp

Item {
    id: root

    required property var rootWindow
    required property var desktopScene
    property var shellApi: null
    property var desktopMenuProvider: null
    readonly property bool deferredReady: !!(shellApi && shellApi.startupTopbarBootstrapReady)
    readonly property bool startupItemsReady: !!topBarSurface && !!topBarSurface.startupItemsReady
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

    visible: !!rootWindow && rootWindow.visible
    opacity: ShellState.topBarOpacity
    z: ShellZOrder.topBar
    Behavior on opacity {
        NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingDefault }
    }
    x: 0
    y: 0
    width: rootWindow ? rootWindow.width : 0
    height: root.surfaceHeight

    PopupHostLayer {
        id: popupHostLayer
        anchors.fill: parent
        rootWindow: root.rootWindow
        z: 1
    }

    Item {
        anchors.fill: parent
        clip: !root.anyPopupOpen

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
}
