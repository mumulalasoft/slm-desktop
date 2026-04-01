import QtQuick 2.15
import QtQuick.Window 2.15
import "../launchpad" as LaunchpadComp

// LaunchpadWindow is a frameless transient Window so KWin stacks it above
// all normal app windows.  The dock is NOT rendered here — DockWindow is its
// own always-on-top Window (Qt.WindowStaysOnTopHint) and naturally appears
// above this Window, giving a single dock instance with no state duplication.
Window {
    id: root

    required property var rootWindow
    required property var desktopScene
    required property var appsModel

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
                onDismissRequested: desktopScene.launchpadVisible = false
                onAppChosen: function(appData) {
                    desktopScene.launchpadVisible = false
                    root.appChosen(appData)
                }
                onAddToDockRequested: function(appData) { root.addToDockRequested(appData) }
                onAddToDesktopRequested: function(appData) { root.addToDesktopRequested(appData) }
            }
        }
    }

    Component.onCompleted: {
        launchpadMotion.ready = true
        launchpadMotion.snapTo(desktopScene.launchpadVisible ? 1.0 : 0.0)
    }
}
