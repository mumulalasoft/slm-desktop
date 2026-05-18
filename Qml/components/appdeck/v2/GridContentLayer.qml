// AppDeck v2 grid content reveal layer (docs/APPDECK.md §8).
// Provides the canonical "content after morph" stagger curve so search bar,
// category tabs, and other grid chrome reveal at the right beat:
//
//   morphProgress 0.00–0.05: hidden (visible:false to skip layout work)
//   morphProgress 0.05–0.45: still hidden but visible:true so renderers warm
//   morphProgress 0.45–0.80: opacity 0→1, scale 0.96→1.0
//   morphProgress 0.80–1.00: settled at 1.0
//
// Use as a wrapper Item around grid header/search/category rows. Tahap 5 also
// re-implements the same curve as a re-usable function on the root for code
// that wants the reveal value without inserting a wrapper.
import QtQuick 2.15

Item {
    id: layer
    // Caller is responsible for positioning + sizing this Item (works inside
    // a Layout or with explicit anchors). The layer applies the reveal curve
    // to its opacity + transform; children compose normally.

    property real morphProgress: 0.0
    // The "feet" of the reveal curve. Defaults follow spec §8 exactly; expose
    // them so individual sections (e.g. footer) can be staggered later.
    property real revealStart: 0.45
    property real revealEnd:   0.80
    property real visibleAt:   0.05
    // Caller-controlled gate (defaults true). When false the layer is fully
    // hidden regardless of morphProgress — used by sections whose own logic
    // suppresses them (favorites row when filterText is non-empty, etc.).
    property bool gateVisible: true

    readonly property real _t: Math.max(0.0, Math.min(1.0, morphProgress))
    readonly property real revealOpacity: {
        if (_t <= revealStart) return 0.0
        if (_t >= revealEnd)   return 1.0
        return (_t - revealStart) / Math.max(0.001, revealEnd - revealStart)
    }
    readonly property real revealScale: 0.96 + (1.0 - 0.96) * revealOpacity

    visible: gateVisible && _t > visibleAt
    opacity: gateVisible ? revealOpacity : 0.0
    transform: Scale {
        origin.x: layer.width  / 2
        origin.y: 0
        xScale: layer.revealScale
        yScale: layer.revealScale
    }
}
