import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

AppDialog {
    id: root

    required property var hostRoot
    property alias archiveNameText: compressArchiveNameField.text

    function openAndFocus(nameValue) {
        archiveNameText = String(nameValue || "")
        open()
        Qt.callLater(function() {
            compressArchiveNameField.forceActiveFocus()
            compressArchiveNameField.selectAll()
        })
    }

    title: "Compress"
    standardButtons: Dialog.NoButton
    x: Math.round((hostRoot.width - width) * 0.5)
    y: Math.round((hostRoot.height - height) * 0.5)
    width: 460

    contentItem: ColumnLayout {
        spacing: 8
        anchors.fill: parent
        anchors.margins: 14

        Label {
            text: "Archive name"
            color: Theme.color("textPrimary")
        }

        TextField {
            id: compressArchiveNameField
            Layout.fillWidth: true
            placeholderText: "Archive.tar"
            color: Theme.color("textPrimary")
            selectionColor: Theme.color("selectedItem")
            selectedTextColor: Theme.color("selectedItemText")
            background: Rectangle {
                radius: Theme.radiusControl
                color: Theme.color("fileManagerSearchBg")
                border.width: Theme.borderWidthThin
                border.color: compressArchiveNameField.activeFocus
                              ? Theme.color("accent")
                              : Theme.color("fileManagerControlBorder")
            }
            onAccepted: hostRoot.applyCompressSelection()
        }

        Label {
            text: "Format"
            color: Theme.color("textPrimary")
        }

        ComboBox {
            Layout.fillWidth: true
            model: ["tar", "zip"]
            currentIndex: hostRoot.compressFormat === "zip" ? 1 : 0
            onActivated: {
                var nextFmt = String(currentText || "tar")
                hostRoot.compressFormat = nextFmt
                var currentName = String(compressArchiveNameField.text || "").trim()
                if (currentName.length <= 0) {
                    return
                }
                if (currentName.endsWith(".tar") || currentName.endsWith(".zip")) {
                    currentName = currentName.replace(/\.(tar|zip)$/i, "")
                }
                compressArchiveNameField.text = currentName + (nextFmt === "zip" ? ".zip" : ".tar")
            }
        }

        Item {
            Layout.fillHeight: true
            Layout.preferredHeight: 6
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                Layout.fillWidth: true
                text: "Cancel"
                onClicked: root.close()
            }

            Button {
                Layout.fillWidth: true
                text: "Create"
                enabled: String(compressArchiveNameField.text || "").trim().length > 0
                onClicked: hostRoot.applyCompressSelection()
            }
        }
    }

    onClosed: {
        hostRoot.compressPendingPaths = []
    }
}
