pragma Singleton
import QtQuick 2.15
import QtQuick.Window 2.15

QtObject {
    id: theme

    readonly property string modeSystem: "system"
    readonly property string modeLight: "light"
    readonly property string modeDark: "dark"

    property bool followSystem: true
    property bool forcedDarkMode: false
    readonly property string mode: followSystem ? modeSystem : (forcedDarkMode ? modeDark : modeLight)

    // User-configurable accent color — set from UIPreferences.accentColor.
    // Validated on assignment: invalid color strings fall back to the default.
    property string userAccentColor: "#0a84ff"
    readonly property color accent: {
        var c = Qt.color(userAccentColor)
        return c.valid ? c : Qt.color("#0a84ff")
    }
    readonly property int transitionDuration: 260

    // Motion tokens — use these instead of hardcoded duration/easing values.
    // Micro: icon swap, badge pulse, small opacity transitions.
    readonly property int durationMicro: 90
    // Short: hover state, chip scale, button press feedback.
    readonly property int durationSm: 130
    // Medium: panel slide, dropdown open, popover appear.
    readonly property int durationMd: 200
    // Large: overlay enter/exit, workspace transition.
    readonly property int durationLg: 290
    // Extra-large: full-screen transitions, theme switch.
    readonly property int durationXl: 380

    // Easing presets aligned with macOS HIG motion feel.
    // Standard: bi-directional transitions (e.g. card flip, panel resize).
    readonly property int easingStandard: Easing.InOutCubic
    // Decelerate: elements entering the screen (ease-out feel).
    readonly property int easingDecelerate: Easing.OutCubic
    // Accelerate: elements leaving the screen (ease-in feel).
    readonly property int easingAccelerate: Easing.InCubic
    // Spring-like: interactive gesture settle / bounce.
    readonly property int easingSpring: Easing.OutBack
    readonly property string fontFamilyUi: "Open Sans"
    readonly property string fontFamilyDisplay: "Open Sans"
    readonly property string fontFamilyMonospace: "SF Mono"

    // Qt Quick font.pixelSize uses device-independent pixels (DIP).
    // Keep typography deterministic and only allow explicit user scaling.
    property real userFontScale: 1.0
    readonly property real fontScale: Math.max(0.8, Math.min(1.5, userFontScale))

    // Shared typography scale (macOS-like)
    readonly property int fontPxMicro: 8
    readonly property int fontPxTiny: 10
    readonly property int fontPxXs: 11
    readonly property int fontPxSm: 12
    readonly property int fontPxMenu: 13
    readonly property int fontPxSmPlus: 13
    readonly property int fontPxBody: 14
    readonly property int fontPxBodyLg: 15
    readonly property int fontPxSubtitle: 15
    readonly property int fontPxTitle: 16
    readonly property int fontPxTitleLg: 16
    readonly property int fontPxHero: 22
    readonly property int fontPxDisplay: 28
    readonly property int fontPxJumbo: 36
    readonly property real lineHeightTight: 1.1
    readonly property real lineHeightNormal: 1.25
    readonly property real lineHeightRelaxed: 1.4

    // Shared spacing/size tokens (macOS-like)
    readonly property int spacingXxs: 4
    readonly property int spacingXs: 6
    readonly property int spacingSm: 8
    readonly property int spacingMd: 10
    readonly property int spacingLg: 12
    readonly property int spacingXl: 16
    readonly property int spacingXxl: 20
    readonly property int controlHeightCompact: 22
    readonly property int controlHeightRegular: 28
    readonly property int controlHeightLarge: 34
    readonly property int topBarHeight: 34
    readonly property int fileManagerToolbarHeight: 44
    readonly property int popupWidthS: 288
    readonly property int popupWidthM: 320
    readonly property int popupWidthL: 360

    readonly property color systemWindowColor: (
        Qt.application && Qt.application.palette
            ? Qt.application.palette.window
            : "#f0f0f0"
    )
    readonly property real systemWindowLuminance: (
        (0.299 * systemWindowColor.r) +
        (0.587 * systemWindowColor.g) +
        (0.114 * systemWindowColor.b)
    )
    readonly property bool systemPrefersDark: systemWindowLuminance < 0.5

    readonly property bool darkMode: followSystem ? systemPrefersDark : forcedDarkMode

    // Shared shape/elevation tokens (macOS-like)
    readonly property real radiusHairline: 1
    readonly property real radiusTiny: 2
    readonly property real radiusXs: 3
    readonly property real radiusSm: 4
    readonly property real radiusSmPlus: 5
    readonly property real radiusMd: 6
    readonly property real radiusMdPlus: 7
    readonly property real radiusControl: 8
    readonly property real radiusControlLarge: 10
    readonly property real radiusLg: 11
    readonly property real radiusCard: 12
    readonly property real radiusPopup: 12
    readonly property real radiusWindow: 14
    readonly property real radiusWindowAlt: 15
    readonly property real radiusXl: 16
    readonly property real radiusXlPlus: 17
    readonly property real radiusXxl: 18
    readonly property real radiusXxxl: 20
    readonly property real radiusHuge: 22
    readonly property real radiusPill: 23
    readonly property real radiusMax: 24
    readonly property real radiusKnob: 3.5

    readonly property int borderWidthNone: 0
    readonly property int borderWidthThin: 1
    readonly property int borderWidthThick: 2

    readonly property real popupSurfaceOpacity: 0.985
    readonly property real popupSurfaceOpacityStrong: 0.99
    readonly property real cardSurfaceOpacity: 0.98
    readonly property real opacityFaint: 0.18
    readonly property real opacitySubtle: 0.2
    readonly property real opacitySeparator: 0.75
    readonly property real opacityMuted: 0.8
    readonly property real opacityDockPreview: 0.84
    readonly property real opacityIconMuted: 0.9
    readonly property real opacityHint: 0.92
    readonly property real opacityGhost: 0.95
    readonly property real opacityElevated: 0.96
    readonly property real opacitySurfaceStrong: 0.98
    readonly property real windowShadowMargin: 5
    readonly property real windowShadowOuterOpacity: 0.14
    readonly property real windowShadowInnerOpacity: 0.08
    readonly property real windowInnerStrokeOpacity: 0.08

    // Shadow / elevation presets.
    // Each token is a plain JS object with the properties consumed by DropShadow
    // or a manual shadow Rectangle: {radius, horizontalOffset, verticalOffset,
    //  opacity, spread}. Light/dark variants use different opacity values.
    //
    // Usage example:
    //   layer.effect: DropShadow {
    //       radius:            Theme.shadowMd.radius
    //       verticalOffset:    Theme.shadowMd.verticalOffset
    //       color:             "#000000"
    //       opacity:           Theme.shadowMd.opacity * (Theme.darkMode ? 1.6 : 1.0)
    //   }
    readonly property var shadowSm: ({
        radius: 6, horizontalOffset: 0, verticalOffset: 2, opacity: 0.10, spread: 0
    })
    readonly property var shadowMd: ({
        radius: 14, horizontalOffset: 0, verticalOffset: 4, opacity: 0.16, spread: 0
    })
    readonly property var shadowLg: ({
        radius: 28, horizontalOffset: 0, verticalOffset: 8, opacity: 0.22, spread: 1
    })
    // Extra-large: modals, full-panel overlays.
    readonly property var shadowXl: ({
        radius: 48, horizontalOffset: 0, verticalOffset: 14, opacity: 0.28, spread: 2
    })

    readonly property var light: ({
        windowBg: "#f5f5f7",
        surface: "#ebecef",
        panelBg: "#d9f3f4f6",
        panelBorder: "#b7bec6",
        borderSubtle: "#dde2e7",
        textPrimary: "#1d1d1f",
        textSecondary: "#5a5a5f",
        textTertiary: "#9aa3ad",
        textDisabled: "#8c95a0",
        textOnGlass: "#1d1d1f",
        focusRing: "#4da3ff",
        focusRingStrong: "#0a84ff",
        controlDisabledBg: "#e3e8ed",
        controlDisabledBorder: "#b8c2cb",
        error: "#d14b4b",
        success: "#28c840",
        warning: "#ff9f0a",
        warningBg: "#fff5e0",
        windowControlClose: "#ff6059",
        windowControlMinimize: "#ffbd2f",
        windowControlMaximize: "#28c941",
        selectedItem: "#0a84ff",
        selectedItemText: "#ffffff",
        selectionRectFill: "#335aa9ff",
        selectionRectBorder: "#99cde5ff",
        dragGhostFill: "#2f0a84ff",
        dragGhostBorder: "#c3dfff",
        screenshotScrim: "#55000000",
        screenshotSelectionFill: "#22ffffff",
        screenshotSelectionBorder: "#e6ffffff",
        transitionTint: "#80060b14",
        dockRevealHint: "#66060b14",
        spaceHudBg: "#dceef5ff",
        spaceHudBorder: "#7a93a9c2",
        storageUsageInfo: "#3f7edb",
        storageUsageWarn: "#e7a83a",
        storageUsageCritical: "#e55757",
        storageUsageUnknown: "#8aa0b8",
        storageTrackMounted: "#c8d2dc",
        storageTrackUnmounted: "#d6dde4",
        dockOutline: "#7fb6d5ee",
        dockGlassTop: "#77b8d8f3",
        dockGlassBottom: "#65a7cde3",
        dockSpecLine: "#d9ffffff",
        dockDropPulseBorder: "#d9202a36",
        dockGapPreviewBg: "#73f4fbff",
        dockRunningDotActive: "#123f95",
        dockRunningDotInactive: "#0b2f84",
        dockTooltipBg: "#e8eef5fc",
        batchFailedItemBg: "#eef5fc",
        batchFailedItemBorder: "#9eb6cd",
        batchProgressStroke: "#6688a1b8",
        styleGalleryBg: "#d9f5f9ff",
        styleGalleryHeaderBg: "#80d9e8fa",
        styleGallerySectionBg: "#80ecf4ff",
        overviewBackdrop: "#66d8e5f2",
        overviewBarBg: "#dbeef4fb",
        overviewBarBorder: "#6f8ea7bf",
        overviewSpaceChipBg: "#b8dbeaef",
        overviewSpaceChipBorder: "#7391abc4",
        overviewWindowCountBadge: "#e2edf8",
        overviewAddChipBg: "#b8dbeaf2",
        overviewWindowBorderUnfocused: "#7a8da3bc",
        overviewWindowPlaceholder: "#30ffffff",
        overviewCloseBtnBg: "#d8f0f3f7",
        overviewCloseBtnBorder: "#7f90a4ba",
        overviewCaptionBg: "#d6ffffff",
        overviewCaptionBorder: "#6490a9bf",
        // Rebrand aliases (overview -> workspace). Keep both key families
        // for backward compatibility while components migrate gradually.
        workspaceBackdrop: "#66d8e5f2",
        workspaceBarBg: "#dbeef4fb",
        workspaceBarBorder: "#6f8ea7bf",
        workspaceChipBg: "#b8dbeaef",
        workspaceChipBorder: "#7391abc4",
        workspaceWindowCountBadge: "#e2edf8",
        workspaceAddChipBg: "#b8dbeaf2",
        workspaceWindowBorderUnfocused: "#7a8da3bc",
        workspaceWindowPlaceholder: "#30ffffff",
        workspaceCloseBtnBg: "#d8f0f3f7",
        workspaceCloseBtnBorder: "#7f90a4ba",
        workspaceCaptionBg: "#d6ffffff",
        workspaceCaptionBorder: "#6490a9bf",
        compositorOverlayScrim: "#30000000",
        compositorPanelBg: "#cce9f0fa",
        compositorPanelBorder: "#7aa4b8d1",
        compositorPanelSheen: "#66ffffff",
        compositorCardBgPrimary: "#ddeeffff",
        compositorCardBgSecondary: "#88d5e2f2",
        compositorCardBgTertiary: "#6bcfdef0",
        compositorCardBorderActive: "#ff8ba4c3",
        compositorCardBorderInactive: "#88a5b6cc",
        compositorLabelPrimary: "#1f2c3a",
        compositorLabelSecondaryActive: "#1e2a38",
        compositorLabelSecondaryInactive: "#42566f",
        compositorFooterLabel: "#4a6078",
        launchpadOrbTop: "#66d8ebff",
        launchpadOrbBottom: "#4cb9e2f0",
        launchpadSearchBg: "#8adfe8f4",
        launchpadSearchBorder: "#9ab8c3d6",
        launchpadSegmentBg: "#99e3edf8",
        launchpadSegmentBorder: "#8fb5c8dd",
        launchpadSegmentActive: "#b7d7f4",
        launchpadIconRing: "#668ba4c0",
        tothespotPanelBg: "#f6fbffff",
        tothespotPanelBorder: "#65a5bdd0",
        tothespotQueryBg: "#edf5fb",
        tothespotQueryBorder: "#79a8c4dc",
        tothespotResultsBg: "#f2f8fe",
        tothespotResultsBorder: "#5aa9bfd0",
        tothespotSectionBg: "#ecf5fd",
        tothespotRowActive: "#d8ebff",
        tothespotRowHover: "#e8f4ff",
        tothespotBadgeActive: "#c6dcf8",
        tothespotBadgeBg: "#e7f1fb",
        tothespotBadgeBorder: "#bfd3e6",
        tothespotPreviewBg: "#edf5fc",
        tothespotPreviewBorder: "#79a8c4dc",
        shellIconPlateBg: "#8af2f7ff",
        hoverItem: "#e9edf2",
        // Semantic control state tokens.
        controlBg: "#f0f4f8",
        controlBgHover: "#e5ecf3",
        controlBgPressed: "#d4dde6",
        errorSoft: "#fce8e8",
        accent: "#0a84ff",
        accentText: "#ffffff",
        accentSoft: "#1f0a84ff",
        dockBg: "#d4e8f2f6",
        dockBorder: "#a5b5bf",
        menuBg: "#f4f6f8",
        menuBorder: "#bcc6ce",
        menuHover: "#0a84ff",
        menuText: "#1d1d1f",
        menuSeparator: "#b6bec5",
        elevationShadowLow: "#16000000",
        elevationShadowMedium: "#26000000",
        elevationShadowHigh: "#33000000",
        windowCard: "#f6f7f9",
        windowCardBorder: "#c5ccd1",
        fileManagerWindowBg: "#eef1f4",
        fileManagerWindowBorder: "#9aa5af",
        fileManagerBodyBg: "#e9edf1",
        fileManagerTabsBg: "#dfe4e9",
        fileManagerTabsBorder: "#adb8c1",
        fileManagerTabActive: "#ccd5dd",
        fileManagerTabCloseHover: "#bac6d0",
        fileManagerControlBg: "#f3f6f9",
        fileManagerControlBorder: "#b3bec7",
        fileManagerControlActive: "#bfd2e1",
        fileManagerSearchBg: "#f5f7f9",
        fileManagerContentBg: "#f2f5f8",
        fileManagerContentBorder: "#b7c1c8",
        fileManagerRowAlt: "#e3eaf2",
        fileManagerRubberBand: "#220a64ff",
        fileManagerGhostBg: "#e2e8ee",
        fileManagerGhostBorder: "#9fadb8",
        fileManagerSidebarBg: "#edf1f4",
        fileManagerSidebarBorder: "#bec7ce",
        fileManagerUnmountHover: "#dce3e9",
        fileManagerStorageTrack: "#b8c1c8",
        fileManagerStorageFill: "#5a6a77",
        overlay: "#40121418",
        wallpaperTint: "#14212830"
    })

    readonly property var dark: ({
        windowBg: "#1c1c1e",
        surface: "#2a2a2d",
        panelBg: "#b42a2a2e",
        panelBorder: "#5b5b61",
        borderSubtle: "#3a3a3f",
        textPrimary: "#f5f5f7",
        textSecondary: "#aeaeb2",
        textTertiary: "#707a87",
        textDisabled: "#7f8a99",
        textOnGlass: "#f5f5f7",
        focusRing: "#79b8ff",
        focusRingStrong: "#4da3ff",
        controlDisabledBg: "#2a313a",
        controlDisabledBorder: "#5d6978",
        error: "#ff6b6b",
        success: "#30d158",
        warning: "#ffb340",
        warningBg: "#2d2010",
        windowControlClose: "#ff6059",
        windowControlMinimize: "#ffbd2f",
        windowControlMaximize: "#28c941",
        selectedItem: "#0a84ff",
        selectedItemText: "#ffffff",
        selectionRectFill: "#335aa9ff",
        selectionRectBorder: "#99cde5ff",
        dragGhostFill: "#2f0a84ff",
        dragGhostBorder: "#c3dfff",
        screenshotScrim: "#66000000",
        screenshotSelectionFill: "#22ffffff",
        screenshotSelectionBorder: "#e6ffffff",
        transitionTint: "#99f0f5ff",
        dockRevealHint: "#66f3f7ff",
        spaceHudBg: "#d4202a38",
        spaceHudBorder: "#45ffffff",
        storageUsageInfo: "#74a9ff",
        storageUsageWarn: "#ffb340",
        storageUsageCritical: "#ff6b6b",
        storageUsageUnknown: "#6c8198",
        storageTrackMounted: "#3b4958",
        storageTrackUnmounted: "#2f3b47",
        dockOutline: "#70d2e8ff",
        dockGlassTop: "#6d87a6c4",
        dockGlassBottom: "#5d6789ba",
        dockSpecLine: "#a6f1ffff",
        dockDropPulseBorder: "#e6ffffff",
        dockGapPreviewBg: "#2b3e5566",
        dockRunningDotActive: "#74a9ff",
        dockRunningDotInactive: "#3f7edb",
        dockTooltipBg: "#e0283340",
        batchFailedItemBg: "#1f2a36",
        batchFailedItemBorder: "#33506a",
        batchProgressStroke: "#45ffffff",
        styleGalleryBg: "#d9222d39",
        styleGalleryHeaderBg: "#80324358",
        styleGallerySectionBg: "#5f2c394a",
        overviewBackdrop: "#6c0a1018",
        overviewBarBg: "#c3192230",
        overviewBarBorder: "#4affffff",
        overviewSpaceChipBg: "#3a263342",
        overviewSpaceChipBorder: "#45ffffff",
        overviewWindowCountBadge: "#4a5f7890",
        overviewAddChipBg: "#4030404d",
        overviewWindowBorderUnfocused: "#88ffffff",
        overviewWindowPlaceholder: "#3a111922",
        overviewCloseBtnBg: "#dd3f4754",
        overviewCloseBtnBorder: "#84ffffff",
        overviewCaptionBg: "#d8111823",
        overviewCaptionBorder: "#35ffffff",
        // Rebrand aliases (overview -> workspace). Keep both key families
        // for backward compatibility while components migrate gradually.
        workspaceBackdrop: "#6c0a1018",
        workspaceBarBg: "#c3192230",
        workspaceBarBorder: "#4affffff",
        workspaceChipBg: "#3a263342",
        workspaceChipBorder: "#45ffffff",
        workspaceWindowCountBadge: "#4a5f7890",
        workspaceAddChipBg: "#4030404d",
        workspaceWindowBorderUnfocused: "#88ffffff",
        workspaceWindowPlaceholder: "#3a111922",
        workspaceCloseBtnBg: "#dd3f4754",
        workspaceCloseBtnBorder: "#84ffffff",
        workspaceCaptionBg: "#d8111823",
        workspaceCaptionBorder: "#35ffffff",
        compositorOverlayScrim: "#66000000",
        compositorPanelBg: "#cc1f2733",
        compositorPanelBorder: "#66ffffff",
        compositorPanelSheen: "#40ffffff",
        compositorCardBgPrimary: "#dd32455d",
        compositorCardBgSecondary: "#701f2a37",
        compositorCardBgTertiary: "#4d1c2430",
        compositorCardBorderActive: "#d4ffffff",
        compositorCardBorderInactive: "#52ffffff",
        compositorLabelPrimary: "#f3f8ff",
        compositorLabelSecondaryActive: "#ffffff",
        compositorLabelSecondaryInactive: "#c8d6e7",
        compositorFooterLabel: "#dbe7f5",
        launchpadOrbTop: "#665f8bc8",
        launchpadOrbBottom: "#4c78a6d6",
        launchpadSearchBg: "#8a2f3946",
        launchpadSearchBorder: "#9a6f7f93",
        launchpadSegmentBg: "#60354358",
        launchpadSegmentBorder: "#8e5f7390",
        launchpadSegmentActive: "#6a6fb0ef",
        launchpadIconRing: "#66dce6f2",
        tothespotPanelBg: "#ee182230",
        tothespotPanelBorder: "#22ffffff",
        tothespotQueryBg: "#2e3b4b",
        tothespotQueryBorder: "#58ffffff",
        tothespotResultsBg: "#152030",
        tothespotResultsBorder: "#24ffffff",
        tothespotSectionBg: "#1b2b3b",
        tothespotRowActive: "#355b8cd0",
        tothespotRowHover: "#243244",
        tothespotBadgeActive: "#2f4663",
        tothespotBadgeBg: "#223142",
        tothespotBadgeBorder: "#4a6a8b",
        tothespotPreviewBg: "#1d2c3d",
        tothespotPreviewBorder: "#22ffffff",
        shellIconPlateBg: "#8a1f2a38",
        hoverItem: "#3a3a3f",
        // Semantic control state tokens.
        controlBg: "#38383f",
        controlBgHover: "#44444b",
        controlBgPressed: "#55555d",
        errorSoft: "#3d1a1a",
        accent: "#0a84ff",
        accentText: "#ffffff",
        accentSoft: "#350a84ff",
        dockBg: "#b42b2b31",
        dockBorder: "#68686e",
        menuBg: "#2f3036",
        menuBorder: "#5f6169",
        menuHover: "#2f8fff",
        menuText: "#f5f5f7",
        menuSeparator: "#5a5a62",
        elevationShadowLow: "#24000000",
        elevationShadowMedium: "#3a000000",
        elevationShadowHigh: "#4d000000",
        windowCard: "#2d2f34",
        windowCardBorder: "#5f5f66",
        fileManagerWindowBg: "#2a2a2e",
        fileManagerWindowBorder: "#66666e",
        fileManagerBodyBg: "#232327",
        fileManagerTabsBg: "#2d2d32",
        fileManagerTabsBorder: "#55555d",
        fileManagerTabActive: "#3a3a42",
        fileManagerTabCloseHover: "#484850",
        fileManagerControlBg: "#34343a",
        fileManagerControlBorder: "#5c5c64",
        fileManagerControlActive: "#4a5e72",
        fileManagerSearchBg: "#313138",
        fileManagerContentBg: "#26262b",
        fileManagerContentBorder: "#54545c",
        fileManagerRowAlt: "#323741",
        fileManagerRubberBand: "#2a4a8cff",
        fileManagerGhostBg: "#36363d",
        fileManagerGhostBorder: "#66666f",
        fileManagerSidebarBg: "#2b2b31",
        fileManagerSidebarBorder: "#4f4f57",
        fileManagerUnmountHover: "#45454d",
        fileManagerStorageTrack: "#5a5a63",
        fileManagerStorageFill: "#d1d1d6",
        overlay: "#8f070d14",
        wallpaperTint: "#28111418"
    })

    function color(tokenName) {
        // All accent-derived tokens are computed dynamically from userAccentColor
        // so that a single settings change propagates across the entire UI.
        var c = accent   // already a QML color (readonly property color)
        switch (tokenName) {
        // ── Core accent ───────────────────────────────────────────────────────
        case "accent":
        case "selectedItem":
        case "focusRingStrong":
        case "menuHover":
            return c
        case "accentText":
        case "selectedItemText":
            return "#ffffff"
        case "accentSoft":
            return Qt.rgba(c.r, c.g, c.b, darkMode ? 0.21 : 0.12)
        // ── Focus / keyboard navigation ───────────────────────────────────────
        case "focusRing":
            return Qt.lighter(c, darkMode ? 1.55 : 1.35)
        case "panelBorderStrong":
            return darkMode ? Qt.rgba(1, 1, 1, 0.22) : Qt.rgba(0, 0, 0, 0.20)
        // ── Selection rectangles (rubber-band, drag ghost) ────────────────────
        case "selectionRectFill":
            return Qt.rgba(c.r, c.g, c.b, 0.20)
        case "selectionRectBorder":
            return Qt.rgba(c.r, c.g, c.b, 0.60)
        case "dragGhostFill":
            return Qt.rgba(c.r, c.g, c.b, darkMode ? 0.22 : 0.18)
        case "dragGhostBorder":
            return Qt.lighter(c, 1.5)
        // ── File manager ──────────────────────────────────────────────────────
        case "fileManagerRubberBand":
            return Qt.rgba(c.r, c.g, c.b, 0.13)
        case "fileManagerControlActive":
            return Qt.rgba(c.r, c.g, c.b, darkMode ? 0.29 : 0.74)
        // ── Dock ──────────────────────────────────────────────────────────────
        case "dockRunningDotActive":
            return darkMode ? Qt.lighter(c, 1.25) : Qt.darker(c, 2.5)
        case "dockRunningDotInactive":
            return darkMode ? c : Qt.darker(c, 3.5)
        case "dockGapPreviewBg":
            return Qt.rgba(c.r, c.g, c.b, darkMode ? 0.17 : 0.45)
        // ── Launchpad ─────────────────────────────────────────────────────────
        case "launchpadSegmentActive":
            return darkMode ? Qt.rgba(c.r, c.g, c.b, 0.42) : Qt.rgba(c.r, c.g, c.b, 0.72)
        // ── Tothespot (search) ────────────────────────────────────────────────
        case "tothespotRowActive":
            return darkMode ? Qt.rgba(c.r, c.g, c.b, 0.22) : Qt.rgba(c.r, c.g, c.b, 0.53)
        case "tothespotRowHover":
            return darkMode ? Qt.rgba(c.r, c.g, c.b, 0.14) : Qt.rgba(c.r, c.g, c.b, 0.28)
        case "tothespotBadgeActive":
            return darkMode ? Qt.rgba(c.r, c.g, c.b, 0.18) : Qt.rgba(c.r, c.g, c.b, 0.22)
        // ── Workspace / overview chips ────────────────────────────────────────
        case "workspaceChipBg":
        case "overviewSpaceChipBg":
            return Qt.rgba(c.r, c.g, c.b, darkMode ? 0.23 : 0.72)
        case "workspaceAddChipBg":
        case "overviewAddChipBg":
            return Qt.rgba(c.r, c.g, c.b, darkMode ? 0.25 : 0.72)
        case "spaceHudBg":
            return Qt.rgba(c.r, c.g, c.b, darkMode ? 0.83 : 0.87)
        }
        var palette = darkMode ? dark : light
        if (palette[tokenName] !== undefined) return palette[tokenName]
        console.warn("Theme.color: unknown token \"" + tokenName + "\"")
        return "#ff00ff"
    }

    function fontSize(tokenName) {
        function scaled(px) {
            return Math.max(8, Math.round(Number(px || fontPxBody) * fontScale))
        }
        switch (String(tokenName || "body")) {
        case "micro": return scaled(fontPxMicro)
        case "tiny": return scaled(fontPxTiny)
        case "xs": return scaled(fontPxXs)
        case "small": return scaled(fontPxSm)
        case "menu": return scaled(fontPxMenu)
        case "smallPlus": return scaled(fontPxSmPlus)
        case "body": return scaled(fontPxBody)
        case "bodyLarge": return scaled(fontPxBodyLg)
        case "subtitle": return scaled(fontPxSubtitle)
        case "title": return scaled(fontPxTitle)
        case "titleLarge": return scaled(fontPxTitleLg)
        case "hero": return scaled(fontPxHero)
        case "display": return scaled(fontPxDisplay)
        case "jumbo": return scaled(fontPxJumbo)
        default:
            return scaled(fontPxBody)
        }
    }

    function fontWeight(tokenName) {
        switch (String(tokenName || "normal")) {
        case "medium": return Font.Medium
        case "semibold": return Font.DemiBold
        case "bold": return Font.Bold
        default:
            return Font.Normal
        }
    }

    function lineHeight(tokenName) {
        switch (String(tokenName || "normal")) {
        case "tight": return lineHeightTight
        case "relaxed": return lineHeightRelaxed
        default:
            return lineHeightNormal
        }
    }

    function metric(tokenName) {
        switch (String(tokenName || "")) {
        case "spacingXxs": return spacingXxs
        case "spacingXs": return spacingXs
        case "spacingSm": return spacingSm
        case "spacingMd": return spacingMd
        case "spacingLg": return spacingLg
        case "spacingXl": return spacingXl
        case "spacingXxl": return spacingXxl
        case "controlHeightCompact": return controlHeightCompact
        case "controlHeightRegular": return controlHeightRegular
        case "controlHeightLarge": return controlHeightLarge
        case "topBarHeight": return topBarHeight
        case "fileManagerToolbarHeight": return fileManagerToolbarHeight
        case "popupWidthS": return popupWidthS
        case "popupWidthM": return popupWidthM
        case "popupWidthL": return popupWidthL
        // Motion duration tokens — use via Theme.metric("durationMd") in QML.
        case "durationMicro": return durationMicro
        case "durationSm": return durationSm
        case "durationMd": return durationMd
        case "durationLg": return durationLg
        case "durationXl": return durationXl
        default:
            return spacingSm
        }
    }

    // Apply a mode string: "auto" | "light" | "dark"
    function applyModeString(modeStr) {
        if (modeStr === "dark") useDarkMode()
        else if (modeStr === "light") useLightMode()
        else useSystemMode()
    }

    function toggleMode() {
        if (mode === modeSystem) {
            followSystem = false
            forcedDarkMode = !systemPrefersDark
            return
        }
        followSystem = false
        forcedDarkMode = !forcedDarkMode
    }

    function useSystemMode() {
        followSystem = true
    }

    function useLightMode() {
        followSystem = false
        forcedDarkMode = false
    }

    function useDarkMode() {
        followSystem = false
        forcedDarkMode = true
    }
}
