import QtQuick
import QtQuick.Controls
import QtQuick.Templates as T
import Slm_Desktop

T.Dialog {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding)

    padding: Theme.metric("spacingXl")
    spacing: Theme.metric("spacingMd")
    font.family: Theme.fontFamilyUi
    font.pixelSize: Theme.fontSize("body")

    background: PopupSurface {
        implicitWidth: 320
        implicitHeight: 160
        popupRadius: Theme.radiusWindowAlt
        popupColor: Theme.color("surface")
        popupBorderColor: Theme.color("panelBorder")
        popupOpacity: Theme.cardSurfaceOpacity
        elevation: "high"
    }

    header: Item {
        visible: control.title && control.title.length > 0
        implicitHeight: visible ? (Theme.metric("controlHeightRegular") + Theme.metric("spacingXs")) : 0

        Label {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: Theme.metric("spacingLg")
            anchors.rightMargin: Theme.metric("spacingLg")
            anchors.topMargin: Theme.metric("spacingXs")
            text: control.title
            color: Theme.color("textPrimary")
            font.family: Theme.fontFamilyDisplay
            font.pixelSize: Theme.fontSize("subtitle")
            font.weight: Theme.fontWeight("semibold")
            elide: Text.ElideRight
            lineHeightMode: Text.ProportionalHeight
            lineHeight: Theme.lineHeight("tight")
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
        }
    }
}
