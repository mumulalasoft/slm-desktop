// AppDeck v2 pulse mode layer (docs/APPDECK.md §9).
// Pulse is a MODE within state=grid — never a separate Window/surface.
// morphProgress stays at 1.0 while the mode swap cross-fades. This layer
// holds the pulse search UI (AppDeckContextView in pulse parameters) and
// is shown/hidden via opacity, not destroy/recreate.
import QtQuick 2.15
import Slm_Desktop
import ".." as AppDeckComp

Item {
    id: pulseLayer
    anchors.fill: parent

    // Wired by AppDeckWindow:
    //   deckMode    — current AppDeckRoot mode (apps|pulse|context)
    //   pulseResultsModel — model fed to AppDeckContextView
    //   contextSurfaceX/Y/W/H — geometry for the contextView container
    property string deckMode: "apps"
    property var pulseResultsModel: null
    property real contextSurfaceX: 0
    property real contextSurfaceY: 0
    property real contextSurfaceW: 0
    property real contextSurfaceH: 0
    property string searchSeed: ""

    readonly property bool active: deckMode === "pulse"

    signal queryChanged(string query)
    signal collapseRequested()

    // Spec §9: opacity-only mode transition; no morph involvement.
    opacity: active ? 1.0 : 0.0
    visible: opacity > 0.01
    // P0 (§12 of fix prompt) — a layer fading below 5% must NOT eat clicks
    // intended for the layer underneath. `enabled:false` on an Item blocks
    // event delivery to its descendant MouseAreas / TapHandlers.
    enabled: visible && opacity > 0.05

    Behavior on opacity {
        NumberAnimation { duration: Theme.durationNormal; easing.type: Theme.easingLight }
    }

    AppDeckComp.AppDeckContextView {
        id: pulseView
        anchors.fill: parent
        pulseResultsModel: pulseLayer.pulseResultsModel
        currentQuery: pulseLayer.searchSeed
        active: pulseLayer.active
        onQueryChanged: function(q) { pulseLayer.queryChanged(q) }
        onCollapseRequested: pulseLayer.collapseRequested()
    }
}
