import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Templates as T
import Slm_Desktop
import Style as DSStyle

T.Dialog {
    id: control

    property int dialogWidth: 360
    property int bodyPadding: Theme.metric("spacingLg")
    property int footerPadding: Theme.metric("spacingSm")
    property Component bodyComponent
    property Component footerComponent
    property bool showDefaultCloseFooter: false
    property string closeButtonText: "Close"

    modal: true
    focus: true
    width: dialogWidth
    padding: 0
    spacing: 0
    font.family: Theme.fontFamilyUi
    font.pixelSize: Theme.fontSize("body")

    background: Rectangle {
        implicitWidth: control.dialogWidth
        implicitHeight: 180
        radius: Theme.radiusWindowAlt
        antialiasing: true
        color: Theme.color("windowCard")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("windowCardBorder")
        opacity: Theme.cardSurfaceOpacity
    }

    header: Item {
        visible: control.title && control.title.length > 0
        implicitHeight: visible ? (Theme.metric("controlHeightLarge") + Theme.metric("spacingXs")) : 0

        DSStyle.Label {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: Theme.metric("spacingLg")
            anchors.rightMargin: Theme.metric("spacingLg")
            anchors.topMargin: Theme.metric("spacingSm")
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
        implicitHeight: (footerLoader.item ? footerLoader.item.implicitHeight : 0)
                        + (defaultFooter.visible ? defaultFooter.implicitHeight : 0)
                        + (control.footerPadding * 2)

        Loader {
            id: footerLoader
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: control.footerPadding
            sourceComponent: control.footerComponent
        }

        DSStyle.DialogButtonBox {
            id: defaultFooter
            visible: control.showDefaultCloseFooter && !control.footerComponent
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: control.footerPadding
            alignment: Qt.AlignRight

            DSStyle.Button {
                text: control.closeButtonText
                onClicked: control.close()
            }
        }
    }
}
