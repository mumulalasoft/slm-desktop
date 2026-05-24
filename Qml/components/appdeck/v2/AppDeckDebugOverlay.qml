// AppDeck v2 debug overlay (docs/APPDECK.md §17).
// Ctrl+Alt+D toggles a HUD showing the live morph state so agents and humans
// can confirm visually whether dock→grid is a real morph (continuous
// morphProgress) or a regression to fade swap. Outlines dockRect (magenta)
// and gridRect (cyan), drops dots at every cached dock/grid anchor.
import QtQuick 2.15
import Slm_Desktop

Item {
    id: dbg
    anchors.fill: parent
    z: 999
    visible: visibleOverlay

    property bool visibleOverlay: false
    // Wired by AppDeckWindow.
    property var appDeckRoot: null
    property var geometry: null
    // P0 — also wired so we can read hoveredIconIndex and confirm hover state
    // survives a morph with a stationary pointer.
    property var controller: null
    // Optional — input-region snapshot taken from AppDeckWindow.
    property int inputX: 0
    property int inputY: 0
    property int inputW: 0
    property int inputH: 0

    // P0 — global hover singleton readouts (id under pointer + last X).
    readonly property string hoveredItemId: (typeof AppDeckController !== "undefined"
                                              && AppDeckController)
                                              ? String(AppDeckController.hoveredItemId || "")
                                              : ""
    readonly property real hoverX: (typeof AppDeckController !== "undefined"
                                     && AppDeckController)
                                     ? Number(AppDeckController.hoverX || 0)
                                     : 0
    readonly property bool dockHovered: (typeof AppDeckController !== "undefined"
                                          && AppDeckController)
                                          ? !!AppDeckController.dockHovered
                                          : false

    Shortcut {
        sequence: "Ctrl+Alt+D"
        context: Qt.ApplicationShortcut
        onActivated: dbg.visibleOverlay = !dbg.visibleOverlay
    }

    Rectangle {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 24
        anchors.rightMargin: 24
        color: Qt.rgba(0, 0, 0, 0.8)
        radius: Theme.radiusMd
        width: hud.width + 24
        height: hud.height + 16

        Text {
            id: hud
            anchors.centerIn: parent
            color: Qt.rgba(1, 1, 1, 1)
            font.family: Theme.fontFamilyMonospace
            font.pixelSize: Theme.fontSize("small")
            textFormat: Text.PlainText
            text: {
                if (!dbg.appDeckRoot || !dbg.geometry) {
                    return "AppDeck v2 debug: (not wired)"
                }
                var anchorsCount = function(obj) {
                    if (!obj) return 0
                    var n = 0
                    for (var k in obj) { if (obj.hasOwnProperty(k)) ++n }
                    return n
                }
                var hoverIdx = dbg.controller ? dbg.controller.hoveredIconIndex : -1
                return "morphProgress: " + Number(dbg.appDeckRoot.morphProgress).toFixed(3) + "\n"
                     + "deckState:     " + dbg.appDeckRoot.deckState + "\n"
                     + "deckMode:      " + dbg.appDeckRoot.deckMode + "\n"
                     + "transitioning: " + dbg.appDeckRoot.transitioning + "\n"
                     + "dockPresence:  " + Number(dbg.appDeckRoot.dockPresence).toFixed(3) + "\n"
                     + "dockRect:      " + dbg.geometry.dockRect + "\n"
                     + "gridRect:      " + dbg.geometry.gridRect + "\n"
                     + "anchors:       " + anchorsCount(dbg.geometry.dockAnchorsById)
                     + "/" + anchorsCount(dbg.geometry.gridAnchorsByIndex) + "\n"
                     + "inputRegion:   " + dbg.inputX + "," + dbg.inputY
                     + " " + dbg.inputW + "x" + dbg.inputH + "\n"
                     + "hoveredIdx:    " + hoverIdx + "\n"
                     + "hoveredItemId: " + (dbg.hoveredItemId || "(none)") + "\n"
                     + "hoverX:        " + Number(dbg.hoverX).toFixed(1)
                     + (dbg.dockHovered ? " [dockHovered]" : "")
            }
        }
    }

    // dockRect outline (magenta — debug only, intentional fixed RGB)
    Rectangle {
        visible: !!dbg.geometry
        x: dbg.geometry ? dbg.geometry.dockRect.x : 0
        y: dbg.geometry ? dbg.geometry.dockRect.y : 0
        width:  dbg.geometry ? dbg.geometry.dockRect.width  : 0
        height: dbg.geometry ? dbg.geometry.dockRect.height : 0
        color: "transparent"
        border.color: Qt.rgba(1, 0, 1, 1)
        border.width: Theme.borderWidthThin
    }

    // gridRect outline (cyan — debug only, intentional fixed RGB)
    Rectangle {
        visible: !!dbg.geometry
        x: dbg.geometry ? dbg.geometry.gridRect.x : 0
        y: dbg.geometry ? dbg.geometry.gridRect.y : 0
        width:  dbg.geometry ? dbg.geometry.gridRect.width  : 0
        height: dbg.geometry ? dbg.geometry.gridRect.height : 0
        color: "transparent"
        border.color: Qt.rgba(0, 1, 1, 1)
        border.width: Theme.borderWidthThin
    }

    // P0 — input region outline (yellow). Confirms wl_surface input_region
    // tracks dock/grid state, and that auto-hide low-presence keeps a usable
    // reveal zone instead of dropping the region entirely.
    Rectangle {
        visible: dbg.inputW > 0 && dbg.inputH > 0
        x: dbg.inputX
        y: dbg.inputY
        width: dbg.inputW
        height: dbg.inputH
        color: "transparent"
        border.color: Qt.rgba(1, 1, 0, 1)
        border.width: Theme.borderWidthThin
    }

    // P0 — pointer cross-hair at last cached hoverX (green vertical line at
    // dock height). Helps confirm hover hit-testing is using the right X.
    Rectangle {
        visible: dbg.hoverX > 0 && !!dbg.geometry
        x: dbg.hoverX - 1
        y: dbg.geometry ? dbg.geometry.dockRect.y : 0
        width: 2
        height: dbg.geometry ? dbg.geometry.dockRect.height : 0
        color: Qt.rgba(0, 1, 0, 0.7)
    }
}
