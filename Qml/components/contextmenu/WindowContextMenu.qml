import QtQuick
import QtQuick.Controls
import Slm_Desktop

// WindowContextMenu — context menu for window title bars / compositor windows.
//
// Usage: call popup(windowId, appId, x, y) from the window decoration or
// compositor surface component.

Item {
    id: root

    // Signals dispatched to the compositor/workspace layer.
    signal minimize(string windowId)
    signal maximize(string windowId)
    signal fullscreen(string windowId)
    signal moveToWorkspace(string windowId)
    signal moveToDisplay(string windowId)
    signal alwaysOnTop(string windowId)
    signal focusApp(string appId)
    signal revealInDock(string appId)
    signal pinToDock(string appId)
    signal close(string windowId)

    // Call to show the menu at screen coordinates (sx, sy).
    function popup(windowId, appId, sx, sy) {
        windowMenu.surfaceContext = {
            "type": "window",
            "windowId": windowId,
            "appId": appId
        }
        windowMenu.popupAt(sx, sy)
    }

    ContextMenu {
        id: windowMenu

        onAction: function(id, ctx) {
            var wid = ctx.windowId || ""
            var aid = ctx.appId || ""
            switch (id) {
            case "minimize":          root.minimize(wid);          break
            case "maximize":          root.maximize(wid);          break
            case "fullscreen":        root.fullscreen(wid);        break
            case "move_to_workspace": root.moveToWorkspace(wid);   break
            case "move_to_display":   root.moveToDisplay(wid);     break
            case "always_on_top":     root.alwaysOnTop(wid);       break
            case "focus_app":         root.focusApp(aid);          break
            case "reveal_in_dock":    root.revealInDock(aid);      break
            case "pin_to_dock":       root.pinToDock(aid);         break
            case "close":             root.close(wid);             break
            default: break
            }
        }
    }
}
