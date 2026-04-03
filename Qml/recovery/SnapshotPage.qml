import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property string statusMessage: ""
    property bool   statusOk:      true

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

        // Empty state
        Text {
            visible: snapshotList.count === 0
            text: qsTr("No snapshots available.")
            color: "#666"
            font.pixelSize: 13
            Layout.alignment: Qt.AlignHCenter
        }

        // Snapshot list
        ListView {
            id: snapshotList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: RecoveryApp.availableSnapshots
            spacing: 6

            delegate: Rectangle {
                width: ListView.view.width
                height: 52
                color: mouseArea.containsMouse ? "#333" : "#2a2a2a"
                radius: 4
                border.color: "#444"
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16

                    Text {
                        text: modelData
                        color: "white"
                        font.pixelSize: 13
                        font.family: "monospace"
                        Layout.fillWidth: true
                    }

                    Button {
                        text: qsTr("Restore")
                        onClicked: {
                            const ok = RecoveryApp.restoreSnapshot(modelData)
                            root.statusOk = ok
                            root.statusMessage = ok
                                ? qsTr("Snapshot \"%1\" restored. Exit to restart the desktop.").arg(modelData)
                                : qsTr("Failed to restore snapshot \"%1\".").arg(modelData)
                        }
                    }
                }

                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    // Clicks handled by the Button child.
                    onClicked: {}
                }
            }
        }

        // Status message
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
}
