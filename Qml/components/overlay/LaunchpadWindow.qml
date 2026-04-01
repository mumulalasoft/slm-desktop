import QtQuick 2.15
import QtQuick.Window 2.15
import "../launchpad" as LaunchpadComp

// LaunchpadWindow is a frameless transient Window so KWin (and any WM/compositor)
// stacks it above all normal app windows.
//
// The bottom dock zone is left transparent so DockWindow (rendered in Main.qml
// directly below this Window) shows through unchanged.  Because the compositor
// alpha-blends the transparent region, DockWindow's dock remains the single dock
// instance — no state discontinuity on launchpad open/close.
// Tapping the transparent dock zone dismisses the launchpad.
Window {
    id: root

    required property var rootWindow
    required property var desktopScene
    required property var appsModel

    readonly property int panelHeight: desktopScene ? desktopScene.panelHeight : 34

    // Height of the transparent dock passthrough zone at the bottom of the window.
    // Sized to fully expose DockWindow's dock surface including its zoom headroom.
    readonly property int dockAreaHeight: desktopScene
        ? Math.ceil(desktopScene.dockHeight + desktopScene.dockBottomMargin + 20)
        : 140

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

        // Dismiss tap area — excludes the top panel strip (TopBarWindow shows
        // through above) and the bottom dock zone (DockWindow shows through below).
        MouseArea {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: root.panelHeight
            anchors.bottom: parent.bottom
            anchors.bottomMargin: root.dockAreaHeight
            z: 0
            onClicked: desktopScene.launchpadVisible = false
        }

        // Dismiss tap in the transparent dock zone — tapping here exits the
        // launchpad so the user can then interact with the dock directly.
        MouseArea {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: root.dockAreaHeight
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
                // Reserve the dock zone at the bottom so the grid and search bar
                // don't overlap the transparent area where DockWindow shows through.
                bottomSafeInset: root.dockAreaHeight
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
