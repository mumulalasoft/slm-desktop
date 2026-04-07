import QtQuick 2.15
import QtQuick.Window 2.15
import Slm_Desktop
import "../dock" as DockComp

// DockWindow — persistent dock surface registered as wlr-layer-shell LayerTop.
//
// The compositor places LayerTop surfaces above all regular application windows
// AND above transient overlays (LaunchpadWindow, etc.) — no exclusion zones or
// dual-host tricks are needed.
//
//   Compositor z-order (high → low):
//     LayerTop: DockWindow          ← always on top
//     Normal:   LaunchpadWindow     ← above apps, below dock
//     Normal:   Application windows
//     LayerBottom / Shell window
//
// On non-Wayland or when the protocol is unavailable, the dock behaves as a
// regular frameless window (visible but not layer-shell managed).
Window {
    id: root

    required property var rootWindow
    required property var desktopScene
    required property var appsModel

    readonly property var dockItem: dockLoader.item
    readonly property var bootstrap: (typeof DockBootstrapState !== "undefined") ? DockBootstrapState : null
    readonly property bool layerShellAvailable: (typeof WlrLayerShell !== "undefined") && !!WlrLayerShell
    readonly property bool layerShellSupported: layerShellAvailable && !!WlrLayerShell.isSupported()
    readonly property bool dockLayerReady: !layerShellAvailable
                                           || !layerShellSupported
                                           || (bootstrap ? !!bootstrap.readyToRender : true)
    readonly property bool bootstrapSurfaceVisible: layerShellSupported && !root.dockLayerReady

    // ── Window setup ──────────────────────────────────────────────────────────

    color:           "transparent"
    flags:           Qt.FramelessWindowHint | Qt.WindowDoesNotAcceptFocus
    // Layer-shell surfaces must not have a transient parent.
    transientParent: null
    title:           "SLM Dock Surface"

    // Keep a non-user-visible mapped surface during bootstrap so wl_surface
    // exists and compositor can deliver first configure.
    visible: DockSystem.dockHostVisible && (root.dockLayerReady || root.bootstrapSurfaceVisible)

    // Initial size: full screen width, tall enough for dock + zoom headroom.
    // The compositor may override width via layer-shell configure.
    width:  rootWindow ? rootWindow.width  : Screen.width
    height: rootWindow ? rootWindow.height : Screen.height

    opacity: root.dockLayerReady ? 1.0 : 0.0

    // ── Layer-shell configuration ─────────────────────────────────────────────

    property bool layerConfigured: false

    // Exclusive zone = dock rendered height so the compositor reserves that
    // strip at the bottom for the dock and doesn't place windows over it.
    readonly property int exclusiveZone: Math.max(0, Math.ceil(dockLoader.item ? dockLoader.item.height : 0))

    function tryConfigureLayerShell() {
        if (root.layerConfigured) return
        if (typeof WlrLayerShell === "undefined" || !WlrLayerShell) return
        if (!WlrLayerShell.isSupported()) return

        // LayerTop = 2
        // AnchorBottom = 2, AnchorLeft = 4, AnchorRight = 8
        const ok = WlrLayerShell.configureAsLayerSurface(
            root,
            2,                    // LayerTop — above all app windows and transient overlays
            2|4|8,                // AnchorBottom | AnchorLeft | AnchorRight
            root.exclusiveZone,   // reserve dock height at bottom
            "slm-dock"
        )
        if (ok) {
            root.layerConfigured = true
            layerShellRetryTimer.stop()
            console.info("[DockWindow] layer surface configured at LayerTop"
                         + " exclusiveZone=" + root.exclusiveZone)
        }
    }

    // Keep the compositor's reserved strip in sync with the actual dock height.
    onExclusiveZoneChanged: {
        if (root.layerConfigured
                && typeof WlrLayerShell !== "undefined" && WlrLayerShell) {
            WlrLayerShell.setExclusiveZone(root, root.exclusiveZone)
        }
    }

    // Retry until the WlrLayerShell protocol is advertised by the compositor.
    Timer {
        id: layerShellRetryTimer
        interval: 50
        repeat: true
        running: !root.layerConfigured
        onTriggered: root.tryConfigureLayerShell()
    }

    onSceneGraphInitialized: root.tryConfigureLayerShell()

    // ── Dock renderer ─────────────────────────────────────────────────────────

    Loader {
        id: dockLoader
        anchors.fill: parent
        active: root.dockLayerReady
        asynchronous: false
        sourceComponent: Component {
            DockComp.Dock {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom

                hostName: "dock"
                acceptsInput: DockSystem.dockAcceptsInput
                rendererActive: root.visible && root.dockLayerReady && DockSystem.dockHostVisible
                visible: root.dockLayerReady
                opacity: 1.0
                appsModel: root.appsModel

                layoutIconSlotWidth:          Number(DockSystem.dockLayoutState.iconSlotWidth || 58)
                layoutItemSpacing:            Number(DockSystem.dockLayoutState.itemSpacing || 0)
                layoutEdgePadding:            Number(DockSystem.dockLayoutState.edgePadding || 6)
                layoutHoverLift:              Number(DockSystem.dockLayoutState.hoverLift || 6)
                layoutGlowWidth:              Number(DockSystem.dockLayoutState.glowWidth || 170)
                layoutMagnificationAmplitude: Number(DockSystem.dockLayoutState.magnificationAmplitude || 0)
                layoutMagnificationSigma:     Number(DockSystem.dockLayoutState.magnificationSigma || 148)

                onIconRectsChanged: function(rects) {
                    DockSystem.updateIconRects(rects)
                }
            }
        }
        onLoaded: {
            if (item) {
                DockSystem.registerDockItem(item)
            }
        }
    }

    onVisibleChanged: {
        if (bootstrap && bootstrap.setVisibleToUser) {
            bootstrap.setVisibleToUser(visible && root.dockLayerReady)
        }
    }

    Component.onCompleted: {
        console.info("[DockWindow] DOCK_CREATED ptr=" + root)
        DockSystem.bindContext(rootWindow, desktopScene)
        root.tryConfigureLayerShell()
    }
}
