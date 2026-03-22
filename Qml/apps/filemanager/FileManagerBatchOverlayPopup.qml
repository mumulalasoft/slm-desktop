import QtQuick 2.15
import QtQuick.Controls 2.15
import Desktop_Shell

Popup {
    id: root

    required property var hostRoot

    visible: false
    enabled: false
    parent: Overlay.overlay
    modal: false
    focus: false
    closePolicy: Popup.NoAutoClose
    x: Math.round(((parent ? parent.width : hostRoot.width) - width) * 0.5)
    y: 10
    width: Math.min(460, Math.max(280, hostRoot.width * 0.46))
    height: 96
    padding: 0

    background: Rectangle {
        radius: Theme.radiusControlLarge
        color: Theme.color("windowCard")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("windowCardBorder")
        opacity: Theme.opacitySurfaceStrong
    }

    contentItem: Column {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        Row {
            width: parent.width
            spacing: 8
            Text {
                text: String(hostRoot.batchOverlayTitle || "Processing")
                color: Theme.color("textPrimary")
                font.family: Theme.fontFamilyUi
                font.pixelSize: Theme.fontSize("smallPlus")
                font.weight: Theme.fontWeight("semibold")
            }
            Text {
                text: String(hostRoot.batchOverlayDetail || "")
                color: Theme.color("textSecondary")
                font.family: Theme.fontFamilyUi
                font.pixelSize: Theme.fontSize("small")
                elide: Text.ElideRight
                width: parent.width - 110
            }
        }

        ProgressBar {
            width: parent.width
            height: 10
            from: 0
            to: 1
            value: hostRoot.batchOverlayProgress
            indeterminate: hostRoot.batchOverlayIndeterminate
        }

        Row {
            width: parent.width
            spacing: 8

            Button {
                text: hostRoot.batchOverlayPaused ? "Resume" : "Pause"
                enabled: hostRoot.batchOverlayVisible
                onClicked: hostRoot.toggleBatchPauseResume()
            }

            Button {
                text: "Cancel"
                enabled: hostRoot.batchOverlayVisible
                onClicked: hostRoot.cancelBatchOperation()
            }
        }
    }
}
