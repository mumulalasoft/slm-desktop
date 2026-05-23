/*
 * InstallerTheme — Calamares-local design-token singleton for the SLM
 * installer surface. Mirrors a curated subset of
 * `third_party/slm-style/qml/SlmStyle/Theme.qml` (the desktop shell SSOT)
 * because Calamares runs in a separate process that cannot import the
 * resource-embedded SlmStyle module.
 *
 * Rule: when a token value changes upstream in Theme.qml, mirror it here
 * intentionally. This file is a frozen snapshot, not an automatic copy —
 * a desktop-shell upgrade must NOT be able to mutate the live ISO's
 * installer behavior.
 *
 * Light values only for v1.0 (the installer is ephemeral and runs before
 * the compositor is active). v1.1 will add a `darkMode` binding here.
 *
 * Design plan: docs/SLM_INSTALLER_DESIGN.md §4 (Visual Language).
 */
pragma Singleton
import QtQuick

QtObject {
    // ── Palette ─────────────────────────────────────────────────────────────
    readonly property color windowBg:      "#f5f5f7"
    readonly property color surface:       "#ebecef"
    readonly property color borderSubtle:  "#dde2e7"
    readonly property color textPrimary:   "#1d1d1f"
    readonly property color textSecondary: "#5a5a5f"
    readonly property color textTertiary:  "#9aa3ad"
    readonly property color textDisabled:  "#8c95a0"
    readonly property color textOnAccent:  "#ffffff"

    readonly property color accent:        "#0a84ff"
    readonly property color accentLight:   "#4da3ff"
    readonly property color error:         "#d14b4b"
    readonly property color success:       "#28c840"
    readonly property color warning:       "#ff9f0a"

    // Partition diagram segment colors. EFI uses the lighter accent so the
    // root segment (accent proper) reads as the primary install target.
    readonly property color partitionEfi:      accentLight
    readonly property color partitionRoot:     accent
    readonly property color partitionRecovery: "#1d3557"

    // ── Typography ─────────────────────────────────────────────────────────
    // Conservative installer ramp from §4. OOBE has a separate (richer) ramp.
    readonly property int  fontPxDisplay:  28
    readonly property int  fontPxSubtitle: 15
    readonly property int  fontPxBody:     14
    readonly property int  fontPxSm:       12
    readonly property int  fontPxXs:       11
    readonly property string fontFamilyUi: "Open Sans"

    // Weights. Calling code uses `InstallerTheme.weightMedium` instead of
    // the bare Font.X literal so the SLM ui-style lint stays clean (the
    // lint enforces tokenization on direct font-weight literals).
    readonly property int weightRegular:  Font.Normal
    readonly property int weightMedium:   Font.Medium
    readonly property int weightSemiBold: Font.DemiBold
    readonly property int weightBold:     Font.Bold

    // Letter-spacing tokens — match the §4 installer type ramp.
    readonly property real letterSpacingDisplay: -0.6   // 28px headline
    readonly property real letterSpacingTitle3:  -0.2   // 15px section label
    readonly property real letterSpacingNormal:   0.0
    readonly property real letterSpacingWide:     0.1
    readonly property real letterSpacingWider:    0.2

    // ── Shape ──────────────────────────────────────────────────────────────
    readonly property real radiusCard:    12
    readonly property real radiusControl:  8
    readonly property real radiusSm:       4

    readonly property int  borderThin:    1
    readonly property int  borderAccent:  2

    // ── Opacity tokens (lint enforces tokenization on numeric opacity) ────
    readonly property real opacityHoverWash: 0.03
    readonly property real opacityDisabled:  0.55
    readonly property real opacitySeparator: 0.5
    readonly property real opacityShown:     1.0
    readonly property real opacityHidden:    0.0

    // ── Motion ─────────────────────────────────────────────────────────────
    // §4 mandates: the installer has exactly two motion moments. These are
    // the canonical durations for them.
    readonly property int  durationDiagram:  280   // partition diagram entrance
    readonly property int  durationFast:     120   // hover, opacity crossfades
    readonly property int  durationMd:       220   // general short transitions

    readonly property int  easingDecelerate: Easing.OutCubic
    readonly property int  easingStandard:   Easing.InOutCubic
    readonly property int  easingAccelerate: Easing.InCubic

    // Stagger between partition diagram segments. EFI starts at 0,
    // Root at +stagger, Recovery at +2*stagger. Keeps the bar from
    // animating as one homogeneous blob.
    readonly property int  diagramStagger:   40
}
