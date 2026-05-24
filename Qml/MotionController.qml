pragma Singleton

import QtQuick 2.15
import SlmStyle

// MotionController — Single source of truth for animation semantics.
//
// Purpose:
//   Centralises motion presets so components never hardcode duration/easing.
//   Provides a runtime quality tier based on Theme.animationMode + FPS health.
//   Supplies a batch() helper to start multiple animations in the same frame.
//
// Usage — in a Behavior:
//   Behavior on opacity {
//       NumberAnimation {
//           duration:    MotionController.preset("enter").duration
//           easing.type: MotionController.preset("enter").easing
//       }
//   }
//
// Usage — in a Transition:
//   Transition {
//       NumberAnimation {
//           properties: "x,y,opacity"
//           duration:    MotionController.preset("page").duration
//           easing.type: MotionController.preset("page").easing
//       }
//   }
//
// Usage — batch start (same event-loop tick):
//   MotionController.batch([enterAnim, fadAnim, slideAnim])
//
// Preset catalogue:
//   micro        icon swap, badge pulse, tiny indicators
//   hover        hover-in/out, button press, chip scale
//   enter        element entering the screen (decelerate)
//   exit         element leaving the screen (accelerate)
//   standard     bi-directional state change, panel resize
//   emphasized   modal reveal, sheet enter, theme switch
//   spring       interactive settle, icon bounce, drop feedback
//   page         panel slide, workspace switch, full-screen
//   workspace    cross-workspace movement
//   instant      no animation (placeholder for future-proof code)
//
// Anti-jitter contract:
//   - Animate transform properties only: x, y, scale, opacity, rotation.
//   - AVOID animating width/height — they trigger layout recalculation.
//   - AVOID changing anchors during animation.
//   - Wrap heavy items in layer.enabled when MotionController.effectsEnabled.
//   - All durations must come from Theme tokens (enforced by lint).

QtObject {
    id: controller

    // ── FPS tracking ───────────────────────────────────────────────────────
    // Written exclusively by MotionMonitor (the visual FPS sampler that lives
    // in the shell root). Components must NOT write these properties.

    property real currentFps: 60.0
    property bool _fpsTooLow: false

    // MotionMonitor calls this on each sample interval (~2 s).
    function reportFps(fps) {
        currentFps = fps
        _fpsTooLow = fps < 50.0
        if (_fpsTooLow) lowFps(fps)
    }

    // Emitted when measured FPS falls below 50. Components may reduce effects.
    signal lowFps(real fps)

    // ── Quality tier ───────────────────────────────────────────────────────
    // "full"     — all effects (blur, shadow, spring) enabled
    // "reduced"  — simplified effects, shorter durations
    // "minimal"  — transforms only, no blur/shadow, instant fallbacks

    readonly property string qualityTier: {
        if (!Theme.animationsEnabled)           return "minimal"
        if (_fpsTooLow)                         return "reduced"
        var m = String(Theme.animationMode || "full")
        if (m === "minimal")                    return "minimal"
        if (m === "reduced")                    return "reduced"
        return "full"
    }

    // True when blur / DropShadow / layer effects are safe to render.
    // Guard heavy layer.effect expressions with this:
    //   layer.enabled: MotionController.effectsEnabled
    readonly property bool effectsEnabled: qualityTier === "full"

    // True when any animated movement is permitted at all.
    readonly property bool animationsEnabled: Theme.animationsEnabled

    // ── Motion presets ─────────────────────────────────────────────────────
    // Returns {duration: int, easing: int} for a named semantic role.
    // Durations are already scaled by Theme._modeScale (via Theme token).
    function preset(name) {
        switch (String(name || "standard")) {
        // ── Micro / hover ──────────────────────────────────────────────────
        case "micro":
            // Icon swap, badge pulse, tiny state indicators.
            return { duration: Theme.durationMicro, easing: Theme.easingLight }
        case "hover":
            // Hover-in/out, button press feedback, chip scale.
            return { duration: Theme.durationSm, easing: Theme.easingLight }
        // ── Element enter / exit ───────────────────────────────────────────
        case "enter":
            // Element decelerates into position — most common surface enter.
            return { duration: Theme.durationMd, easing: Theme.easingDecelerate }
        case "exit":
            // Element accelerates away — quick, unobtrusive.
            return { duration: Theme.durationSm, easing: Theme.easingAccelerate }
        // ── Standard / emphasis ────────────────────────────────────────────
        case "standard":
            // Bi-directional state transitions: panel resize, card flip.
            return { duration: Theme.durationNormal, easing: Theme.easingStandard }
        case "emphasized":
            // High-importance transitions: modal, sheet reveal, theme switch.
            return { duration: Theme.durationLg, easing: Theme.easingDefault }
        // ── Spring / bounce ────────────────────────────────────────────────
        case "spring":
            // Interactive settle: icon bounce, drag release, drop feedback.
            return { duration: Theme.durationMd, easing: Theme.easingSpring }
        // ── Full-panel / workspace ─────────────────────────────────────────
        case "page":
            // Panel slide, app launch sweep, overlay enter/exit.
            return { duration: Theme.durationXl, easing: Theme.easingStandard }
        case "workspace":
            // Cross-workspace swipe, Mission Control enter/exit.
            return { duration: Theme.durationWorkspace, easing: Theme.easingStandard }
        // ── Utility ────────────────────────────────────────────────────────
        case "instant":
            // Immediate update — keeps easing slot for future-proof code.
            return { duration: 1, easing: Theme.easingLight }
        default:
            return { duration: Theme.durationNormal, easing: Theme.easingStandard }
        }
    }

    // ── Batch helper ───────────────────────────────────────────────────────
    // Schedules multiple QML Animation objects to start in the same
    // event-loop tick, so they begin on the same rendered frame.
    //
    // Usage:
    //   MotionController.batch([enterAnim, fadeAnim, slideAnim])
    //
    // Each element must expose start(). Null/undefined entries are skipped.
    function batch(animations) {
        Qt.callLater(function() {
            for (var i = 0; i < animations.length; ++i) {
                var a = animations[i]
                if (a && typeof a.start === "function") a.start()
            }
        })
    }
}
