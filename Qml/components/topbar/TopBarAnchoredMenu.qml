import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

Menu {
    id: control

    property var anchorItem: null
    property var popupHost: null
    property int popupGap: Theme.metric("spacingSm")
    property int outerPadding: Theme.metric("spacingSm")
    property bool alignRightToAnchor: false
    property bool hostManagedPosition: false
    property string appletId: ""
    property string _runtimeAppletId: ""
    readonly property var anchorWindow: (anchorItem && anchorItem.Window && anchorItem.Window.window)
                                        ? anchorItem.Window.window
                                        : null

    readonly property var popupParent: popupHost
                                       ? popupHost
                                       : ((anchorWindow && anchorWindow.overlay)
                                          ? anchorWindow.overlay : null)
    readonly property bool windowPopupKind: control.popupType === Popup.Window

    parent: windowPopupKind ? null : popupParent
    popupType: Popup.Window
    z: 3000
    modal: false
    focus: false
    dim: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: Theme.metric("spacingSm")

    function _isWindowPopup() {
        return control.popupType === Popup.Window
    }

    function _anchorPoint() {
        if (!anchorItem) {
            return Qt.point(0, 0)
        }
        if (_isWindowPopup() && anchorItem.mapToGlobal) {
            return anchorItem.mapToGlobal(0, 0)
        }
        if (popupParent && anchorItem.mapToItem) {
            return anchorItem.mapToItem(popupParent, 0, 0)
        }
        if (anchorItem.mapToGlobal) {
            return anchorItem.mapToGlobal(0, 0)
        }
        return Qt.point(0, 0)
    }

    function _clampX(rawX) {
        var minX = outerPadding
        var maxWidth = 100000
        if (!_isWindowPopup() && popupParent && popupParent.width > 0) {
            maxWidth = popupParent.width
        } else if (Screen && Screen.width > 0) {
            maxWidth = Screen.width
        }
        var maxX = Math.max(minX, Math.round(maxWidth - control.width - outerPadding))
        return Math.max(minX, Math.min(maxX, Math.round(rawX)))
    }

    function _clampY(rawY) {
        var minY = outerPadding
        var maxHeight = 100000
        if (!_isWindowPopup() && popupParent && popupParent.height > 0) {
            maxHeight = popupParent.height
        } else if (Screen && Screen.height > 0) {
            maxHeight = Screen.height
        }
        var maxY = Math.max(minY, Math.round(maxHeight - control.height - outerPadding))
        return Math.max(minY, Math.min(maxY, Math.round(rawY)))
    }

    function _targetX() {
        var p = _anchorPoint()
        if (alignRightToAnchor) {
            return _clampX(p.x + (anchorItem ? anchorItem.width : 0) - control.width)
        }
        return _clampX(p.x)
    }

    function _targetY() {
        var p = _anchorPoint()
        return _clampY(p.y + (anchorItem ? anchorItem.height : 0) + popupGap)
    }

    Binding {
        target: control
        property: "x"
        when: !control.hostManagedPosition
        value: control._targetX()
    }
    Binding {
        target: control
        property: "y"
        when: !control.hostManagedPosition
        value: control._targetY()
    }

    onOpened: {
        if (!hostManagedPosition) {
            x = _targetX()
            y = _targetY()
        }
    }

    function resolvedAppletId() {
        if (String(appletId || "").length > 0) {
            return String(appletId)
        }
        return _runtimeAppletId
    }

    function buildAnchorRectGlobal() {
        if (!anchorItem || !anchorItem.mapToGlobal) {
            return ({})
        }
        var g = anchorItem.mapToGlobal(0, 0)
        var rect = {
            "x": Number(g.x || 0),
            "y": Number(g.y || 0),
            "width": Number(anchorItem.width || 0),
            "height": Number(anchorItem.height || 0)
        }
        var win = (anchorItem.Window && anchorItem.Window.window) ? anchorItem.Window.window : null
        if (win && win.screen) {
            rect.monitorRect = {
                "x": Number(win.screen.virtualX || 0),
                "y": Number(win.screen.virtualY || 0),
                "width": Number(win.screen.width || 0),
                "height": Number(win.screen.height || 0)
            }
        }
        return rect
    }

    Component.onCompleted: {
        if (!_runtimeAppletId.length) {
            _runtimeAppletId = "topbar-menu-" + Date.now() + "-" + Math.floor(Math.random() * 1000000)
        }
    }

    onAboutToShow: {
        TopBarPopupController.requestTogglePopup(
                    resolvedAppletId(),
                    buildAnchorRectGlobal(),
                    control,
                    { "width": Number(control.width || 0),
                      "height": Number(control.height || 0) },
                    alignRightToAnchor ? "right" : "left")
    }

    onAboutToHide: {
        if (TopBarPopupController.isPopupOpen(resolvedAppletId())) {
            TopBarPopupController.closeActivePopup()
        }
    }
}
