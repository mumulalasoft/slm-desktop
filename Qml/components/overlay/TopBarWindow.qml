import QtQuick 2.15
import Slm_Desktop
import "../topbar" as TopBarComp

Item {
    id: root

    required property var rootWindow
    required property var desktopScene
    property var shellApi: null

    readonly property bool anyPopupOpen: !!topBarSurface && !!topBarSurface.anyPopupOpen
    readonly property int popupHeadroom: Math.round(Math.min((rootWindow ? rootWindow.height : 0) * 0.45, 420))

    signal launcherRequested()
    signal styleGalleryRequested()
    signal tothespotRequested()
    signal screenshotCaptureRequested(string mode, int delaySec, bool grabPointer, bool concealText)

    // TopBar is a persistent layer — never hidden by overlay state.
    // Opacity dims when launchpad is open so it blends with the frosted backdrop.
    visible: !!rootWindow && rootWindow.visible
    opacity: ShellState.topBarOpacity
    z: ShellZOrder.topBar
    Behavior on opacity {
        NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easingDefault }
    }
    x: 0
    y: 0
    width: rootWindow ? rootWindow.width : 0
    height: (desktopScene ? desktopScene.panelHeight : 0) + (anyPopupOpen ? popupHeadroom : 0)

    Item {
        anchors.fill: parent
        clip: true

        TopBarComp.TopBar {
            id: topBarSurface
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: desktopScene ? desktopScene.panelHeight : 0
            fileManagerContent: (root.shellApi && root.shellApi.fileManagerContent)
                                ? root.shellApi.fileManagerContent : null

            onLauncherRequested: root.launcherRequested()
            onStyleGalleryRequested: root.styleGalleryRequested()
            onTothespotRequested: root.tothespotRequested()
            onScreenshotCaptureRequested: function(mode, delaySec, grabPointer, concealText) {
                root.screenshotCaptureRequested(mode, delaySec, grabPointer, concealText)
            }
        }
    }
}
