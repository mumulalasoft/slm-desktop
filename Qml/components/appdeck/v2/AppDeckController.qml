// AppDeck v2 per-surface controller (docs/APPDECK.md §3, §16).
// Proxies ShellStateController + ShellPolicyController into the deckState /
// deckMode derivation that today lives inline at AppDeckWindow.qml:58–90.
// Note: this is NOT the global Qml/AppDeckController.qml singleton (which owns
// hover/drag/context interaction state). This object lives as a child of an
// AppDeckRoot instance and writes derived state back to it.
import QtQuick 2.15

QtObject {
    id: appDeckController

    // Wired by AppDeckWindow.qml — the surface state and policy come from
    // the host shell; the rootWindow gives us searchVisible / lock-state.
    property var root: null                 // AppDeckRoot instance
    property var rootWindow: null           // outer Shell window
    property var desktopScene: null         // for desktopScene.appdeckVisible fallback

    // §3 — policy gates
    readonly property var shellPolicy: (typeof ShellPolicyController !== "undefined")
                                       ? ShellPolicyController
                                       : null
    readonly property bool policySurfaceVisible: !shellPolicy
                                                 || shellPolicy.appDeckSurfaceVisible
    readonly property bool policyContentVisible: !shellPolicy
                                                 || shellPolicy.appDeckContentVisible
    readonly property bool policyAllowsExpandedModes: !shellPolicy
                                                      || !shellPolicy.visibilityPolicyName
                                                      || shellPolicy.visibilityPolicyName === "Normal"

    readonly property bool appDeckSuppressed: !rootWindow
                                              || rootWindow.lockScreenVisible
                                              || !policySurfaceVisible
    readonly property bool appDeckHidden: appDeckSuppressed

    // §4 — derived deckState (dock | grid)
    readonly property bool appDeckGridRequested: {
        if (appDeckHidden) {
            return false
        }
        if (!policyAllowsExpandedModes) {
            return false
        }
        if (rootWindow && rootWindow.searchVisible === true) {
            return true
        }
        if (typeof ShellStateController !== "undefined" && ShellStateController) {
            return ShellStateController.appdeckVisible === true
        }
        return desktopScene ? desktopScene.appdeckVisible === true : false
    }

    readonly property bool appDeckContextRequested: !appDeckHidden
                                                    && policyAllowsExpandedModes
                                                    && typeof AppDeckController !== "undefined"
                                                    && AppDeckController
                                                    && String(AppDeckController.contextItemId || "").length > 0

    readonly property string derivedDeckState: appDeckGridRequested || appDeckContextRequested
                                                ? "grid"
                                                : "dock"
    readonly property string derivedDeckMode: {
        if (derivedDeckState === "dock") {
            return "apps"
        }
        if (rootWindow && rootWindow.searchVisible === true) {
            return "pulse"
        }
        if (appDeckContextRequested) {
            return "context"
        }
        return "apps"
    }

    // When `driveRoot` is true, this controller becomes the SSOT and pushes
    // derived state into the AppDeckRoot. Tahap 1 keeps it false to avoid a
    // binding loop with the legacy `state`/`mode` properties on AppDeckWindow
    // that we still mirror into appDeckRoot for now. Later Tahap drops those
    // legacy bindings and flips driveRoot to true.
    property bool driveRoot: false

    onDerivedDeckStateChanged: if (driveRoot && root) root.deckState = derivedDeckState
    onDerivedDeckModeChanged:  if (driveRoot && root) root.deckMode  = derivedDeckMode
    Component.onCompleted: {
        if (driveRoot && root) {
            root.deckState = derivedDeckState
            root.deckMode  = derivedDeckMode
        }
    }

    // P0 — root-level hover state (spec §14). Delegates never write their own
    // hover; they read this. -1 means no icon under pointer or no info.
    // Meaningful primarily in grid mode (keyed by globalIndex). Dock hover
    // continues to be tracked via the global AppDeckController.hoveredItemId
    // singleton because dock anchors are keyed by id, not index.
    property int hoveredIconIndex: -1

    function _debug(msg) {
        if (root && root.debugLogsEnabled) {
            console.log("[AppDeckHover] " + msg)
        }
    }

    // P0 — spec §15, §16. Re-hit-tests the cached pointer X against the
    // appropriate anchor map (dock or grid) and pushes the resulting id into
    // the global AppDeckController singleton so dock/grid delegates pick up
    // hover even when the pointer never moved (post-morph state).
    function recomputeHover() {
        if (!root || !root.geometry) {
            hoveredIconIndex = -1
            return
        }
        // No hover info available — leave state alone (we don't know where
        // the pointer is). Worst case the user moves the mouse and Qt fires
        // pointer_motion which restores hover naturally.
        if (typeof AppDeckController === "undefined" || !AppDeckController) {
            return
        }
        if (!AppDeckController.dockHovered && root.deckState === "dock") {
            // Pointer not over the dock surface; keep hoveredIconIndex at -1
            // but don't clear the global hovered id — it may be intentional
            // (e.g. context menu owner).
            hoveredIconIndex = -1
            return
        }

        var px = Number(AppDeckController.hoverX || 0)
        var bestKey = ""
        var bestDist = Number.MAX_VALUE
        var bestIndex = -1

        if (root.deckState === "grid") {
            var gridMap = root.geometry.gridAnchorsByIndex
            for (var gk in gridMap) {
                if (!gridMap.hasOwnProperty(gk)) continue
                var gp = gridMap[gk]
                if (!gp || typeof gp.x !== "number") continue
                var gDist = Math.abs(gp.x - px)
                if (gDist < bestDist) {
                    bestDist = gDist
                    bestKey = gk
                    bestIndex = parseInt(gk, 10)
                }
            }
        } else {
            var dockMap = root.geometry.dockAnchorsById
            for (var dk in dockMap) {
                if (!dockMap.hasOwnProperty(dk)) continue
                var dp = dockMap[dk]
                if (!dp || typeof dp.x !== "number") continue
                var dDist = Math.abs(dp.x - px)
                if (dDist < bestDist) {
                    bestDist = dDist
                    bestKey = dk
                }
            }
        }

        hoveredIconIndex = bestIndex
        _debug("recompute deckState=" + root.deckState
               + " px=" + px
               + " bestKey=" + bestKey
               + " bestDist=" + Number(bestDist).toFixed(1)
               + " hoveredIconIndex=" + hoveredIconIndex)

        // Push the resolved id into the global singleton only if we actually
        // matched something. In grid mode `bestKey` is the integer index, so
        // we don't synthesise an itemId from it — grid delegates pick up
        // hover via the integer-keyed hoveredIconIndex instead.
        if (root.deckState === "dock" && bestKey.length > 0
                && AppDeckController.onHover) {
            AppDeckController.onHover(bestKey, px, AppDeckController.inputOwnerHost)
        }
    }

    // Wire recompute to the AppDeckRoot's morph-end signal. AppDeckWindow.qml
    // owns the Connections block that calls recomputeHover() (a QtObject has
    // no default property, so Connections cannot be a child here). See
    // AppDeckWindow.qml's `Connections { target: appDeckRoot }` block.
}
