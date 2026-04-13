pragma Singleton

import QtQuick 2.15

// DockController — global interaction controller for the single Dock renderer.
//
// All dock pointer/gesture events route here. Because there is exactly one
// Dock renderer (DockWindow), there is no host-routing needed.
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

    // inputOwnerHost kept for compatibility with existing Dock renderer call-sites.
    // With a single renderer it is always "dock".
    readonly property string inputOwnerHost: "dock"

    function setInputOwner(hostName) {
        // No-op: single renderer always owns input.
        void hostName
    }

    function onHover(itemId, position, hostName) {
        hoveredItemId     = String(itemId || "")
        lastHoverPosition = Number(position || 0)
        hoverX            = Number(position || 0)
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
    }

    function onDragStart(itemId, hostName) {
        dragItemId   = String(itemId || "")
        dragActive   = true
        dragPosition = 0
    }

    function onDragMove(deltaX, hostName) {
        if (dragActive) {
            dragPosition = Number(dragPosition || 0) + Number(deltaX || 0)
        }
    }

    function onDragEnd(hostName) {
        dragActive   = false
        dragItemId   = ""
        dragPosition = 0
    }

    function onContextMenu(itemId, hostName) {
        contextItemId = String(itemId || "")
    }

    function onLeave(hostName) {
        hoveredItemId = ""
        pressedItemId = ""
        dockHovered   = false
    }
}
