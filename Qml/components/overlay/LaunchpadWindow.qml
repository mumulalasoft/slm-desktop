import QtQuick 2.15
import QtQuick.Window 2.15
import Slm_Desktop
import "../launchpad" as LaunchpadComp

// LaunchpadWindow — transient frameless Window for the app launcher overlay.
//
// As a compositor-managed transient Window it appears above all application
// windows. The DockWindow (wlr-layer-shell LayerTop) is placed above this
// window by the compositor — no exclusion zones or height trimming needed.
//
// Performance: the Window stays mapped to the compositor at all times so there
// is no wl_surface creation / XDG configure roundtrip on each open. When the
// launchpad is closed, Qt.WindowTransparentForInput empties the input region so
// pointer events pass through to the desktop below. Content is hidden via
// opacity so the scene graph is not traversed, but the surface itself is ready.
//
// bottomSafeInset is a layout inset so the app grid does not extend into the
// dock area that DockWindow renders on top of.
Window {
    id: root

    required property var rootWindow
    required property var desktopScene
    required property var appsModel

    readonly property int panelHeight: desktopScene ? desktopScene.panelHeight : 34
    readonly property bool layerShellAvailable: (typeof WlrLayerShell !== "undefined") && !!WlrLayerShell
    readonly property bool layerShellSupported: layerShellAvailable && !!WlrLayerShell.isSupported()
    readonly property bool dockLayerReady: !layerShellAvailable
                                           || !layerShellSupported
                                           || ((typeof DockBootstrapState !== "undefined" && DockBootstrapState)
                                               ? !!DockBootstrapState.readyToRender
                                               : true)

    // Layout inset so the Launchpad grid content is not obscured by the dock
    // that DockWindow renders on top of this window.
    readonly property int bottomSafeInset: Math.max(
        24,
        Math.round(
            (desktopScene ? Number(desktopScene.dockHeight || 120) : 120)
            + (desktopScene ? Number(desktopScene.dockBottomMargin || 0) : 0)
            + 20
        )
    )

    // True while the launchpad is the active overlay.
    readonly property bool isActive: !!desktopScene && desktopScene.launchpadVisible === true

    property bool dismissArmed: false

    signal appChosen(var appData)
    signal addToDockRequested(var appData)
    signal addToDesktopRequested(var appData)

    // Keep behavior deterministic across compositors: map only when active.
    visible: !!rootWindow && !!desktopScene && rootWindow.visible && dockLayerReady && root.isActive
    flags:   Qt.FramelessWindowHint
    color:   "transparent"
    transientParent: rootWindow
    title:   "Desktop Launchpad"
    x:      rootWindow ? rootWindow.x : 0
    y:      rootWindow ? rootWindow.y : 0
    width:  rootWindow ? rootWindow.width  : 0
    height: rootWindow ? rootWindow.height : 0

    onIsActiveChanged: {
        if (isActive) {
            requestActivate()
            root.dismissArmed = false
            dismissArmTimer.restart()
        } else {
            root.dismissArmed = false
            dismissArmTimer.stop()
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

        visible: true

        // Wallpaper base — always opaque so ApplicationWindow never shows through
        // launchpadFrame while it animates.
        Image {
            anchors.fill: parent
            z: -1
            enabled: false
            source: root._wallpaperSource
            fillMode: Image.PreserveAspectCrop
            smooth: true
            mipmap: true
        }

        // Dismiss tap area — excludes the top panel strip so topbar keeps its
        // own interactions. The dock zone is not excluded because DockWindow
        // (LayerTop) sits above this window and handles its own input.
        MouseArea {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: root.panelHeight
            anchors.bottom: parent.bottom
            z: 0
            enabled: root.dismissArmed
            onClicked: desktopScene.launchpadVisible = false
        }

        LaunchpadComp.Launchpad {
            id: launchpadContent
            z: 1
            anchors.fill: parent
            visible: root.isActive
            appsModel: root.appsModel
            topSafeInset: root.panelHeight
            revealProgress: 1.0
            bottomSafeInset: root.bottomSafeInset
            onDismissRequested: desktopScene.launchpadVisible = false
            onAppChosen: function(appData) {
                desktopScene.launchpadVisible = false
                root.appChosen(appData)
            }
            onAddToDockRequested: function(appData) { root.addToDockRequested(appData) }
            onAddToDesktopRequested: function(appData) { root.addToDesktopRequested(appData) }
        }
    }

    Component.onCompleted: {
        if (isActive) {
            dismissArmTimer.restart()
        }
    }

    Timer {
        id: dismissArmTimer
        interval: 180
        repeat: false
        onTriggered: root.dismissArmed = true
    }
}
