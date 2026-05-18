// AppDeck v2 geometry model (docs/APPDECK.md §5, §12).
// Single source of truth for:
//   - dockRect / gridRect — surface-space bounds of the dock pill and grid panel
//   - dockAnchorsById / gridAnchorsByIndex — per-icon anchor points in surface
//     coords, consumed by IconMorphLayer (Tahap 4)
//   - freezeAnchors/unfreezeAnchors — anchor snapshot lock during morph (§12)
// This object holds NO visual state — it just transforms inputs (icon rects
// from AppDeck.qml, grid layout from AppDeckGridView) into a unified anchor
// space for the morph layer to interpolate.
import QtQuick 2.15

QtObject {
    id: geometry

    // Wired by parent (AppDeckRoot/AppDeckWindow). These refs let us
    // mapToItem-transform dock and grid local coordinates into the surface
    // root coordinate space that IconMorphLayer renders in.
    property var dockHostRef: null      // AppDeck.qml root (dockView.dockItem)
    property var gridHostRef: null      // AppDeckGridAppsView
    property var rootSurfaceRef: null   // AppDeckRoot Item

    // Surface-space rectangles.
    property rect dockRect: Qt.rect(0, 0, 1, 1)
    property rect gridRect: Qt.rect(0, 0, 1, 1)

    // Anchor caches. Keys are stable: dockAnchorsById is keyed by the id
    // string AppDeck.collectIconRects emits (desktopFile / name / fallback
    // index). gridAnchorsByIndex is keyed by globalIndex (current grid page).
    property var dockAnchorsById: ({})
    property var gridAnchorsByIndex: ({})

    // §12 — when frozen, updateDockAnchors / updateGridAnchors are no-ops, so
    // dockAnchorFor / gridAnchorFor return the values captured at freeze time.
    // A pending flag remembers that a recompute is owed once we unfreeze.
    property bool frozen: false
    property bool _recomputePending: false

    function lerp(a, b, t) {
        return a + (b - a) * t
    }

    function pointLerp(a, b, t) {
        return Qt.point(lerp(a.x, b.x, t), lerp(a.y, b.y, t))
    }

    function dockAnchorFor(idOrIndex) {
        var key = String(idOrIndex)
        var p = dockAnchorsById[key]
        if (p && typeof p.x === "number") {
            return Qt.point(p.x, p.y)
        }
        // Fallback to dock-rect center so the icon at least morphs from a
        // sensible point rather than (0,0) before anchors populate.
        return Qt.point(dockRect.x + dockRect.width  / 2,
                        dockRect.y + dockRect.height / 2)
    }

    function gridAnchorFor(idOrIndex) {
        var key = String(idOrIndex)
        var p = gridAnchorsByIndex[key]
        if (p && typeof p.x === "number") {
            return Qt.point(p.x, p.y)
        }
        return Qt.point(gridRect.x + gridRect.width  / 2,
                        gridRect.y + gridRect.height / 2)
    }

    function freezeAnchors() {
        frozen = true
    }

    function unfreezeAnchors() {
        frozen = false
        if (_recomputePending) {
            _recomputePending = false
            recompute()
        }
    }

    function recompute() {
        if (frozen) {
            _recomputePending = true
            return
        }
        _updateDockRect()
        _updateGridRect()
        _updateDockAnchorsFromCache()
        _updateGridAnchorsFromLayout()
    }

    // Called by AppDeckWindow when AppDeck.iconRectsChanged fires. The rects
    // are in dockHostRef-local coordinates; we transform once into surface
    // space.
    function applyDockIconRects(rects) {
        if (frozen) {
            _recomputePending = true
            return
        }
        if (!dockHostRef || !rootSurfaceRef || !rects) {
            return
        }
        var out = {}
        for (var i = 0; i < rects.length; ++i) {
            var r = rects[i]
            if (!r) continue
            var center = dockHostRef.mapToItem(rootSurfaceRef,
                                               r.x + r.width  / 2,
                                               r.y + r.height / 2)
            out[String(r.id)] = center
        }
        dockAnchorsById = out
    }

    // Called by AppDeckWindow when AppDeckGridAppsView.gridLayoutSettled fires.
    // We probe gridHostRef.gridIconCenterFor(idx) for every globalIndex in the
    // visible page range; the function returns (-1,-1) for off-page indices,
    // which we exclude.
    function refreshGridAnchorsForRange(startIndex, endIndex) {
        if (frozen) {
            _recomputePending = true
            return
        }
        if (!gridHostRef || !rootSurfaceRef
                || typeof gridHostRef.gridIconCenterFor !== "function") {
            return
        }
        var out = {}
        for (var i = startIndex; i <= endIndex; ++i) {
            var local = gridHostRef.gridIconCenterFor(i)
            if (local.x < 0) continue
            // gridHostRef returns coords in AppDeckGridAppsView root space.
            var surf = gridHostRef.mapToItem(rootSurfaceRef, local.x, local.y)
            out[String(i)] = surf
        }
        gridAnchorsByIndex = out
    }

    // --- internals ---

    function _updateDockRect() {
        if (!dockHostRef || !rootSurfaceRef) return
        var tl = dockHostRef.mapToItem(rootSurfaceRef, 0, 0)
        dockRect = Qt.rect(tl.x, tl.y,
                           dockHostRef.width  || 0,
                           dockHostRef.height || 0)
    }

    function _updateGridRect() {
        if (!gridHostRef || !rootSurfaceRef) return
        var tl = gridHostRef.mapToItem(rootSurfaceRef, 0, 0)
        gridRect = Qt.rect(tl.x, tl.y,
                           gridHostRef.width  || 0,
                           gridHostRef.height || 0)
    }

    function _updateDockAnchorsFromCache() {
        if (!dockHostRef) return
        var rects = dockHostRef.iconRectsCache
        if (rects && rects.length > 0) {
            applyDockIconRects(rects)
        }
    }

    function _updateGridAnchorsFromLayout() {
        // Best-effort: probe a generous range. IconMorphLayer (Tahap 4) will
        // refine the range to match its actual model length.
        refreshGridAnchorsForRange(0, 47)
    }
}
