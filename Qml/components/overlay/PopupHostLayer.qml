import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

Item {
    id: root

    required property var rootWindow
    property int popupGap: Theme.metric("spacingSm")
    property int popupMargin: Theme.metric("spacingSm")

    readonly property bool hasActivePopup: !!activePopupObject

    property var activePopupObject: null
    property bool activePopupOwnedByHost: false

    function _closeAndDestroyActivePopup() {
        if (!activePopupObject) {
            return
        }
        var popup = activePopupObject
        activePopupObject = null
        var popupOwnedByHost = activePopupOwnedByHost
        activePopupOwnedByHost = false
        if (popup.close) {
            popup.close()
        }
        if (popupOwnedByHost) {
            popup.destroy()
        } else if (popup.hostManagedPosition !== undefined) {
            popup.hostManagedPosition = false
        }
    }

    function _monitorRect(anchorRect) {
        var rect = (anchorRect && anchorRect.monitorRect) ? anchorRect.monitorRect : null
        if (rect && rect.width > 0 && rect.height > 0) {
            return rect
        }
        if (rootWindow && rootWindow.screen) {
            return {
                "x": Number(rootWindow.screen.virtualX || 0),
                "y": Number(rootWindow.screen.virtualY || 0),
                "width": Number(rootWindow.screen.width || rootWindow.width || 1920),
                "height": Number(rootWindow.screen.height || rootWindow.height || 1080)
            }
        }
        return {
            "x": 0,
            "y": 0,
            "width": Number(rootWindow ? rootWindow.width : 1920),
            "height": Number(rootWindow ? rootWindow.height : 1080)
        }
    }

    function _clamp(value, minV, maxV) {
        return Math.max(minV, Math.min(maxV, value))
    }

    function _resolvePopupPosition(anchorRect, popupWidth, popupHeight, alignment) {
        var a = anchorRect || {}
        var monitor = _monitorRect(a)
        var ax = Number(a.x || 0)
        var ay = Number(a.y || 0)
        var aw = Number(a.width || 0)
        var ah = Number(a.height || 0)

        var minX = monitor.x + popupMargin
        var maxX = monitor.x + monitor.width - popupWidth - popupMargin
        var minY = monitor.y + popupMargin
        var maxY = monitor.y + monitor.height - popupHeight - popupMargin

        var x = ax
        var align = String(alignment || "center")
        if (align === "right") {
            x = ax + aw - popupWidth
        } else if (align === "center") {
            x = ax + ((aw - popupWidth) * 0.5)
        }
        x = _clamp(Math.round(x), Math.round(minX), Math.round(maxX))

        var belowY = ay + ah + popupGap
        var aboveY = ay - popupHeight - popupGap
        var y = belowY
        if (belowY > maxY && aboveY >= minY) {
            y = aboveY
        }
        y = _clamp(Math.round(y), Math.round(minY), Math.round(maxY))
        return Qt.point(x, y)
    }

    function _preferredWidth(defaultWidth) {
        var preferred = TopBarPopupController.activePreferredSize || ({})
        var width = Number(preferred.width || 0)
        return width > 0 ? width : defaultWidth
    }

    function _preferredHeight(defaultHeight) {
        var preferred = TopBarPopupController.activePreferredSize || ({})
        var height = Number(preferred.height || 0)
        return height > 0 ? height : defaultHeight
    }

    function _applyGeometryToPopup(popup) {
        if (!popup) {
            return
        }
        var width = _preferredWidth(Number(popup.width || 0))
        var height = _preferredHeight(Number(popup.height || 0))
        if (popup.width !== undefined) {
            popup.width = width
        }
        if (popup.height !== undefined) {
            popup.height = height
        }

        var pos = _resolvePopupPosition(TopBarPopupController.activeAnchorRectGlobal,
                                        Number(width || 0),
                                        Number(height || 0),
                                        TopBarPopupController.activeAlignment)
        popup.x = pos.x
        popup.y = pos.y
    }

    function _openRequestedPopup() {
        _closeAndDestroyActivePopup()
        var comp = TopBarPopupController.activePopupComponent
        if (!comp) {
            return
        }
        var popup = null
        if (comp.createObject) {
            popup = comp.createObject(null)
            activePopupOwnedByHost = !!popup
        } else {
            popup = comp
            activePopupOwnedByHost = false
        }
        if (!popup) {
            activePopupOwnedByHost = false
            TopBarPopupController.closeActivePopup()
            return
        }

        popup.parent = root
        if (popup.hostManagedPosition !== undefined) {
            popup.hostManagedPosition = true
        }
        if (popup.popupType !== undefined) {
            popup.popupType = Popup.Window
        }
        if (popup.closePolicy !== undefined) {
            popup.closePolicy = Popup.CloseOnEscape | Popup.CloseOnPressOutside
        }
        if (popup.modal !== undefined) {
            popup.modal = false
        }
        if (popup.focus !== undefined) {
            popup.focus = false
        }
        _applyGeometryToPopup(popup)

        activePopupObject = popup

        if (popup.open) {
            if (popup.opened !== true && popup.visible !== true) {
                popup.open()
            }
        } else if (popup.visible !== undefined) {
            popup.visible = true
        }
    }

    Connections {
        target: root.activePopupObject
        ignoreUnknownSignals: true
        function onAboutToHide() {
            TopBarPopupController.closeActivePopup()
        }
        function onClosed() {
            TopBarPopupController.closeActivePopup()
        }
        function onWidthChanged() {
            root._applyGeometryToPopup(root.activePopupObject)
        }
        function onHeightChanged() {
            root._applyGeometryToPopup(root.activePopupObject)
        }
    }

    Connections {
        target: TopBarPopupController
        ignoreUnknownSignals: true
        function onRequestSerialChanged() {
            if (!TopBarPopupController.activeAppletId.length || !TopBarPopupController.activePopupComponent) {
                root._closeAndDestroyActivePopup()
                return
            }
            root._openRequestedPopup()
        }
    }
}
