import QtQuick
import QtQuick.Controls
import QtQuick.Templates as T
import Slm_Desktop

T.Popup {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding)

    margins: 0
    padding: Theme.metric("spacingXs")

    background: PopupSurface {
        implicitWidth: 236
        implicitHeight: 40
        popupRadius: Theme.radiusWindowAlt
        popupColor: Theme.color("menuBg")
        popupBorderColor: Theme.color("menuBorder")
        popupOpacity: Theme.popupSurfaceOpacityStrong
        elevation: "high"
    }

    T.Overlay.modal: Rectangle {
        color: Theme.color("overlay")
    }

    T.Overlay.modeless: Rectangle {
        color: "transparent"
    }
}
