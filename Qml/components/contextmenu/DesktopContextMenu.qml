import QtQuick
import QtQuick.Controls
import Slm_Desktop

// DesktopContextMenu — right-click handler for the desktop surface.
//
// Embed this inside DesktopScene.qml (or any desktop area Item) and it will
// catch right-clicks, build the context menu, and dispatch actions.
//
// Example:
//   DesktopContextMenu {
//       anchors.fill: parent
//       desktopPath: "/home/user/Desktop"
//       onOpenTerminal: { /* launch terminal */ }
//       onOpenSettings:  { /* open settings app */ }
//       onChangeWallpaper: { /* open appearance panel */ }
//   }

Item {
    id: root

    // Path of the desktop folder (used for paste / new file operations).
    property string desktopPath: ""

    signal newFolder()
    signal newTextFile()
    signal paste()
    signal changeWallpaper()
    signal openAppearance()
    signal openSettings()
    signal openTerminal()

    // ── right-click detection ────────────────────────────────────────────────
    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: function(eventPoint) {
            var hasClip = (typeof ClipboardServiceClient !== "undefined"
                           && ClipboardServiceClient
                           && ClipboardServiceClient.hasContent === true)
            desktopMenu.surfaceContext = {
                "type": "desktop",
                "filePath": root.desktopPath,
                "canPaste": hasClip
            }
            desktopMenu.popupAt(eventPoint.position.x, eventPoint.position.y)
        }
    }

    // ── menu ─────────────────────────────────────────────────────────────────
    ContextMenu {
        id: desktopMenu

        onAction: function(id, ctx) {
            switch (id) {
            case "new_folder":       root.newFolder();      break
            case "new_text_file":    root.newTextFile();    break
            case "paste":            root.paste();          break
            case "change_wallpaper": root.changeWallpaper(); break
            case "appearance":       root.openAppearance(); break
            case "settings":         root.openSettings();   break
            case "open_terminal":    root.openTerminal();   break
            default: break
            }
        }

        onExtensionAction: function(actionId, ctx) {
            if (typeof ActionRegistry !== "undefined" && ActionRegistry)
                ActionRegistry.invokeActionWithContext(actionId, ctx)
        }
    }
}
