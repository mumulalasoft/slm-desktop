// AppDeck v2 design tokens (docs/APPDECK.md §25, §27).
// Mirrors the inline tokens that currently live in AppDeckWindow.qml so the
// migration can swap call sites incrementally without behavioural drift.
// Instantiated locally inside AppDeckRoot (not a singleton) — sub-folder QML
// types in this project are imported via relative-path aliases rather than
// QML module URIs, which keeps singleton registration simple to avoid.
import QtQuick 2.15

QtObject {
    // Motion tokens (§25)
    readonly property int morphDuration: 380
    readonly property int modeDuration: 240
    readonly property int collapseDuration: 280
    readonly property int contextDuration: 200
    readonly property int autoHideDelay: 2200
    readonly property int autoShowDuration: 220
    readonly property int autoHideDuration: 340

    // Resting micro-motion (§25)
    readonly property real restingAmplitude: 0.012
    readonly property real restingFrequency: 0.48

    // Morph geometry endpoints (§6) — surface shape interpolation
    readonly property real dockRadius: 28.0
    readonly property real gridRadius: 22.0

    // Icon morph scale endpoints (§7)
    readonly property real dockIconScale: 1.0
    readonly property real gridIconScale: 1.0

    // Auto-hide low-presence floor (§15) — never 0.0, must keep reveal zone.
    readonly property real autoHideMinPresence: 0.08

    // Feature flag (§19 Tahap 4 rollback): when false, IconMorphLayer disables
    // and dock/grid render their own icon delegates as in legacy behavior.
    // Default OFF — flip to true by setting env var SLM_APPDECK_V2_ICON_MORPH=1
    // before launching the shell, or hard-code true here after QEMU validation.
    readonly property bool iconMorphEnabled: {
        var env = (typeof Qt !== "undefined" && typeof Qt.application !== "undefined"
                   && Qt.application && Qt.application.arguments) ? null : null
        // Qt6: Qt.platform.os exists but env access goes via Qt.application's env
        // helper or via context property. We rely on a context property the C++
        // host sets when SLM_APPDECK_V2_ICON_MORPH is in the environment; until
        // wired, the safe default is false.
        return (typeof AppDeckV2IconMorphEnabled !== "undefined")
                ? !!AppDeckV2IconMorphEnabled
                : false
    }
}
