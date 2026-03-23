import QtQuick
import Slm_Desktop
import Style as DSStyle

Item {
    id: control

    property string title: ""
    property int outerMargin: Theme.metric("spacingMd")
    property int contentSpacing: Theme.metric("spacingSm")
    property Component bodyComponent
    property Component footerComponent
    property bool showDivider: true

    implicitWidth: 420
    implicitHeight: (layoutColumn.implicitHeight + (outerMargin * 2))

    DSStyle.PopupSurface {
        anchors.fill: parent
        popupRadius: Theme.radiusWindow
        popupOpacity: Theme.popupSurfaceOpacityStrong
        elevation: "high"
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
            font.pixelSize: Theme.fontSize("title")
            font.weight: Theme.fontWeight("bold")
            elide: Text.ElideRight
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
            color: Theme.color("windowCardBorder")
            opacity: 0.8
        }

        Loader {
            id: footerLoader
            width: parent.width
            sourceComponent: control.footerComponent
        }
    }
}
