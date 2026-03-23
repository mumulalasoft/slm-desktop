import QtQuick
import QtQuick.Window
import Slm_Desktop

Menu {
    id: control

    property int popupGap: Theme.metric("spacingSm")
    property int popupWidth: Theme.metric("popupWidthS")
    property var anchorItem: null
    popupType: Popup.Window
    modal: false
    focus: false
    dim: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    z: 2000
    implicitWidth: popupWidth
    implicitHeight: Math.max(40, implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding)
    width: popupWidth
    height: implicitHeight
    function anchorGlobalX() {
        if (!anchorItem || !anchorItem.mapToGlobal) {
            return 0
        }
        return Number(anchorItem.mapToGlobal(0, 0).x)
    }

    function anchorGlobalY() {
        if (!anchorItem || !anchorItem.mapToGlobal) {
            return 0
        }
        return Number(anchorItem.mapToGlobal(0, 0).y)
    }

    function clampX(rawX) {
        var minX = 8
        var maxX = 100000
        if (Screen && Screen.desktopAvailableWidth > 0) {
            maxX = Math.round(Screen.desktopAvailableWidth - control.width - 8)
        } else if (Screen && Screen.width > 0) {
            maxX = Math.round(Screen.width - control.width - 8)
        }
        return Math.max(minX, Math.min(maxX, Math.round(rawX)))
    }

    function clampY(rawY) {
        var minY = 8
        var maxY = 100000
        if (Screen && Screen.desktopAvailableHeight > 0) {
            maxY = Math.round(Screen.desktopAvailableHeight - control.height - 8)
        } else if (Screen && Screen.height > 0) {
            maxY = Math.round(Screen.height - control.height - 8)
        }
        return Math.max(minY, Math.min(maxY, Math.round(rawY)))
    }

    x: clampX(anchorGlobalX() + (anchorItem ? anchorItem.width : 0) - width)
    y: clampY(anchorGlobalY() + (anchorItem ? anchorItem.height : 0) + popupGap)
    topPadding: 0
    bottomPadding: 0
    padding: Theme.metric("spacingXs")

    background: PopupSurface {
        implicitWidth: control.popupWidth
        implicitHeight: 40
        popupRadius: Theme.radiusWindowAlt
        popupOpacity: Theme.popupSurfaceOpacityStrong
        elevation: "high"
    }
}
