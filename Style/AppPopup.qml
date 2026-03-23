import QtQuick
import QtQuick.Controls
import QtQuick.Templates as T
import Slm_Desktop

T.Popup {
    id: control

    property real popupRadius: Theme.radiusPopup
    property color popupColor: Theme.color("windowCard")
    property color popupBorderColor: Theme.color("windowCardBorder")
    property real popupOpacity: Theme.cardSurfaceOpacity

    padding: Theme.metric("spacingSm")

    background: PopupSurface {
        implicitWidth: 320
        implicitHeight: 120
        popupRadius: Math.max(control.popupRadius, Theme.radiusWindowAlt)
        popupColor: control.popupColor
        popupBorderColor: control.popupBorderColor
        popupOpacity: control.popupOpacity
        elevation: "high"
    }

    // Keep AppPopup minimal for Wayland stability.
    // Overlay visuals are configured per-usage when needed.
}
