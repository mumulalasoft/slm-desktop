import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Templates as T
import Slm_Desktop
import Style as DSStyle

T.Dialog {
    id: control

    property int dialogWidth: 332
    property int bodyPadding: Theme.metric("spacingMd")
    property int footerPadding: Theme.metric("spacingMd")
    property Component bodyComponent
    property Component footerComponent
    property bool showDefaultCloseFooter: false
    property bool showFooterDivider: true
    property string closeButtonText: "Close"

    modal: true
    focus: true
    width: dialogWidth
    padding: 0
    spacing: 0
    font.family: Theme.fontFamilyUi
    font.pixelSize: Theme.fontSize("body")

    background: DSStyle.PopupSurface {
        implicitWidth: control.dialogWidth
        implicitHeight: 164
        popupRadius: Theme.radiusWindowAlt
        popupColor: Theme.color("surface")
        popupBorderColor: Theme.color("panelBorder")
        popupOpacity: Theme.cardSurfaceOpacity
        elevation: "high"
    }

    header: Item {
        visible: control.title && control.title.length > 0
        implicitHeight: visible ? (Theme.metric("controlHeightRegular") + Theme.metric("spacingXxs")) : 0

        DSStyle.Label {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: Theme.metric("spacingMd")
            anchors.rightMargin: Theme.metric("spacingMd")
            anchors.topMargin: Theme.metric("spacingXs")
            text: control.title
            color: Theme.color("textPrimary")
            font.family: Theme.fontFamilyDisplay
            font.pixelSize: Theme.fontSize("subtitle")
            font.weight: Theme.fontWeight("semibold")
            elide: Text.ElideRight
            lineHeightMode: Text.ProportionalHeight
            lineHeight: Theme.lineHeight("tight")
            verticalAlignment: Text.AlignVCenter
        }
    }

    contentItem: Item {
        implicitWidth: control.dialogWidth
        implicitHeight: (bodyLoader.item ? bodyLoader.item.implicitHeight : 0) + (control.bodyPadding * 2)

        Loader {
            id: bodyLoader
            anchors.fill: parent
            anchors.margins: control.bodyPadding
            sourceComponent: control.bodyComponent
        }
    }

    footer: Item {
        visible: !!control.footerComponent || control.showDefaultCloseFooter
        implicitWidth: control.dialogWidth
        readonly property bool hasCustomFooter: !!control.footerComponent
        readonly property bool hasAnyFooter: hasCustomFooter || control.showDefaultCloseFooter
        implicitHeight: (footerLoader.item ? footerLoader.item.implicitHeight : 0)
                        + (divider.visible ? divider.height : 0)
                        + (defaultFooter.visible ? defaultFooter.implicitHeight : 0)
                        + (control.footerPadding * 2)

        Rectangle {
            id: divider
            visible: control.showFooterDivider && footer.hasAnyFooter
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 1
            color: Theme.color("panelBorder")
            opacity: Theme.darkMode ? 0.65 : 0.9
        }

        Loader {
            id: footerLoader
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: divider.bottom
            anchors.leftMargin: control.footerPadding
            anchors.rightMargin: control.footerPadding
            anchors.topMargin: control.footerPadding
            sourceComponent: control.footerComponent
        }

        DSStyle.DialogButtonBox {
            id: defaultFooter
            visible: control.showDefaultCloseFooter && !control.footerComponent
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: control.footerPadding
            anchors.rightMargin: control.footerPadding
            anchors.bottomMargin: control.footerPadding
            alignment: Qt.AlignRight

            DSStyle.Button {
                text: control.closeButtonText
                onClicked: control.close()
            }
        }
    }
}
