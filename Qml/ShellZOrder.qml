pragma Singleton

import QtQuick 2.15

// Shell layer z-order constants.
//
// All persistent and overlay layers are Items inside the shell's main window scene.
// Z-values govern their stacking order within that shared scene graph.
//
// Normal render order (low → high):
//   wallpaper → desktop → workspaceSurfaces → dock → topBar → debugOverlay
//
// Persistent layers (TopBarWindow, ShellDockHost) are Items inside the shell's
// ApplicationWindow.  Overlay layers (LaunchpadWindow, etc.) are transient
// Windows — KWin stacks them above the shell window automatically.
// LaunchpadWindow leaves the dock zone uncovered — ShellDockHost handles it.
//
QtObject {
    // Persistent item layers (inside main shell window)
    readonly property int wallpaper:          0
    readonly property int desktopIcons:       10
    readonly property int workspaceSurfaces:  20
    readonly property int dock:               200  // ShellDockHost Item (DockSystem)
    readonly property int topBar:             220  // TopBarWindow Item — above dock

    // Internal z values within DesktopScene for non-window Items
    readonly property int dockRevealHint:     230  // hint strip above dock
    readonly property int hud:                240  // space-switch HUD
    readonly property int styleGallery:       250
    readonly property int themeTransition:    260
    readonly property int pointerCapture:     900  // top-level mouse capture

    // Dev/debug (never ship in release)
    readonly property int debugOverlay:       950
}
