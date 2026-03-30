import QtQuick 2.15
import QtQuick.Window 2.15
import "../dock" as DockComp
import "../launchpad" as LaunchpadComp

Window {
    id: root

    required property var rootWindow
    required property var desktopScene
    required property var appsModel
    required property var dockModel

    // Panel height reserved at the top so TopBarWindow (persistent) shows through.
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
    width: rootWindow ? rootWindow.width : 0
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
                if (!launchpadMotion.ready) {
                    return
                }
                if (MotionController.channel === "launchpad.reveal") {
                    launchpadMotion.progress = Math.max(0, Math.min(1, Number(MotionController.value || 0)))
                }
            }
        }

        Connections {
            target: desktopScene
            ignoreUnknownSignals: true
            function onLaunchpadVisibleChanged() {
                if (!launchpadMotion.ready) {
                    return
                }
                launchpadMotion.animateTo(desktopScene.launchpadVisible ? 1.0 : 0.0)
            }
        }

        // Dismiss area excludes the top panel strip so TopBar remains interactive.
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
                if (!launchpadMotion.ready) {
                    return
                }
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
                onAddToDockRequested: function(appData) {
                    root.addToDockRequested(appData)
                }
                onAddToDesktopRequested: function(appData) {
                    root.addToDesktopRequested(appData)
                }
            }
        }

        Item {
            id: launchpadDockLayer
            z: 2
            visible: launchpadFrame.visible
            x: Math.round((parent.width - width) / 2)
            y: Math.round(parent.height - height - desktopScene.dockBottomMargin)
            readonly property int zoomHeadroom: 76
            readonly property bool headroomActive: launchpadDockSurface.hovered || desktopScene.pointerNearDock
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
