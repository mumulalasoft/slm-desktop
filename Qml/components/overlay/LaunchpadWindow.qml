import QtQuick 2.15
import QtQuick.Window 2.15
import Slm_Desktop
import "../launchpad" as LaunchpadComp

// LaunchpadWindow is a frameless transient Window so KWin stacks it above
// all normal app windows.
//
// The dock is NOT rendered inside this Window.  Instead, the window height is
// shortened to leave the dock zone uncovered: mouse events in the dock zone
// fall through to the ApplicationWindow where ShellDockHost (the single dock
// renderer) lives.  This gives one dock instance with no state duplication.
//
// Height = desktopScene.dockShownY - DockSystem.zoomHeadroom
//   → covers screen from top to just above the dock interaction zone
//   → dock is always visible and interactive at the bottom while launchpad is open
Window {
    id: root

    required property var rootWindow
    required property var desktopScene
    required property var appsModel

    readonly property int panelHeight: desktopScene ? desktopScene.panelHeight : 34

    // Dock zone height excluded from the bottom of this window so events
    // there reach ShellDockHost in ApplicationWindow directly.
    readonly property real dockExcludeHeight: desktopScene
        ? (desktopScene.dockShownY - DockSystem.zoomHeadroom)
        : (rootWindow ? rootWindow.height : 0)

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
    width:  rootWindow ? rootWindow.width : 0
    // Height stops above the dock interaction zone; dock zone stays in
    // ApplicationWindow and is handled by ShellDockHost.
    height: rootWindow
            ? Math.max(rootWindow.height * 0.4, dockExcludeHeight)
            : 0

    onVisibleChanged: {
        if (visible) {
            requestActivate()
        }
    }

    readonly property string _wallpaperSource: {
        const uri = String((typeof UIPreferences !== "undefined" && UIPreferences)
                           ? (UIPreferences.wallpaperUri || "") : "")
        return uri.length > 0 ? uri : "qrc:/images/wallpaper.jpeg"
    }

    Item {
        id: launchpadOverlay
        anchors.fill: parent

        // Wallpaper base — always opaque so ApplicationWindow never shows through
        // launchpadFrame while it animates. z:-1 keeps it below dismiss MouseArea
        // and launchpadFrame. enabled:false ensures no mouse event interception.
        Image {
            anchors.fill: parent
            z: -1
            enabled: false
            source: root._wallpaperSource
            fillMode: Image.PreserveAspectCrop
            smooth: true
            mipmap: true
        }

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
        // The dock zone at the bottom is excluded from this Window's surface
        // entirely, so no explicit bottom exclusion is needed here.
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
            y: 0
            width: parent.width
            height: parent.height
            radius: 0
            clip: false
            color: "transparent"
            visible: desktopScene.launchpadVisible || opacity > 0.01
            opacity: launchpadMotion.progress
            onVisibleChanged: {
                if (!launchpadMotion.ready) return
                launchpadMotion.animateTo(desktopScene.launchpadVisible ? 1.0 : 0.0)
            }

            LaunchpadComp.Launchpad {
                id: launchpadContent
                anchors.fill: parent
                visible: parent.visible
                appsModel: root.appsModel
                topSafeInset: root.panelHeight
                // Window height already excludes the dock zone, so only a
                // small bottom padding is needed here.
                bottomSafeInset: 8
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
