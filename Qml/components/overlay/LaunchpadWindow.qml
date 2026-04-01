import QtQuick 2.15
import QtQuick.Window 2.15
import "../dock" as DockComp
import "../launchpad" as LaunchpadComp

// LaunchpadWindow is a frameless transient Window so KWin (and any WM/compositor)
// stacks it above all normal app windows.
//
// The dock is rendered inside this Window (launchpadDockLayer) so it always
// appears above the launchpad wallpaper with no visual seam.  DockWindow in
// Main.qml is NOT forced to reveal during launchpad — its hide/show state is
// preserved unchanged.  When the launchpad closes, the shell dock state is
// exactly as it was before opening.
Window {
    id: root

    required property var rootWindow
    required property var desktopScene
    required property var appsModel
    required property var dockModel

    readonly property int panelHeight: desktopScene ? desktopScene.panelHeight : 34

    signal appChosen(var appData)
    signal addToDockRequested(var appData)
    signal addToDesktopRequested(var appData)

    visible: !!rootWindow && !!desktopScene
             && rootWindow.visible
             && (desktopScene.launchpadVisible || launchpadFrame.opacity > 0.01)
    color: "transparent"
    flags: Qt.FramelessWindowHint
    transientParent: rootWindow
    title: "Desktop Launchpad"
    x: rootWindow ? rootWindow.x : 0
    y: rootWindow ? rootWindow.y : 0
    width:  rootWindow ? rootWindow.width  : 0
    height: rootWindow ? rootWindow.height : 0

    onVisibleChanged: {
        if (visible) {
            requestActivate()
        }
    }

    Item {
        id: launchpadOverlay
        anchors.fill: parent

        QtObject {
            id: launchpadMotion
            property bool ready: false
            property real progress: 0.0

            function ensureChannel() {
                if (typeof MotionController === "undefined" || !MotionController) {
                    return false
                }
                if (MotionController.channel !== "launchpad.reveal") {
                    MotionController.channel = "launchpad.reveal"
                }
                if (MotionController.preset !== "launcher") {
                    MotionController.preset = "launcher"
                }
                return true
            }

            function snapTo(stateValue) {
                if (!ensureChannel()) {
                    progress = stateValue
                    return
                }
                MotionController.cancelAndSettle(stateValue)
                progress = MotionController.value
            }

            function animateTo(stateValue) {
                if (!ensureChannel()) {
                    progress = stateValue
                    return
                }
                MotionController.startFromCurrent(stateValue)
            }
        }

        Connections {
            target: (typeof MotionController !== "undefined" && MotionController) ? MotionController : null
            ignoreUnknownSignals: true
            function onValueChanged() {
                if (!launchpadMotion.ready) return
                if (MotionController.channel === "launchpad.reveal") {
                    launchpadMotion.progress = Math.max(0, Math.min(1, Number(MotionController.value || 0)))
                }
            }
        }

        Connections {
            target: desktopScene
            ignoreUnknownSignals: true
            function onLaunchpadVisibleChanged() {
                if (!launchpadMotion.ready) return
                launchpadMotion.animateTo(desktopScene.launchpadVisible ? 1.0 : 0.0)
            }
        }

        // Dismiss tap area — excludes the top panel strip so TopBarWindow stays
        // interactive through the transparent top portion of this Window.
        MouseArea {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: root.panelHeight
            anchors.bottom: parent.bottom
            z: 0
            onClicked: desktopScene.launchpadVisible = false
        }

        Rectangle {
            id: launchpadFrame
            z: 1
            x: 0
            y: root.panelHeight
            width: parent.width
            height: parent.height - root.panelHeight
            radius: 0
            clip: false
            color: "transparent"
            visible: desktopScene.launchpadVisible || opacity > 0.01
            opacity: launchpadMotion.progress
            transform: Scale {
                origin.x: launchpadFrame.width * 0.5
                origin.y: launchpadFrame.height * 0.5
                xScale: 0.965 + (0.035 * launchpadMotion.progress)
                yScale: 0.965 + (0.035 * launchpadMotion.progress)
            }
            onVisibleChanged: {
                if (!launchpadMotion.ready) return
                launchpadMotion.animateTo(desktopScene.launchpadVisible ? 1.0 : 0.0)
            }

            LaunchpadComp.Launchpad {
                id: launchpadContent
                anchors.fill: parent
                visible: parent.visible
                appsModel: root.appsModel
                bottomSafeInset: Math.max(120, launchpadDockLayer.height + 14)
                onDismissRequested: desktopScene.launchpadVisible = false
                onAppChosen: function(appData) {
                    desktopScene.launchpadVisible = false
                    root.appChosen(appData)
                }
                onAddToDockRequested: function(appData) { root.addToDockRequested(appData) }
                onAddToDesktopRequested: function(appData) { root.addToDesktopRequested(appData) }
            }
        }

        // Dock rendered inside this Window so it appears above the launchpad
        // wallpaper with no visual boundary.  Positioned identically to DockWindow.
        Item {
            id: launchpadDockLayer
            z: 2
            visible: !!desktopScene && desktopScene.launchpadVisible
            x: Math.round((parent.width - width) / 2)
            y: Math.round(parent.height - height - (desktopScene ? desktopScene.dockBottomMargin : 0))
            readonly property int zoomHeadroom: 76
            readonly property bool headroomActive: launchpadDockSurface.hovered
                                                   || (desktopScene ? desktopScene.pointerNearDock : false)
            width: Math.max(1, Math.ceil(launchpadDockSurface.width))
            height: Math.max(1, Math.ceil(launchpadDockSurface.height + (headroomActive ? zoomHeadroom : 0)))

            DockComp.Dock {
                id: launchpadDockSurface
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                opacity: 1.0
                appsModel: root.dockModel
                onLaunchpadRequested: desktopScene.launchpadVisible = false
            }
        }
    }

    Component.onCompleted: {
        launchpadMotion.ready = true
        launchpadMotion.snapTo(desktopScene.launchpadVisible ? 1.0 : 0.0)
    }
}
