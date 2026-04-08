pragma Singleton

import QtQuick 2.15
import Slm_Desktop

// DockSystem — global dock state (single source of truth).
//
// Architecture: ONE DockWindow, ONE Dock renderer, ONE DockSystem.
//
//   KWin Compositor
//    ├─ DockWindow (wlr-layer-shell: TOP, AnchorBottom) ← single renderer
//    ├─ LaunchpadWindow                                  ← no dock here
//    └─ Application Windows
//
// Rules:
//  - No dual-host. No shell/launchpad host split.
//  - LaunchpadWindow does NOT contain a Dock.
//  - ShellRoot does NOT duplicate the Dock.
//  - DockWindow is the only place Dock lives.
QtObject {
    id: root

    // ── External context ──────────────────────────────────────────────────────

    // Bound from Main.qml at startup. Never changed after binding.
    property var rootWindowRef: null
    property var desktopSceneRef: null

    // The single Dock renderer item (set via registerDockItem from DockWindow).
    property var dockItemRef: null
    property var iconRects: []

    // ── Shell mode ────────────────────────────────────────────────────────────

    readonly property string shellMode: {
        var launchpadOn = desktopSceneRef
                ? (desktopSceneRef.launchpadVisible === true)
                : (ShellState.launchpadVisible === true)
        var workspaceOn = desktopSceneRef
                ? (desktopSceneRef.workspaceVisible === true)
                : (ShellState.workspaceOverviewVisible === true)
        if (launchpadOn)   return "launchpad"
        if (workspaceOn)   return "workspaceOverview"
        if (ShellState.showDesktop) return "showDesktop"
        return "normal"
    }

    readonly property bool launchpadActive: desktopSceneRef
                                            ? (desktopSceneRef.launchpadVisible === true)
                                            : (ShellState.launchpadVisible === true)

    // ── Presentation policy ───────────────────────────────────────────────────

    // Dock opacity is driven by ShellState, not by mode switching.
    // The dock surface is ALWAYS the same — it never hides/shows per mode.
    readonly property real dockOpacity: Number(ShellState.dockOpacity || 1.0)

    // Single host — always "dock". No shell/launchpad split.
    readonly property real  dockHostOpacity:  dockOpacity
    readonly property bool  dockHostVisible:  dockOpacity > 0.01
    readonly property bool  dockAcceptsInput: true

    // SSOT visibility flag (guide-required name, same as dockHostVisible).
    readonly property bool dockVisible: dockHostVisible

    // SSOT geometry — bounding rect of the dock surface on screen.
    readonly property rect dockRect: {
        var item = dockItemRef
        if (!item || !rootWindowRef) return Qt.rect(0, 0, 0, 0)
        var x = rootWindowRef ? Number(rootWindowRef.x || 0) : 0
        var y = rootWindowRef ? Number(rootWindowRef.y || 0) : 0
        var w = item.width  || 0
        var h = item.height || 0
        var winH = rootWindowRef ? Number(rootWindowRef.height || 0) : 0
        var margin = desktopSceneRef ? Number(desktopSceneRef.dockBottomMargin || 0) : 0
        return Qt.rect(
            x + Math.round((Number(rootWindowRef.width || 0) - w) / 2),
            y + Math.round(winH - h - margin),
            w, h
        )
    }

    // For DesktopScene geometry queries.
    readonly property var activeDockItem: dockItemRef

    // ── Layout state ──────────────────────────────────────────────────────────

    readonly property string dockIconSizePref: (typeof DesktopSettings !== "undefined" && DesktopSettings
                                                && DesktopSettings.dockIconSize !== undefined)
                                               ? String(DesktopSettings.dockIconSize) : "medium"
    readonly property bool dockMagnificationEnabled: (typeof DesktopSettings !== "undefined"
                                                      && DesktopSettings
                                                      && DesktopSettings.dockMagnificationEnabled !== undefined)
                                                     ? (DesktopSettings.dockMagnificationEnabled === true) : true

    function resolveIconSlotWidth(sizePref) {
        var sz = String(sizePref || "medium")
        if (sz === "small") return 42
        if (sz === "large") return 64
        return 52
    }

    readonly property var dockLayoutState: ({
        "screenGeometry": {
            "x":      rootWindowRef ? Number(rootWindowRef.x || 0) : 0,
            "y":      rootWindowRef ? Number(rootWindowRef.y || 0) : 0,
            "width":  rootWindowRef ? Number(rootWindowRef.width || 0) : 0,
            "height": rootWindowRef ? Number(rootWindowRef.height || 0) : 0
        },
        "dockBottomMargin":         desktopSceneRef ? Number(desktopSceneRef.dockBottomMargin || 0) : 0,
        "zoomHeadroom":             76,
        "iconSlotWidth":            resolveIconSlotWidth(dockIconSizePref),
        "itemSpacing":              0,
        "edgePadding":              4,
        "magnificationEnabled":     dockMagnificationEnabled,
        "magnificationAmplitude":   dockMagnificationEnabled ? 10 : 0,
        "magnificationSigma":       148,
        "hoverLift":                6,
        "glowWidth":                170,
        "alignment":                "center"
    })

    readonly property var dockCoreState: ({
        "preferences": {
            "hideMode": desktopSceneRef ? String(desktopSceneRef.dockHideMode || "duration_hide") : "duration_hide"
        },
        "visibilityBase":  ShellState.dockVisible,
        "hoveredItemId":   String(DockController.hoveredItemId || ""),
        "pressedItemId":   String(DockController.pressedItemId || ""),
        "activeItemId":    String(DockController.activeItemId || ""),
        "dragState": {
            "active":    DockController.dragActive === true,
            "itemId":    String(DockController.dragItemId || ""),
            "position":  Number(DockController.dragPosition || 0)
        }
    })

    readonly property var dockResolvedPresentation: ({
        "visible":          dockHostVisible,
        "opacity":          dockOpacity,
        "iconRects":        root.iconRects,
        "hitTestPolicy":    "dock",
        "effectiveGeometry": {
            "x":      rootWindowRef ? Number(rootWindowRef.x || 0) : 0,
            "y":      rootWindowRef ? Number(rootWindowRef.y || 0) : 0,
            "width":  rootWindowRef ? Number(rootWindowRef.width || 0) : 0,
            "height": rootWindowRef ? Number(rootWindowRef.height || 0) : 0
        }
    })

    // ── API ───────────────────────────────────────────────────────────────────

    function bindContext(rootWindow, desktopScene) {
        rootWindowRef    = rootWindow
        desktopSceneRef  = desktopScene
    }

    // Called by DockWindow.qml once on Component.onCompleted.
    function registerDockItem(item) {
        dockItemRef = item
    }

    // Called by Dock renderer when its icon rects change.
    function updateIconRects(rects) {
        iconRects = rects || []
    }

    // ── SSOT interaction API (renderer calls these, never sets state directly) ─

    function setHoveredItem(id) {
        DockController.hoveredItemId = String(id || "")
        DockController.dockHovered   = String(id || "").length > 0
    }

    function setPressedItem(id) {
        DockController.pressedItemId = String(id || "")
    }

    function clearInteraction() {
        DockController.onLeave("dock")
    }

    // ── Resolve helpers (called by the Dock renderer in DockWindow) ──────────

    function resolveNearestPinnedModelIndex(hostName, localX) {
        var rects = root.iconRects
        var x = Number(localX || 0)
        var bestDist = Number.MAX_VALUE
        var bestModelIndex = -1
        for (var i = 0; i < rects.length; ++i) {
            var r = rects[i]
            if (!r || r.pinned !== true) continue
            var cx = Number(r.x || 0) + (Number(r.width || 0) * 0.5)
            var dist = Math.abs(cx - x)
            if (dist < bestDist) {
                bestDist = dist
                bestModelIndex = Number(r.modelIndex)
            }
        }
        return bestModelIndex
    }

    function resolveInsertionPinnedPos(hostName, localX) {
        var rects = root.iconRects
        var centers = []
        for (var i = 0; i < rects.length; ++i) {
            var r = rects[i]
            if (!r || r.pinned !== true) continue
            centers.push(Number(r.x || 0) + (Number(r.width || 0) * 0.5))
        }
        centers.sort(function(a, b) { return a - b })
        if (centers.length <= 0) return 0
        var x = Number(localX || 0)
        for (var idx = 0; idx < centers.length; ++idx) {
            if (x < centers[idx]) return idx
        }
        return centers.length
    }

    function resolveMagnificationInfluence(hostName, iconCenterX, hoverX, hovered) {
        if (hovered !== true) return 0
        var layout = dockLayoutState || {}
        var amplitude = Number(layout.magnificationAmplitude || 0)
        var sigma     = Number(layout.magnificationSigma || 148)
        if (amplitude <= 0 || sigma <= 0) return 0
        var d = Number(iconCenterX || 0) - Number(hoverX || 0)
        return amplitude * Math.exp(-(d * d) / (2 * sigma * sigma))
    }

    function resolveItemCenterX(hostName, itemId) {
        var id = String(itemId || "")
        if (!id.length) return -1
        var rects = root.iconRects
        for (var i = 0; i < rects.length; ++i) {
            var r = rects[i]
            if (!r) continue
            if (String(r.id || "") !== id) continue
            return Number(r.x || 0) + (Number(r.width || 0) * 0.5)
        }
        return -1
    }

    function resolveNearestItemId(hostName, localX) {
        var rects = root.iconRects
        var x = Number(localX || 0)
        var bestDist = Number.MAX_VALUE
        var bestId = ""
        for (var i = 0; i < rects.length; ++i) {
            var r = rects[i]
            if (!r) continue
            var id = String(r.id || "")
            if (!id.length) continue
            var cx = Number(r.x || 0) + (Number(r.width || 0) * 0.5)
            var dist = Math.abs(cx - x)
            if (dist < bestDist) {
                bestDist = dist
                bestId = id
            }
        }
        return bestId
    }

    function resolveHitItemId(hostName, localX, localY) {
        var rects = root.iconRects
        var x = Number(localX || 0)
        var y = Number(localY || 0)
        for (var i = 0; i < rects.length; ++i) {
            var r = rects[i]
            if (!r) continue
            var rx = Number(r.x || 0)
            var ry = Number(r.y || 0)
            var rw = Number(r.width || 0)
            var rh = Number(r.height || 0)
            if (x >= rx && x <= (rx + rw) && y >= ry && y <= (ry + rh)) {
                return String(r.id || "")
            }
        }
        return ""
    }
}
