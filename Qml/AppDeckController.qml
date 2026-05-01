pragma Singleton

import QtQuick 2.15

// AppDeckController — global interaction controller for the single AppDeck renderer.
//
// All appdeck pointer/gesture events route here. Because there is exactly one
// AppDeck renderer (AppDeckWindow), there is no host-routing needed.
QtObject {
    id: root

    property string hoveredItemId: ""
    property string pressedItemId: ""
    property string activeItemId: ""
    property string dragItemId: ""
    property bool dragActive: false
    property real dragPosition: 0
    property string contextItemId: ""
    property real lastHoverPosition: 0

    // Interaction geometry — owned here, not by the renderer.
    property real hoverX: 0
    property bool dockHovered: false

    // inputOwnerHost kept for compatibility with existing AppDeck renderer call-sites.
    // With a single renderer it is always "appdeck".
    readonly property string inputOwnerHost: "appdeck"

    function _syncDockStateToShell() {
        if (typeof ShellStateController === "undefined" || !ShellStateController) {
            return
        }
        if (ShellStateController.setDockHoveredItem) {
            ShellStateController.setDockHoveredItem(String(hoveredItemId || ""))
        }
        if (ShellStateController.setDockActiveItem) {
            // Context menu owner acts as active item state.
            ShellStateController.setDockActiveItem(String(contextItemId || ""))
        }
    }

    function _syncDockStateFromShell() {
        if (typeof ShellStateController === "undefined" || !ShellStateController) {
            return
        }
        hoveredItemId = String(ShellStateController.dockHoveredItem || "")
        contextItemId = String(ShellStateController.dockActiveItem || "")
    }

    function setInputOwner(hostName) {
        // No-op: single renderer always owns input.
        void hostName
    }

    onHoveredItemIdChanged: _syncDockStateToShell()
    onContextItemIdChanged: _syncDockStateToShell()

    function onHover(itemId, position, hostName) {
        hoveredItemId     = String(itemId || "")
        lastHoverPosition = Number(position || 0)
        hoverX            = Number(position || 0)
        _syncDockStateToShell()
    }

    function onPress(itemId, hostName) {
        pressedItemId = String(itemId || "")
    }

    function onRelease(itemId, hostName) {
        if (pressedItemId === String(itemId || "") && !dragActive) {
            activeItemId = String(itemId || "")
        }
        pressedItemId = ""
    }

    function onActivate(itemId, hostName) {
        activeItemId = String(itemId || "")
        if (contextItemId.length > 0) {
            contextItemId = ""
            _syncDockStateToShell()
        }
    }

    function onDragStart(itemId, hostName) {
        dragItemId   = String(itemId || "")
        dragActive   = true
        dragPosition = 0
        if (typeof ShellStateController !== "undefined" && ShellStateController
                && ShellStateController.setDragSession) {
            ShellStateController.setDragSession({
                "source": "appdeck",
                "source_component": "appdeck",
                "object_type": "app_entry",
                "item_id": dragItemId,
                "mime": ["application/x-desktop-entry"],
                "capabilities": ["launch", "reorder"],
                "allowed_operations": ["move"],
                "preferred_action": "move",
                "target_hints": {
                    "host": String(hostName || "appdeck")
                },
                "active": true,
                "position": Number(dragPosition || 0)
            })
        }
    }

    function onDragMove(deltaX, hostName) {
        if (dragActive) {
            dragPosition = Number(dragPosition || 0) + Number(deltaX || 0)
            if (typeof ShellStateController !== "undefined" && ShellStateController
                    && ShellStateController.setDragSession) {
                ShellStateController.setDragSession({
                    "source": "appdeck",
                    "source_component": "appdeck",
                    "object_type": "app_entry",
                    "item_id": String(dragItemId || ""),
                    "mime": ["application/x-desktop-entry"],
                    "capabilities": ["launch", "reorder"],
                    "allowed_operations": ["move"],
                    "preferred_action": "move",
                    "target_hints": {
                        "host": String(hostName || "appdeck")
                    },
                    "active": true,
                    "position": Number(dragPosition || 0)
                })
            }
        }
    }

    function onDragEnd(hostName) {
        dragActive   = false
        dragItemId   = ""
        dragPosition = 0
        if (typeof ShellStateController !== "undefined" && ShellStateController) {
            if (ShellStateController.clearDragSession) {
                ShellStateController.clearDragSession()
            } else if (ShellStateController.setDragSession) {
                ShellStateController.setDragSession({})
            }
        }
    }

    function onContextMenu(itemId, hostName) {
        contextItemId = String(itemId || "")
        _syncDockStateToShell()
    }

    function onLeave(hostName) {
        hoveredItemId = ""
        pressedItemId = ""
        dockHovered   = false
        contextItemId = ""
        _syncDockStateToShell()
    }

    Component.onCompleted: {
        _syncDockStateFromShell()
        if (typeof ShellStateController !== "undefined" && ShellStateController) {
            try {
                ShellStateController.dockHoveredItemChanged.connect(root._syncDockStateFromShell)
                ShellStateController.dockActiveItemChanged.connect(root._syncDockStateFromShell)
            } catch (e) {}
        }
    }
}
