pragma Singleton

import QtQuick 2.15
import Slm_Desktop

// DockSystem — global dock state resolver (single source of truth).
//
// Single-renderer model:
// - One DockWindow renders one Dock instance across all shell modes.
// - DockSystem remains source of truth for layout/policy/interaction resolve.
QtObject {
    id: root

    // External context binding (set from Main.qml)
    property var rootWindowRef: null
    property var desktopSceneRef: null

    // Host dock items (thin visual hosts only)
    property var hostDockItems: ({})
    property var hostIconRectsByHost: ({})
    property real launchpadRevealProgress: launchpadTargetVisible ? 1.0 : 0.0

    // Shell mode (global)
    readonly property string shellMode: {
        var launchpadVisibleNow = desktopSceneRef
                ? (desktopSceneRef.launchpadVisible === true)
                : (ShellState.launchpadVisible === true)
        var workspaceVisibleNow = desktopSceneRef
                ? (desktopSceneRef.workspaceVisible === true)
                : (ShellState.workspaceOverviewVisible === true)
        if (launchpadVisibleNow) return "launchpad"
        if (workspaceVisibleNow) return "workspaceOverview"
        if (ShellState.showDesktop) return "showDesktop"
        return "normal"
    }

    readonly property bool launchpadTargetVisible: desktopSceneRef
                                                   ? (desktopSceneRef.launchpadVisible === true)
                                                   : (ShellState.launchpadVisible === true)

    // Dock animation phase (single pipeline owner)
    readonly property string phase: {
        if (launchpadTargetVisible) {
            return launchpadRevealProgress >= 0.995 ? "launchpadActive" : "enteringLaunchpad"
        }
        return launchpadRevealProgress <= 0.005 ? "idle" : "leavingLaunchpad"
    }

    // Presentation policy
    readonly property real dockOpacity: Number(ShellState.dockOpacity || 1.0)
    // Mode-driven host policy without dock reveal lag:
    // - launchpad mode switches host instantly
    // - shell host stays visible until launchpad host is registered (no gap)
    readonly property bool launchpadHostReady: !!hostDockItems["launchpad"]
    readonly property real shellHostOpacity: {
        if (!launchpadTargetVisible) {
            return dockOpacity
        }
        return launchpadHostReady ? 0.0 : dockOpacity
    }
    readonly property real launchpadHostOpacity: {
        if (!launchpadTargetVisible) {
            return 0.0
        }
        return launchpadHostReady ? dockOpacity : 0.0
    }
    readonly property bool shellHostVisible: dockResolvedPresentation.visible && shellHostOpacity > 0.01
    readonly property bool launchpadHostVisible: dockResolvedPresentation.visible && launchpadHostOpacity > 0.01
    readonly property bool shellAcceptsInput: activeHostName() === "shell"
    readonly property bool launchpadAcceptsInput: activeHostName() === "launchpad"

    // ─── Layout constants ──────────────────────────────────────────────────
    readonly property int zoomHeadroom: 76
    readonly property string dockIconSizePref: (typeof UIPreferences !== "undefined" && UIPreferences
                                                && UIPreferences.dockIconSize !== undefined)
                                               ? String(UIPreferences.dockIconSize) : "medium"
    readonly property bool dockMagnificationEnabledPref: (typeof UIPreferences !== "undefined"
                                                          && UIPreferences
                                                          && UIPreferences.dockMagnificationEnabled !== undefined)
                                                         ? (UIPreferences.dockMagnificationEnabled === true) : true

    function resolveIconSlotWidth(sizePref) {
        var sz = String(sizePref || "medium")
        if (sz === "small") return 48
        if (sz === "large") return 72
        return 58
    }

    // Minimal unified layout state (shared by both hosts)
    readonly property var dockLayoutState: ({
        "screenGeometry": {
            "x": rootWindowRef ? Number(rootWindowRef.x || 0) : 0,
            "y": rootWindowRef ? Number(rootWindowRef.y || 0) : 0,
            "width": rootWindowRef ? Number(rootWindowRef.width || 0) : 0,
            "height": rootWindowRef ? Number(rootWindowRef.height || 0) : 0
        },
        "dockBottomMargin": desktopSceneRef ? Number(desktopSceneRef.dockBottomMargin || 0) : 0,
        "zoomHeadroom": zoomHeadroom,
        "iconSlotWidth": resolveIconSlotWidth(dockIconSizePref),
        "itemSpacing": 0,
        "edgePadding": 6,
        "magnificationEnabled": dockMagnificationEnabledPref,
        "magnificationAmplitude": dockMagnificationEnabledPref ? 10 : 0,
        "magnificationSigma": 148,
        "hoverLift": 6,
        "glowWidth": 170,
        "alignment": "center"
    })

    // Core/system states (v1 lightweight representation)
    readonly property var dockCoreState: ({
        "preferences": {
            "hideMode": desktopSceneRef ? String(desktopSceneRef.dockHideMode || "duration_hide") : "duration_hide"
        },
        "visibilityBase": ShellState.dockVisible,
        "hoveredItemId": String(DockController.hoveredItemId || ""),
        "pressedItemId": String(DockController.pressedItemId || ""),
        "activeItemId": String(DockController.activeItemId || ""),
        "dragState": {
            "active": DockController.dragActive === true,
            "itemId": String(DockController.dragItemId || ""),
            "position": Number(DockController.dragPosition || 0)
        }
    })

    readonly property var dockAnimationState: ({
        "phase": phase,
        "opacity": dockOpacity,
        "scale": 1.0,
        "offsetY": 0,
        "transitionProgress": launchpadRevealProgress
    })

    readonly property var dockResolvedPresentation: ({
        "visible": dockOpacity > 0.01,
        "opacity": dockOpacity,
        "scale": 1.0,
        "offsetY": 0,
        "blurAmount": 0,
        "backgroundStyle": "default",
        "hitTestPolicy": "dock",
        "iconRects": iconRectsForHost(activeHostName()),
        "effectiveGeometry": {
            "x": rootWindowRef ? Number(rootWindowRef.x || 0) : 0,
            "y": rootWindowRef ? Number(rootWindowRef.y || 0) : 0,
            "width": rootWindowRef ? Number(rootWindowRef.width || 0) : 0,
            "height": rootWindowRef ? Number(rootWindowRef.height || 0) : 0
        }
    })

    readonly property var activeDockItem: hostDockItems[activeHostName()] || null

    function bindContext(rootWindow, desktopScene) {
        rootWindowRef = rootWindow
        desktopSceneRef = desktopScene
    }

    function setLaunchpadRevealProgress(progress) {
        // Compatibility no-op: dock state machine is now strictly mode-driven
        // from launchpadTargetVisible (0/1 instant), not by reveal animation.
        void progress
    }

    function registerHostDockItem(hostName, dockItem) {
        var key = String(hostName || "")
        if (!key.length) {
            key = "dock"
        }
        var next = {}
        for (var existing in hostDockItems) {
            if (hostDockItems.hasOwnProperty(existing)) {
                next[existing] = hostDockItems[existing]
            }
        }
        next[key] = dockItem
        hostDockItems = next
        syncInputOwner()
    }

    function updateHostIconRects(hostName, rects) {
        var key = String(hostName || "")
        if (!key.length) {
            key = "dock"
        }
        var next = {}
        for (var existing in hostIconRectsByHost) {
            if (hostIconRectsByHost.hasOwnProperty(existing)) {
                next[existing] = hostIconRectsByHost[existing]
            }
        }
        next[key] = rects || []
        hostIconRectsByHost = next
        syncInputOwner()
    }

    function activeHostName() {
        if (launchpadTargetVisible && launchpadHostReady) {
            return "launchpad"
        }
        return "shell"
    }

    function iconRectsForHost(hostName) {
        var key = String(hostName || "")
        if (!key.length) {
            key = activeHostName()
        }
        if (hostIconRectsByHost[key] !== undefined) {
            return hostIconRectsByHost[key]
        }
        return []
    }

    function resolveHitItemId(hostName, localX, localY) {
        var rects = iconRectsForHost(hostName)
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

    function resolveNearestPinnedModelIndex(hostName, localX) {
        var rects = iconRectsForHost(hostName)
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
        var rects = iconRectsForHost(hostName)
        var centers = []
        for (var i = 0; i < rects.length; ++i) {
            var r = rects[i]
            if (!r || r.pinned !== true) continue
            centers.push(Number(r.x || 0) + (Number(r.width || 0) * 0.5))
        }
        centers.sort(function(a, b) { return a - b })
        if (centers.length <= 0) {
            return 0
        }
        var x = Number(localX || 0)
        for (var idx = 0; idx < centers.length; ++idx) {
            if (x < centers[idx]) {
                return idx
            }
        }
        return centers.length
    }

    function resolveMagnificationInfluence(hostName, iconCenterX, hoverX, hovered) {
        if (hovered !== true) {
            return 0
        }
        var layout = dockLayoutState || {}
        var amplitude = Number(layout.magnificationAmplitude || 0)
        var sigma = Number(layout.magnificationSigma || 148)
        if (amplitude <= 0 || sigma <= 0) {
            return 0
        }
        var center = Number(iconCenterX || 0)
        var hover = Number(hoverX || 0)
        var d = center - hover
        return amplitude * Math.exp(-(d * d) / (2 * sigma * sigma))
    }

    function resolveItemCenterX(hostName, itemId) {
        var id = String(itemId || "")
        if (!id.length) {
            return -1
        }
        var rects = iconRectsForHost(hostName)
        for (var i = 0; i < rects.length; ++i) {
            var r = rects[i]
            if (!r) continue
            if (String(r.id || "") !== id) continue
            return Number(r.x || 0) + (Number(r.width || 0) * 0.5)
        }
        return -1
    }

    function resolveNearestItemId(hostName, localX) {
        var rects = iconRectsForHost(hostName)
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

    function syncInputOwner() {
        var owner = activeHostName()
        DockController.setInputOwner(owner)
        var hoveredId = String(DockController.hoveredItemId || "")
        var hoverPos = Number(DockController.lastHoverPosition || 0)
        if (hoveredId.length > 0) {
            var cx = resolveItemCenterX(owner, hoveredId)
            if (cx >= 0) {
                DockController.onHover(hoveredId, cx, owner)
                return
            }
        }
        if (hoverPos > 0) {
            var nearestId = resolveNearestItemId(owner, hoverPos)
            if (nearestId.length > 0) {
                DockController.onHover(nearestId, hoverPos, owner)
            }
        }
    }

    onShellModeChanged: {
        syncInputOwner()
    }
    onLaunchpadRevealProgressChanged: syncInputOwner()
    onHostDockItemsChanged: syncInputOwner()
    onHostIconRectsByHostChanged: syncInputOwner()
}
