// FileOperationsProgressOverlay.qml
//
// Progress indicator untuk file operations yang sedang berjalan.
// Menampilkan daftar operasi aktif dengan progress bar dan tombol cancel.
//
// Cara pakai:
//   FileOperationsProgressOverlay {
//       anchors.bottom: parent.bottom
//       anchors.right: parent.right
//   }

import QtQuick
import QtQuick.Controls

Item {
    id: root

    implicitWidth: 320
    implicitHeight: operationList.count > 0 ? operationColumn.implicitHeight + 16 : 0

    visible: operationList.count > 0

    // ── Internal model ────────────────────────────────────────────────────

    ListModel {
        id: operationList
    }

    // ── Wiring ke bridge ──────────────────────────────────────────────────

    Connections {
        target: typeof FileManagerShellBridge !== "undefined"
                ? FileManagerShellBridge : null
        enabled: target !== null

        function onOperationProgress(operationId, fraction, statusText) {
            const idx = findOperation(operationId)
            if (idx >= 0) {
                operationList.setProperty(idx, "fraction", fraction)
                operationList.setProperty(idx, "statusText", statusText)
            } else {
                operationList.append({
                    "operationId": operationId,
                    "fraction": fraction,
                    "statusText": statusText,
                    "done": false,
                    "success": true,
                    "error": ""
                })
            }
        }

        function onOperationFinished(operationId, success, error) {
            const idx = findOperation(operationId)
            if (idx >= 0) {
                operationList.setProperty(idx, "done", true)
                operationList.setProperty(idx, "success", success)
                operationList.setProperty(idx, "error", error)
                operationList.setProperty(idx, "fraction", 1.0)
                // Auto-hapus setelah 2 detik
                removeTimer.pendingId = operationId
                removeTimer.restart()
            }
        }
    }

    function findOperation(operationId) {
        for (let i = 0; i < operationList.count; i++) {
            if (operationList.get(i).operationId === operationId) return i
        }
        return -1
    }

    Timer {
        id: removeTimer
        property string pendingId: ""
        interval: 2000
        onTriggered: {
            const idx = root.findOperation(pendingId)
            if (idx >= 0) operationList.remove(idx)
        }
    }

    // ── UI ────────────────────────────────────────────────────────────────

    Rectangle {
        id: background
        anchors.fill: operationColumn
        anchors.margins: -8
        radius: 8
        color: Qt.rgba(0, 0, 0, 0.75)
    }

    Column {
        id: operationColumn
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            margins: 8
        }
        spacing: 6

        Repeater {
            model: operationList

            delegate: Item {
                width: parent.width
                height: opRow.implicitHeight + 4

                Row {
                    id: opRow
                    width: parent.width
                    spacing: 6

                    Column {
                        width: parent.width - cancelBtn.width - 6
                        spacing: 3

                        Text {
                            width: parent.width
                            text: model.statusText || (model.done
                                ? (model.success ? qsTr("Selesai") : qsTr("Gagal"))
                                : qsTr("Memproses..."))
                            color: model.done && !model.success ? "#ff6b6b" : "#ffffff"
                            font.pixelSize: 11
                            elide: Text.ElideMiddle
                        }

                        Rectangle {
                            width: parent.width
                            height: 4
                            radius: 2
                            color: "#444"

                            Rectangle {
                                width: parent.width * Math.min(model.fraction, 1.0)
                                height: parent.height
                                radius: parent.radius
                                color: model.done && !model.success
                                    ? "#ff6b6b"
                                    : (model.done ? "#4caf50" : "#2196F3")

                                Behavior on width {
                                    NumberAnimation { duration: 150 }
                                }
                            }
                        }
                    }

                    // Cancel button — tersedia hanya saat operasi belum selesai
                    Item {
                        id: cancelBtn
                        width: model.done ? 0 : 20
                        height: 20
                        visible: !model.done

                        Accessible.role: Accessible.Button
                        Accessible.name: qsTr("Batalkan operasi ini")

                        Rectangle {
                            anchors.fill: parent
                            radius: 3
                            color: cancelArea.containsMouse ? "#555" : "transparent"
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "✕"
                            color: "#ccc"
                            font.pixelSize: 10
                        }

                        MouseArea {
                            id: cancelArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                if (FileManagerShellBridge)
                                    FileManagerShellBridge.cancelOperation(model.operationId)
                            }
                        }
                    }
                }
            }
        }
    }
}
