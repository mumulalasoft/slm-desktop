// AppDeck v2 morph container (docs/APPDECK.md §6, §11).
// One Rectangle whose x/y/width/height/radius/color are pure functions of
// morphProgress lerping sourceRect→targetRect. Per spec §11, this container
// MUST NOT carry per-property Behavior blocks — the upstream NumberAnimation
// on AppDeckRoot.morphProgress is the single source of motion; everything
// else follows declaratively. Children of MorphContainer (gradient highlight,
// top seam, etc.) attach via the default property and clip to its bounds.
import QtQuick 2.15

Rectangle {
    id: morphContainer

    // §6 — endpoint rects. Both are in the same coordinate space as the
    // parent (typically the AppDeck surface root).
    property rect sourceRect: Qt.rect(0, 0, 1, 1)
    property rect targetRect: Qt.rect(0, 0, 1, 1)

    // §11 — driving signal; bound to AppDeckRoot.morphProgress upstream.
    property real morphProgress: 0.0

    // §6 — corner radius endpoints; lerped just like geometry.
    property real sourceRadius: 28.0
    property real targetRadius: 22.0

    // §6 — color endpoints; mixed by progress.
    property color sourceColor: "#ffffff"
    property color targetColor: "#ffffff"

    readonly property real _t: Math.max(0.0, Math.min(1.0, morphProgress))

    function _lerp(a, b, t) { return a + (b - a) * t }
    function _mix(a, b, t) {
        return Qt.rgba(a.r + ((b.r - a.r) * t),
                       a.g + ((b.g - a.g) * t),
                       a.b + ((b.b - a.b) * t),
                       a.a + ((b.a - a.a) * t))
    }

    x: _lerp(sourceRect.x, targetRect.x, _t)
    y: _lerp(sourceRect.y, targetRect.y, _t)
    width:  _lerp(sourceRect.width,  targetRect.width,  _t)
    height: _lerp(sourceRect.height, targetRect.height, _t)
    radius: _lerp(sourceRadius, targetRadius, _t)
    color:  _mix(sourceColor, targetColor, _t)
}
