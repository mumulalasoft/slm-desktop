// AppDeck v2 visual icon delegate (docs/APPDECK.md §7).
// Rendered ONLY by IconMorphLayer — not by the dock or grid chrome. This
// is the "single delegate per app" the spec mandates: the same Item moves
// from dockAnchor to gridAnchor under morphProgress.
//
// Drag/drop/reorder logic remains on the dock-chrome side (AppDeckAppDelegate
// inside AppDeck.qml) because that interaction is anchored to dock-row layout
// math. Click activation, however, lives here so the morph layer is the only
// surface that needs hit testing during transitions.
import QtQuick 2.15
import Slm_Desktop
import ".." as AppDeckComp

Item {
    id: iconDelegate

    property var appData: null
    property string display: appData ? String(appData.display || appData.name || "") : ""
    property string iconSource: appData ? String(appData.icon || "") : ""
    property bool running: appData ? !!appData.running : false

    // Icon visual sizing — IconMorphLayer scales via `scale`, so width/height
    // here are the base (dock-time) footprint. Grid-time presentation is the
    // same delegate with a bigger `scale`.
    implicitWidth: 56
    implicitHeight: 56

    signal activated(var appData)

    AppDeckComp.AppDeckItem {
        id: visual
        anchors.fill: parent
        iconPath: iconDelegate.iconSource
        label: iconDelegate.display
        showRunningDot: iconDelegate.running
        // AppDeckItem already handles hover + press visuals; we re-emit
        // activation up to IconMorphLayer.
        onClicked: iconDelegate.activated(iconDelegate.appData)
    }
}
