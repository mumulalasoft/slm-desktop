import QtQuick
import Slm_Desktop
import Style as DSStyle

Item {
    id: control

    property string title: ""
    property int outerMargin: Theme.metric("spacingLg")
    property int contentSpacing: Theme.metric("spacingMd")
    property Component bodyComponent
    property Component footerComponent
    property bool showDivider: true

    implicitWidth: 380
    implicitHeight: (layoutColumn.implicitHeight + (outerMargin * 2))

    DSStyle.PopupSurface {
        anchors.fill: parent
        popupRadius: Theme.radiusWindowAlt
        popupColor: Theme.color("surface")
        popupBorderColor: Theme.color("panelBorder")
        popupOpacity: Theme.cardSurfaceOpacity
        elevation: "medium"
    }

    Column {
        id: layoutColumn
        anchors.fill: parent
        anchors.margins: control.outerMargin
        spacing: control.contentSpacing

        DSStyle.Label {
            visible: control.title.length > 0
            width: parent.width
            text: control.title
            color: Theme.color("textPrimary")
            font.family: Theme.fontFamilyDisplay
            font.pixelSize: Theme.fontSize("subtitle")
            font.weight: Theme.fontWeight("semibold")
            elide: Text.ElideRight
            lineHeightMode: Text.ProportionalHeight
            lineHeight: Theme.lineHeight("tight")
        }

        Loader {
            id: bodyLoader
            width: parent.width
            sourceComponent: control.bodyComponent
        }

        Rectangle {
            visible: !!control.footerComponent && control.showDivider
            width: parent.width
            height: 1
            color: Theme.color("panelBorder")
            opacity: Theme.darkMode ? 0.65 : 0.9
        }

        Loader {
            id: footerLoader
            width: parent.width
            sourceComponent: control.footerComponent
        }
    }
}
