// AppDeck v2 root state owner (docs/APPDECK.md §4, §10, §11).
// Single source of truth for deckState, deckMode, morphProgress, transitioning
// and the morph driver animation. Anchors fill the parent (AppDeckWindow inner
// surface). State changes flow through AppDeckMotion (Tahap 7) which retargets
// morphAnim; reading code should bind to morphProgress / deckState rather than
// driving them directly.
import QtQuick 2.15
import Slm_Desktop

Item {
    id: appDeckRoot
    anchors.fill: parent

    // §4 — primary state
    property string deckState: "dock"          // dock | grid
    property string deckMode: "apps"           // apps | pulse | context
    property string appearanceMode: "always_show" // always_show | auto_hide

    // §10 — animation lock + morph driver value
    property bool transitioning: false
    property real morphProgress: 0.0            // 0=dock, 1=grid

    // §10 — gravity point for expand origin (set by Motion.expandToGrid)
    property point gravityCenter: Qt.point(width / 2, height)

    // P0 — emitted when a transition ends (animation finished, watchdog reset,
    // or `transitioning` flips to false from any binding source). Consumers
    // (AppDeckWindow, the v2 controller) re-hit-test the pointer + refresh the
    // layer-shell input region so a stationary pointer still gets correct hover
    // after a morph that didn't fire pointer_motion events.
    signal hoverRecomputeRequested()

    // P0 — emitted when transitionWatchdog forces transitioning to false. The
    // SSOT for `transitioning` may be bound from outside (AppDeckWindow.root);
    // a direct write would be overridden by the binding, so we signal upward
    // instead and the binding owner clears its own copy.
    signal watchdogReset()

    // Debug logging gate (§19 / SLM_APPDECK_DEBUG). Settable by parent so the
    // shell can wire it from a context property or a command-line probe; no
    // direct env-var access from QML.
    property bool debugLogsEnabled: false

    // §15 — auto-hide presence. 1.0 = fully revealed, autoHideMinPresence
    // (token 0.08) = low-presence edge glow. Per spec §15, the surface MUST
    // never go visible:false — only fade to the low-presence floor so the
    // reveal zone keeps receiving pointer events.
    property real dockPresence: 1.0
    Behavior on dockPresence {
        NumberAnimation {
            duration: tokensInstance.autoShowDuration
            easing.type: Theme.easingLight
        }
    }

    // Local tokens object — children read durations / endpoints via
    // appDeckRoot.tokens.*. Kept as a child rather than a singleton to avoid
    // QT_QML_SINGLETON_TYPE registration ceremony for a v2-only construct.
    readonly property alias tokens: tokensInstance
    AppDeckTokens { id: tokensInstance }

    // §11 — emitted when morph animation finishes (parent listens to retarget
    // input region, refresh anchors, etc.)
    signal morphFinished(string finalState)

    // §11 — the ONE animation that drives every visual morph. All other
    // properties (container size, icon anchor, content reveal) must bind to
    // morphProgress, not run their own NumberAnimation.
    // §12 — geometry handle wired by parent (AppDeckWindow). Used to freeze
    // anchors during a morph so mid-flight reflow can't teleport icons.
    property var geometry: null

    NumberAnimation {
        id: morphAnim
        target: appDeckRoot
        property: "morphProgress"
        duration: tokensInstance.morphDuration
        easing.type: Theme.easingDecelerate
        onStarted: {
            if (appDeckRoot.geometry && typeof appDeckRoot.geometry.freezeAnchors === "function") {
                appDeckRoot.geometry.freezeAnchors()
            }
            appDeckRoot.transitioning = true
        }
        onStopped: {
            appDeckRoot.transitioning = false
            if (appDeckRoot.geometry && typeof appDeckRoot.geometry.unfreezeAnchors === "function") {
                appDeckRoot.geometry.unfreezeAnchors()
            }
            appDeckRoot.morphFinished(
                appDeckRoot.morphProgress >= 0.5 ? "grid" : "dock")
        }
    }

    // P0 watchdog — prevents `transitioning` from latching true if the morph
    // ends without onStopped (animation dequeued, MotionController stall,
    // legacy fallback path that bypasses morphAnim entirely). Restarted on
    // every transitioning rising edge; cleared on falling edge.
    Timer {
        id: transitionWatchdog
        interval: tokensInstance.morphDuration + 220
        repeat: false
        onTriggered: {
            if (!appDeckRoot.transitioning) {
                return
            }
            if (appDeckRoot.debugLogsEnabled) {
                console.warn("[AppDeck] transition watchdog reset"
                             + " progress=" + Number(appDeckRoot.morphProgress).toFixed(3)
                             + " deckState=" + appDeckRoot.deckState
                             + " deckMode=" + appDeckRoot.deckMode)
            }
            // We don't write transitioning here because the SSOT is currently
            // bound from AppDeckWindow (one-way). Signal upward; the binding
            // owner clears its copy, the falling edge below cleans up.
            appDeckRoot.watchdogReset()
            // Defensive freeze release (no-op if not frozen).
            if (appDeckRoot.geometry && typeof appDeckRoot.geometry.unfreezeAnchors === "function") {
                appDeckRoot.geometry.unfreezeAnchors()
            }
        }
    }

    onTransitioningChanged: {
        if (transitioning) {
            transitionWatchdog.restart()
        } else {
            transitionWatchdog.stop()
            // Defer hover recompute one event-loop pass so anchor recompute
            // and input-region updates settle first.
            Qt.callLater(function() {
                appDeckRoot.hoverRecomputeRequested()
            })
        }
        if (debugLogsEnabled) {
            console.log("[AppDeckRoot] transitioning=" + transitioning
                        + " progress=" + Number(morphProgress).toFixed(3)
                        + " state=" + deckState)
        }
    }

    // Exposed so AppDeckMotion (Tahap 7) can retarget the driver without
    // reaching across scopes.
    readonly property var morphAnimation: morphAnim
}
