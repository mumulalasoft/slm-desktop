pragma Singleton

import QtQuick 2.15

// Shell layer z-order constants.
//
// TopBarWindow and DockWindow are Items inside the shell's main window scene.
// Transient overlay Windows (LaunchpadWindow, WorkspaceWindow, TothespotWindow)
// are separate Qt native windows and stack above the shell window automatically.
//
// These z values govern stacking of Items within the main shell window scene only.
//
// Normal render order (low → high):
//   wallpaper → desktop → workspaceSurfaces → dock → topBar → debugOverlay
//
// Overlay Windows appear above all Items regardless of z-value:
//   LaunchpadWindow, WorkspaceWindow, TothespotWindow, system dialogs.
//
QtObject {
    // Persistent item layers (inside main shell window)
    readonly property int wallpaper:          0
    readonly property int desktopIcons:       10
    readonly property int workspaceSurfaces:  20
    readonly property int dock:               200  // DockWindow Item
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
