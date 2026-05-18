// AppDeck v2 motion controller (docs/APPDECK.md §10).
// One QtObject exposing the spec API:
//   expandToGrid(gravityPoint)
//   collapseToDock(reason)
//   switchMode(mode)
//   revealDock()
//   hideDock()
// It retargets AppDeckRoot.morphAnimation (which already wires
// freezeAnchors / unfreezeAnchors via onStarted / onStopped, so callers do
// not have to remember to lock geometry themselves).
//
// Tahap 7 lands the API surface. Legacy AppDeckWindow.qml functions
// (enterGrid / enterDock / collapseToDock / dismissGridByPointer) keep
// existing for now — they continue to drive surfaceTransition through
// MotionController. Future tahap can swap their bodies to forward into
// AppDeckMotion, but doing so is a behavior change and is deliberately not
// part of Tahap 7.
import QtQuick 2.15

QtObject {
    id: motion

    // Wired by AppDeckWindow at instantiation.
    property var root: null       // AppDeckRoot instance
    property var geometry: null   // AppDeckGeometry instance

    function expandToGrid(gravityPoint) {
        if (!root || root.transitioning) {
            return
        }
        root.gravityCenter = gravityPoint
        root.deckState = "grid"
        var anim = root.morphAnimation
        if (anim) {
            anim.from = root.morphProgress
            anim.to = 1.0
            anim.start()
        }
    }

    function collapseToDock(reason) {
        if (!root) {
            return
        }
        if (root.deckState === "dock" && root.morphProgress === 0.0) {
            return
        }
        root.deckMode = "apps"
        var anim = root.morphAnimation
        if (anim) {
            anim.from = root.morphProgress
            anim.to = 0.0
            anim.start()
        }
        // reason is informational — emitted upstream as a log line
        console.log("[APPDECK-MOTION] collapseToDock reason=" + reason)
    }

    function switchMode(mode) {
        if (!root) {
            return
        }
        // §9 — mode swap inside state=grid keeps morphProgress at 1.0.
        root.deckMode = mode
    }

    function revealDock() {
        if (!root) return
        root.dockPresence = 1.0
    }

    function hideDock() {
        if (!root) return
        root.dockPresence = root.tokens
                ? root.tokens.autoHideMinPresence
                : 0.08
    }
}
