import QtQuick
import QtQuick.Controls
import QtQuick.Templates as T
import Slm_Desktop

T.Menu {
    id: control
    popupType: Popup.Item
    cascade: true

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding)

    margins: 0
    overlap: 0
    padding: Theme.metric("spacingXs")

    delegate: MenuItem {}

    contentItem: ListView {
        implicitHeight: contentHeight
        model: control.contentModel
        interactive: Window.window
                     ? contentHeight + control.topPadding + control.bottomPadding > control.height
                     : false
        clip: true
        currentIndex: control.currentIndex

        ScrollIndicator.vertical: ScrollIndicator {}
    }

    background: PopupSurface {
        implicitWidth: 244
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
