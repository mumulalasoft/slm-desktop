import QtQuick 2.15
import QtQuick.Window 2.15
import Slm_Desktop
import "../launchpad" as LaunchpadComp

// LaunchpadWindow is a frameless transient Window so KWin stacks it above
// all normal app windows.
//
// Dock host here is renderer-only. All dock state/logic stays global in
// DockSystem.
Window {
    id: root

    required property var rootWindow
    required property var desktopScene
    required property var appsModel
    property var dockAppsModel: (typeof DockModel !== "undefined" && DockModel) ? DockModel : appsModel

    readonly property int panelHeight: desktopScene ? desktopScene.panelHeight : 34
    readonly property int dockSafeInset: Math.max(
                                             24,
                                             Math.round(
                                                 (desktopScene ? Number(desktopScene.dockHeight || 120) : 120)
                                                 + (desktopScene ? Number(desktopScene.dockBottomMargin || 0) : 0)
                                                 + 20
                                             )
                                         )
    property bool dismissArmed: false
    property real revealProgress: 1.0

    signal appChosen(var appData)
    signal addToDockRequested(var appData)
    signal addToDesktopRequested(var appData)

    visible: !!rootWindow && !!desktopScene
             && rootWindow.visible
             && desktopScene.launchpadVisible
    color: "transparent"
    flags: Qt.FramelessWindowHint
    transientParent: rootWindow
    title: "Desktop Launchpad"
    x: rootWindow ? rootWindow.x : 0
    y: rootWindow ? rootWindow.y : 0
    width:  rootWindow ? rootWindow.width : 0
    height: rootWindow ? rootWindow.height : 0

    onVisibleChanged: {
        if (visible && desktopScene && desktopScene.launchpadVisible) {
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
        }

        Connections {
            target: desktopScene
            ignoreUnknownSignals: true
            function onLaunchpadVisibleChanged() {
                if (desktopScene.launchpadVisible) {
                    root.dismissArmed = false
                    dismissArmTimer.restart()
                } else {
                    root.dismissArmed = false
                    dismissArmTimer.stop()
                }
            }
        }

        // Dismiss tap area — excludes the top panel strip so topbar keeps its
        // own interactions.
        MouseArea {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: root.panelHeight
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 0
            z: 0
            enabled: root.dismissArmed
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
            visible: root.visible
            opacity: 1.0

            LaunchpadComp.Launchpad {
                id: launchpadContent
                anchors.fill: parent
                visible: parent.visible
                appsModel: root.appsModel
                topSafeInset: root.panelHeight
                revealProgress: 1.0
                bottomSafeInset: root.dockSafeInset
                onDismissRequested: desktopScene.launchpadVisible = false
                onAppChosen: function(appData) {
                    desktopScene.launchpadVisible = false
                    root.appChosen(appData)
                }
                onAddToDockRequested: function(appData) { root.addToDockRequested(appData) }
                onAddToDesktopRequested: function(appData) { root.addToDesktopRequested(appData) }
            }
        }

        LaunchpadDockHost {
            id: launchpadDockHost
            z: 2
            launchpadWindow: root
            desktopScene: root.desktopScene
            appsModel: root.dockAppsModel
        }

    }

    Component.onCompleted: {
        if (desktopScene.launchpadVisible) {
            dismissArmTimer.restart()
        }
    }

    Behavior on revealProgress {
        enabled: false
        NumberAnimation {
            duration: Theme.durationWorkspace
            easing.type: Theme.easingDefault
        }
    }

    Timer {
        id: dismissArmTimer
        interval: 180
        repeat: false
        onTriggered: root.dismissArmed = true
    }
}
