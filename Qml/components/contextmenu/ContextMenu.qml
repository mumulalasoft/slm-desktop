import QtQuick
import QtQuick.Controls
import Slm_Desktop

// ContextMenu — reusable popup driven by ContextMenuService.buildMenu().
//
// Usage:
//   ContextMenu {
//       id: myMenu
//       surface: "file"          // surface type passed to buildMenu
//       surfaceContext: { type: "file", mimeType: "image/png", filePath: "/home/..." }
//       onAction: function(id, ctx) { /* handle built-in action */ }
//   }
//   // Then call:  myMenu.popup()   or   myMenu.popupAt(x, y)
//
// Extension actions (.desktop) are dispatched through ActionRegistry directly.

Menu {
    id: root

    // The full context map passed to ContextMenuService.buildMenu().
    // Callers must populate at minimum the "type" key.
    property var surfaceContext: ({})

    // Emitted when the user triggers a built-in action.
    // id  — action identifier (e.g. "open", "rename", "copy")
    // ctx — the surfaceContext at the time of trigger
    signal action(string id, var ctx)

    // Emitted when an extension (.desktop) action is invoked.
    signal extensionAction(string actionId, var ctx)

    // ── internal state ───────────────────────────────────────────────────────
    property var _items: []

    // Populate the model just before showing so the list is always fresh.
    onAboutToShow: {
        _rebuildItems()
    }

    // ── public helpers ───────────────────────────────────────────────────────
    function popupAt(x, y) {
        _rebuildItems()
        popup(x, y)
    }

    // ── internal ─────────────────────────────────────────────────────────────
    function _rebuildItems() {
        if (typeof ContextMenuService === "undefined" || !ContextMenuService)
            return
        root._items = ContextMenuService.buildMenu(root.surfaceContext) || []
        _syncModel()
    }

    function _syncModel() {
        // Clear existing dynamic items (keep only statically declared ones,
        // which there are none here).
        while (root.count > 0)
            root.removeItem(root.itemAt(0))

        _buildLevel(root, root._items)
    }

    function _buildLevel(parent, items) {
        for (var i = 0; i < items.length; ++i) {
            var it = items[i]
            if (!it || it.visible === false)
                continue

            if (it.isSeparator === true) {
                var sep = separatorComponent.createObject(parent)
                parent.addItem(sep)
                continue
            }

            var children = it.children || []
            if (children.length > 0) {
                // Submenu — Menu has no icon property; title only.
                var sub = submenuComponent.createObject(parent, {
                    "title": it.label || "",
                    "_itemData": it
                })
                _buildLevel(sub, children)
                parent.addMenu(sub)
            } else {
                // Leaf item
                var leaf = menuItemComponent.createObject(parent, {
                    "text": it.label || "",
                    "iconName": it.icon || "",
                    "enabled": it.enabled !== false,
                    "_itemData": it
                })
                parent.addItem(leaf)
            }
        }
    }

    // ── component pool ───────────────────────────────────────────────────────
    Component {
        id: menuItemComponent
        MenuItem {
            property var _itemData: ({})
            property string iconName: ""

            icon.name: iconName
            onTriggered: {
                var id = _itemData.id || ""
                // Extension actions have a "desktopActionId" field set by ActionRegistry.
                if (_itemData.desktopActionId) {
                    root.extensionAction(_itemData.desktopActionId, root.surfaceContext)
                } else {
                    root.action(id, root.surfaceContext)
                }
            }
        }
    }

    Component {
        id: separatorComponent
        MenuSeparator {}
    }

    Component {
        id: submenuComponent
        Menu {
            property var _itemData: ({})
        }
    }
}
