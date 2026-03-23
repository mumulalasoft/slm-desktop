import QtQuick 2.15
import QtQuick.Controls 2.15
import Slm_Desktop

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
    height: 34
    spacing: 8

    Item {
        width: Math.max(0, root.width - 224)
        height: root.height

        Row {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            Label {
                text: root.selectionSummaryText
                visible: text.length > 0
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("small")
            }

            Button {
                width: 108
                height: 26
                text: "Clear Selection"
                visible: root.showClearSelection
                enabled: visible
                onClicked: root.clearSelectionRequested()
            }

            Button {
                width: 112
                height: 26
                text: "Invert Selection"
                visible: root.showInvertSelection
                enabled: visible
                onClicked: root.invertSelectionRequested()
            }
        }
    }

    Button {
        text: "Cancel"
        width: 96
        height: 32
        onClicked: root.cancelRequested()
    }

    Button {
        text: root.primaryText
        width: 96
        height: 32
        enabled: root.primaryEnabled
        onClicked: root.primaryRequested()
    }
}
