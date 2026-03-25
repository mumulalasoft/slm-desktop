# SLM Desktop UI Style Guide

This guide defines the visual rules for SLM Desktop QML UI so future changes stay consistent with the macOS-inspired direction.

## 1. Source of Truth

- Use `Qml/Theme.qml` as the only source for visual tokens.
- Do not hardcode radius, border width, or core surface opacity in component files.
- Prefer `Theme.color("...")` tokens over literal hex colors for app UI surfaces and text.

## 2. Shape Tokens

Use these tokens from `Theme.qml`:

- `Theme.radiusHairline`
- `Theme.radiusTiny`
- `Theme.radiusXs`
- `Theme.radiusSm`
- `Theme.radiusSmPlus`
- `Theme.radiusMd`
- `Theme.radiusMdPlus`
- `Theme.radiusControl`
- `Theme.radiusControlLarge`
- `Theme.radiusLg`
- `Theme.radiusCard`
- `Theme.radiusWindow`
- `Theme.radiusWindowAlt`
- `Theme.radiusXl`
- `Theme.radiusXlPlus`
- `Theme.radiusXxl`
- `Theme.radiusXxxl`
- `Theme.radiusHuge`
- `Theme.radiusPill`
- `Theme.radiusMax`
- `Theme.radiusKnob`

Practical defaults:

- Small controls, icon buttons: `radiusControl`
- Input groups / segmented controls: `radiusControlLarge`
- Cards / dialogs: `radiusCard`
- Menus / popups: `radiusPopup` (where defined by component contract)
- Main detached windows: `radiusWindow`

## 3. Border Tokens

Use only:

- `Theme.borderWidthNone`
- `Theme.borderWidthThin`
- `Theme.borderWidthThick`

Default:

- Most borders: `Theme.borderWidthThin`
- Emphasis ring/stroke only when needed: `Theme.borderWidthThick`

## 4. Surface Opacity Tokens

Use:

- `Theme.popupSurfaceOpacity`
- `Theme.popupSurfaceOpacityStrong`
- `Theme.cardSurfaceOpacity`
- `Theme.opacityFaint`
- `Theme.opacitySubtle`
- `Theme.opacitySeparator`
- `Theme.opacityMuted`
- `Theme.opacityDockPreview`
- `Theme.opacityIconMuted`
- `Theme.opacityHint`
- `Theme.opacityGhost`
- `Theme.opacityElevated`
- `Theme.opacitySurfaceStrong`

Default:

- Popups/menus: `popupSurfaceOpacity`
- Stronger popup emphasis (rare): `popupSurfaceOpacityStrong`
- Card overlays: `cardSurfaceOpacity`
- Separators: `opacitySeparator`
- Icon de-emphasis: `opacityIconMuted`
- Drag ghost/previews: `opacityGhost` / `opacityElevated`

## 5. Color Rules

- Use `Theme.color("textPrimary")` for primary text.
- Use `Theme.color("textSecondary")` for metadata/secondary labels.
- Use `Theme.color("selectedItem")` + `Theme.color("selectedItemText")` for selected states.
- Use `Theme.color("accent")` + `Theme.color("accentText")` for accent controls.
- Use `Theme.color("menuBg")`/`menuBorder` for popup surfaces.
- Use `Theme.color("windowCard")`/`windowCardBorder` for card-like panels.

### 5.1 Area Token Families

- `Dock`: `dockOutline`, `dockGlassTop`, `dockGlassBottom`, `dockSpecLine`, `dockDropPulseBorder`, `dockGapPreviewBg`, `dockRunningDotActive`, `dockRunningDotInactive`, `dockTooltipBg`
- `Overview`: `overviewBackdrop`, `overviewBarBg`, `overviewBarBorder`, `overviewSpaceChipBg`, `overviewSpaceChipBorder`, `overviewAddChipBg`, `overviewWindowBorderUnfocused`, `overviewWindowPlaceholder`, `overviewCloseBtnBg`, `overviewCloseBtnBorder`, `overviewCaptionBg`, `overviewCaptionBorder`
- `Compositor Switcher`: `compositorOverlayScrim`, `compositorPanelBg`, `compositorPanelBorder`, `compositorPanelSheen`, `compositorCardBgPrimary`, `compositorCardBgSecondary`, `compositorCardBgTertiary`, `compositorCardBorderActive`, `compositorCardBorderInactive`, `compositorLabelPrimary`, `compositorLabelSecondaryActive`, `compositorLabelSecondaryInactive`, `compositorFooterLabel`
- `Launchpad`: `launchpadOrbTop`, `launchpadOrbBottom`, `launchpadSearchBg`, `launchpadSearchBorder`, `launchpadSegmentBg`, `launchpadSegmentBorder`, `launchpadSegmentActive`, `launchpadIconRing`
- `Tothespot`: `tothespotPanelBg`, `tothespotPanelBorder`, `tothespotQueryBg`, `tothespotQueryBorder`, `tothespotResultsBg`, `tothespotResultsBorder`, `tothespotSectionBg`, `tothespotRowActive`, `tothespotRowHover`, `tothespotBadgeActive`, `tothespotBadgeBg`, `tothespotBadgeBorder`, `tothespotPreviewBg`, `tothespotPreviewBorder`
- `File/Storage`: `storageUsageInfo`, `storageUsageWarn`, `storageUsageCritical`, `storageUsageUnknown`, `storageTrackMounted`, `storageTrackUnmounted`
- `Generic`: `error`, `warning`, `success`, `transitionTint`, `dockRevealHint`, `spaceHudBg`, `spaceHudBorder`, `screenshotScrim`, `screenshotSelectionFill`, `screenshotSelectionBorder`, `shellIconPlateBg`

### 5.2 Hex Literal Policy

- Do not use direct hex colors (for example `#rrggbb`/`#aarrggbb`) in `Qml/components/**`, `Qml/DesktopScene.qml`, or `Main.qml`.
- Define or update tokens in `Qml/Theme.qml`, then consume via `Theme.color("...")`.
- Exception: Only `Qml/Theme.qml` stores palette hex values.

## 6. Text Selection Rules

For editable text controls (`TextField`, `TextArea`):

- `selectionColor: Theme.color("selectedItem")`
- `selectedTextColor: Theme.color("selectedItemText")`

This avoids fallback colors and keeps consistency in light/dark modes.

## 7. Typography Rules

Use `Theme.fontSize("token")` for all text sizing in QML.

Token set:

- `micro`, `tiny`, `xs`, `small`, `smallPlus`
- `body`, `bodyLarge`
- `subtitle`, `title`, `titleLarge`
- `hero`, `display`, `jumbo`

Do not hardcode numeric `font.pixelSize` values in components/scenes.

Use `Theme.fontWeight("token")` for font weight:

- `normal`, `medium`, `semibold`, `bold`

Do not use direct `font.bold` assignments.
Do not use direct `font.weight: Font.*` or numeric weight literals.

When custom line spacing is needed, use `Theme.lineHeight("token")`:

- `tight`, `normal`, `relaxed`

## 8. Interaction Rules

- Keep hover states subtle using `accentSoft` or component-specific hover tokens.
- Keep focus/selection states clear with `selectedItem` rather than custom ad-hoc colors.
- Avoid adding new visual constants inside component files unless first promoted to `Theme.qml`.

## 9. Migration Rule for New Code

Before merging UI changes:

1. No new hardcoded `radius:` numeric values (except intentional `0`).
2. No new hardcoded `border.width:` numeric values.
3. No new hardcoded popup/card opacity values where Theme token exists.
4. No new hardcoded hex colors outside `Qml/Theme.qml`.
5. No new hardcoded numeric `font.pixelSize` values.
6. No direct `font.bold` usage and no direct literal `font.weight`.
7. Build passes:
   - `cmake --build build -j4 --target appSlm_Desktop`
8. Style lint passes:
   - `scripts/lint-ui-style.sh`

## 10. Exceptions

- Icons, media previews, and external brand assets may use literal values when necessary.
- Technical debug views (temporary) may use literals, but must be cleaned before merge.
