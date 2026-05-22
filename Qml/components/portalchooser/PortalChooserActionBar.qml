import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop
import SlmStyle as DSStyle

Row {
    id: root

    property string selectionSummaryText: ""
    property bool showClearSelection: false
    property bool showInvertSelection: false
    property string primaryText: "Open"
    property bool primaryEnabled: false

    signal clearSelectionRequested()
    signal invertSelectionRequested()
    signal cancelRequested()
    signal primaryRequested()

    width: parent ? parent.width : 0
    height: Theme.metric("controlHeightLarge")
    spacing: Theme.metric("spacingSm")

    Item {
        width: Math.max(0, root.width - 224)
        height: root.height

        Row {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            DSStyle.Label {
                text: root.selectionSummaryText
                visible: text.length > 0
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("small")
            }

            DSStyle.Button {
                width: 108
                height: 26
                text: "Clear Selection"
                visible: root.showClearSelection
                enabled: visible
                onClicked: root.clearSelectionRequested()
            }

            DSStyle.Button {
                width: 112
                height: 26
                text: "Invert Selection"
                visible: root.showInvertSelection
                enabled: visible
                onClicked: root.invertSelectionRequested()
            }
        }
    }

    DSStyle.Button {
        text: "Cancel"
        width: 96
        height: Theme.metric("controlHeightRegular")
        anchors.verticalCenter: parent.verticalCenter
        onClicked: root.cancelRequested()
    }

    DSStyle.Button {
        text: root.primaryText
        width: 96
        height: Theme.metric("controlHeightRegular")
        anchors.verticalCenter: parent.verticalCenter
        enabled: root.primaryEnabled
        defaultAction: true
        onClicked: root.primaryRequested()
    }
}
