pragma Singleton

import QtQuick 2.15

QtObject {
    id: root

    property string activeAppletId: ""
    property var activeAnchorRectGlobal: ({})
    property var activePopupComponent: null
    property var activePreferredSize: ({})
    property string activeAlignment: "center"
    property int requestSerial: 0

    function requestTogglePopup(appletId, anchorRectGlobal, popupComponent, preferredSize, alignment) {
        var id = String(appletId || "")
        if (!id.length) {
            return
        }
        if (root.activeAppletId === id) {
            if (popupComponent && (popupComponent.opened === true || popupComponent.visible === true)) {
                closeActivePopup()
                return
            }
            // Same applet requested while popup is not currently visible:
            // treat as refresh/reposition rather than toggle-close.
        } else if (root.activeAppletId.length > 0) {
            // Ensure one-active-popup invariant across all applets.
            closeActivePopup()
        }
        root.activeAppletId = id
        root.activeAnchorRectGlobal = anchorRectGlobal || ({})
        root.activePopupComponent = popupComponent || null
        root.activePreferredSize = preferredSize || ({})
        root.activeAlignment = String(alignment || "center")
        root.requestSerial = root.requestSerial + 1
    }

    function closeActivePopup() {
        if (!root.activeAppletId.length && !root.activePopupComponent) {
            return
        }
        root.activeAppletId = ""
        root.activeAnchorRectGlobal = ({})
        root.activePopupComponent = null
        root.activePreferredSize = ({})
        root.activeAlignment = "center"
        root.requestSerial = root.requestSerial + 1
    }

    function isPopupOpen(appletId) {
        return root.activeAppletId === String(appletId || "")
    }
}
