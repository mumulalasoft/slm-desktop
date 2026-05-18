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
}
