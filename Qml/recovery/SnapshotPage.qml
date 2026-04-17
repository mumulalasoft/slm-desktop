import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property string statusMessage: ""
    property bool statusOk: true
    property string selectedSnapshotId: ""
    property var preview: ({ rows: [], changedCount: 0, rowCount: 0, exists: false })

    function reloadPreview() {
        if (!selectedSnapshotId || selectedSnapshotId.length === 0) {
            preview = ({ rows: [], changedCount: 0, rowCount: 0, exists: false })
            return
        }
        preview = RecoveryApp.previewSnapshotDiff(selectedSnapshotId) || ({ rows: [] })
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        Text {
            text: qsTr("Restore Snapshot")
            color: "white"
            font.pixelSize: 18
            font.bold: true
        }

        Text {
            text: qsTr("Select a snapshot to restore as the active configuration. "
                       + "Snapshots are named by timestamp (newest first).")
            color: "#aaa"
            font.pixelSize: 13
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: "#444" }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            Rectangle {
                Layout.preferredWidth: 340
                Layout.fillHeight: true
                radius: 6
                color: "#242424"
                border.color: "#444"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    Text {
                        text: qsTr("Snapshots")
                        color: "#e0e0e0"
                        font.pixelSize: 12
                        font.bold: true
                    }

                    Text {
                        visible: snapshotList.count === 0
                        text: qsTr("No snapshots available.")
                        color: "#888"
                        font.pixelSize: 13
                    }

                    ListView {
                        id: snapshotList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: RecoveryApp.availableSnapshots
                        spacing: 6

                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 44
                            radius: 4
                            border.width: 1
                            border.color: modelData === root.selectedSnapshotId ? "#c0392b" : "#444"
                            color: itemMouse.containsMouse ? "#343434" : "#2a2a2a"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12

                                Text {
                                    text: modelData
                                    color: "white"
                                    font.pixelSize: 12
                                    font.family: "monospace"
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                            }

                            MouseArea {
                                id: itemMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    root.selectedSnapshotId = modelData
                                    root.reloadPreview()
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 6
                color: "#242424"
                border.color: "#444"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: qsTr("Preview")
                            color: "#e0e0e0"
                            font.pixelSize: 12
                            font.bold: true
                        }

                        Item { Layout.fillWidth: true }

                        Button {
                            text: qsTr("Reload")
                            enabled: root.selectedSnapshotId.length > 0
                            onClicked: root.reloadPreview()
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: "#9e9e9e"
                        font.pixelSize: 12
                        text: root.selectedSnapshotId.length === 0
                              ? qsTr("Select a snapshot to preview differences.")
                              : qsTr("Snapshot: %1 • Changes: %2/%3")
                                    .arg(root.selectedSnapshotId)
                                    .arg(Number(root.preview.changedCount || 0))
                                    .arg(Number(root.preview.rowCount || 0))
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: !!root.preview.error
                        wrapMode: Text.WordWrap
                        color: "#ff8a80"
                        font.pixelSize: 12
                        text: String(root.preview.error || "")
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: "#3f3f3f" }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: root.preview.rows || []
                        spacing: 6
                        delegate: Rectangle {
                            width: ListView.view.width
                            radius: 4
                            color: "#2b2b2b"
                            border.width: 1
                            border.color: modelData.status === "same" ? "#3c3c3c" : "#6f4e4e"
                            implicitHeight: diffColumn.implicitHeight + 12

                            ColumnLayout {
                                id: diffColumn
                                anchors.fill: parent
                                anchors.margins: 6
                                spacing: 4

                                RowLayout {
                                    Layout.fillWidth: true
                                    Text {
                                        text: String(modelData.key || "")
                                        color: "white"
                                        font.pixelSize: 12
                                        font.bold: true
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        text: String(modelData.status || "")
                                        color: modelData.status === "same" ? "#9e9e9e" : "#ffcc80"
                                        font.pixelSize: 11
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("Current: %1").arg(String(modelData.active || ""))
                                    color: "#bdbdbd"
                                    font.pixelSize: 11
                                    wrapMode: Text.WordWrap
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("Snapshot: %1").arg(String(modelData.snapshot || ""))
                                    color: "#e0e0e0"
                                    font.pixelSize: 11
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Item { Layout.fillWidth: true }
                        Button {
                            text: qsTr("Restore Snapshot")
                            enabled: root.selectedSnapshotId.length > 0 && !root.preview.error
                            onClicked: confirmDialog.open()
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: statusLabel.implicitHeight + 16
            color: root.statusOk ? "#1b5e20" : "#b71c1c"
            radius: 4
            visible: root.statusMessage.length > 0

            Text {
                id: statusLabel
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                anchors.margins: 12
                text: root.statusMessage
                color: "white"
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }
        }
    }

    Popup {
        id: confirmDialog
        parent: root
        modal: true
        focus: true
        width: 520
        height: dialogContent.implicitHeight + 28
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            radius: 8
            color: "#262626"
            border.color: "#4a4a4a"
            border.width: 1
        }

        ColumnLayout {
            id: dialogContent
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            Text {
                text: qsTr("Confirm Snapshot Restore")
                color: "white"
                font.pixelSize: 15
                font.bold: true
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: "#d0d0d0"
                font.pixelSize: 12
                text: qsTr("This will replace active desktop configuration with snapshot \"%1\".")
                        .arg(root.selectedSnapshotId)
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: "#aaaaaa"
                font.pixelSize: 12
                text: qsTr("Type the snapshot id and check confirmation to continue.")
            }

            TextField {
                id: confirmInput
                Layout.fillWidth: true
                placeholderText: qsTr("Type snapshot id")
                color: "white"
                selectByMouse: true
            }

            CheckBox {
                id: confirmCheck
                text: qsTr("I understand and want to restore this snapshot")
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("Cancel")
                    onClicked: confirmDialog.close()
                }
                Button {
                    text: qsTr("Restore")
                    enabled: confirmCheck.checked && confirmInput.text === root.selectedSnapshotId
                    onClicked: {
                        const ok = RecoveryApp.restoreSnapshot(root.selectedSnapshotId)
                        root.statusOk = ok
                        root.statusMessage = ok
                            ? qsTr("Snapshot \"%1\" restored. Exit to restart the desktop.").arg(root.selectedSnapshotId)
                            : qsTr("Failed to restore snapshot \"%1\".").arg(root.selectedSnapshotId)
                        confirmDialog.close()
                        root.reloadPreview()
                    }
                }
            }
        }
        onOpened: {
            confirmInput.text = ""
            confirmCheck.checked = false
            confirmInput.forceActiveFocus()
        }
    }
}
