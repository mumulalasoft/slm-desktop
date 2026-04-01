import QtQuick 2.15
import Slm_Desktop
import "../launchpad" as LaunchpadComp

// LaunchpadWindow is a full-screen Item inside the shell's main window scene.
// Being an Item (not a separate Window) lets DockWindow's z:200 appear above it
// while still covering workspace surfaces (z:20) and app window content.
Item {
    id: root

    required property var rootWindow
    required property var desktopScene
    required property var appsModel

    readonly property int panelHeight: desktopScene ? desktopScene.panelHeight : 34
    readonly property real launchpadProgress: (desktopScene && desktopScene.launchpadVisible) ? 1.0 : 0.0

    signal appChosen(var appData)
    signal addToDockRequested(var appData)
    signal addToDesktopRequested(var appData)

    visible: !!rootWindow && !!desktopScene
             && rootWindow.visible
             && (desktopScene.launchpadVisible || launchpadFrame.opacity > 0.01)
    x: 0
    y: 0
    width:  rootWindow ? rootWindow.width  : 0
    height: rootWindow ? rootWindow.height : 0

    onVisibleChanged: {
        if (visible) {
            launchpadContent.forceActiveFocus()
        }
    }

    Item {
        id: launchpadOverlay
        anchors.fill: parent

        QtObject {
            id: launchpadMotion
            property real progress: root.launchpadProgress

            Behavior on progress {
                NumberAnimation {
                    duration: Theme.durationSlow
                    easing.type: Theme.easingDefault
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            z: 0
            onClicked: desktopScene.launchpadVisible = false
        }

        Rectangle {
            id: launchpadFrame
            z: 1
            anchors.fill: parent
            radius: 0
            clip: false
            color: "transparent"
            visible: (desktopScene ? desktopScene.launchpadVisible : false) || opacity > 0.01
            opacity: launchpadMotion.progress
            transform: Scale {
                origin.x: launchpadFrame.width * 0.5
                origin.y: launchpadFrame.height * 0.5
                xScale: 0.965 + (0.035 * launchpadMotion.progress)
                yScale: 0.965 + (0.035 * launchpadMotion.progress)
            }

            LaunchpadComp.Launchpad {
                id: launchpadContent
                anchors.fill: parent
                visible: parent.visible
                appsModel: root.appsModel
                bottomSafeInset: 120
                onDismissRequested: desktopScene.launchpadVisible = false
                onAppChosen: function(appData) {
                    desktopScene.launchpadVisible = false
                    root.appChosen(appData)
                }
                onAddToDockRequested: function(appData) {
                    root.addToDockRequested(appData)
                }
                onAddToDesktopRequested: function(appData) {
                    root.addToDesktopRequested(appData)
                }
            }
        }
    }

}
