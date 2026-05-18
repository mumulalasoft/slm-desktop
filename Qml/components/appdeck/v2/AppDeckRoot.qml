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

    // Exposed so AppDeckMotion (Tahap 7) can retarget the driver without
    // reaching across scopes.
    readonly property var morphAnimation: morphAnim
}
