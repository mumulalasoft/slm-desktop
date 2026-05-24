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

    // P0 — signals so AppDeckWindow's legacy state machine (still the actual
    // morph driver until morphProgress is unbound from root.surfaceTransition)
    // can react to the spec API call sites. Direct callers that drive
    // morphAnim themselves can ignore the signals.
    signal toggleRequested(string reason)
    signal reverseRequested(string targetState)  // "grid" | "dock"

    function _debug(msg) {
        if (root && root.debugLogsEnabled) {
            console.log("[AppDeckMotion] " + msg)
        }
    }

    function expandToGrid(gravityPoint) {
        if (!root) {
            return
        }
        // P0: do NOT early-return on `transitioning` — callers (toggle /
        // reverse) decide whether to interrupt. The animation `from` is
        // re-pinned to the current morphProgress so retargets are smooth.
        root.gravityCenter = gravityPoint
        root.deckState = "grid"
        var anim = root.morphAnimation
        if (anim) {
            anim.stop()
            anim.from = root.morphProgress
            anim.to = 1.0
            anim.start()
        }
        _debug("expandToGrid gravity=" + gravityPoint + " from=" + root.morphProgress)
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
            anim.stop()
            anim.from = root.morphProgress
            anim.to = 0.0
            anim.start()
        }
        _debug("collapseToDock reason=" + reason + " from=" + root.morphProgress)
    }

    // P0 — spec §4. Routed through `toggleRequested` so the legacy state
    // machine in AppDeckWindow can dispatch enterGrid / enterDock; that path
    // is what actually moves the morph today. Once AppDeckRoot owns
    // morphProgress directly, this can call expandToGrid / collapseToDock
    // inline.
    function toggleAppDeck(reason) {
        if (!root) {
            return
        }
        _debug("toggle reason=" + reason
               + " deckState=" + root.deckState
               + " progress=" + Number(root.morphProgress).toFixed(3)
               + " transitioning=" + root.transitioning)
        if (root.transitioning) {
            interruptOrReverseTransition()
            return
        }
        motion.toggleRequested(String(reason || "button"))
    }

    // P0 — spec §5. When clicked mid-morph, retarget to the opposite endpoint
    // based on how far the morph has gotten. >= 0.5 means already closer to
    // grid → reverse to dock; < 0.5 means still closer to dock → reverse to
    // grid.
    function interruptOrReverseTransition() {
        if (!root) {
            return
        }
        var target = root.morphProgress >= 0.5 ? "dock" : "grid"
        _debug("interrupt reverse target=" + target
               + " progress=" + Number(root.morphProgress).toFixed(3))
        motion.reverseRequested(target)
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
