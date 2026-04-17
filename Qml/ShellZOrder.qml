pragma Singleton

import QtQuick 2.15

// Shell layer z-order constants.
//
// Items inside the shell's ApplicationWindow scene graph use these z-values.
// Windows managed by the compositor (LaunchpadWindow, DockWindow) are ordered
// at the Wayland protocol level — not by these z-values.
//
// Compositor surface order (high → low):
//   LayerTop:  DockWindow (wlr-layer-shell)   ← above everything
//   Normal:    LaunchpadWindow (transient)     ← above app windows
//   Normal:    Application windows
//   Shell ApplicationWindow (contains items below)
//
// Item render order inside ApplicationWindow (low → high):
//   wallpaper → desktop → workspaceSurfaces → topBar → debugOverlay
//
QtObject {
    // Persistent item layers (inside main shell window)
    readonly property int wallpaper:          0
    readonly property int desktopIcons:       10
    readonly property int workspaceSurfaces:  20
    readonly property int dock:               200  // kept for any legacy reference
    readonly property int topBar:             220  // TopBarWindow Item

    // Internal z values within DesktopScene for non-window Items
    readonly property int dockRevealHint:     230  // hint strip above dock
    readonly property int hud:                240  // space-switch HUD
    readonly property int styleGallery:       250
    readonly property int themeTransition:    260
    readonly property int pointerCapture:     900  // top-level mouse capture

    // Dev/debug (never ship in release)
    readonly property int debugOverlay:       950
}
