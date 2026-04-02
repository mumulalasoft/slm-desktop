import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Item {
    id: root

    property string statusMessage: ""
    property bool   statusOk:      true

    Dialog {
        id: factoryResetConfirmDialog
        title: qsTr("Konfirmasi Factory Reset")
        modal: true
        anchors.centerIn: parent
        width: 400

        contentItem: Text {
            text: qsTr("Ini akan menghapus SEMUA konfigurasi desktop (aktif, sebelumnya, "
                       + "safe baseline, dan semua snapshot), lalu menulis ulang default minimal.\n\n"
                       + "Data pengguna tidak akan terpengaruh. Lanjutkan?")
            color: "#ddd"
            wrapMode: Text.WordWrap
            padding: 8
        }

        background: Rectangle { color: "#2a2a2a"; radius: 6; border.color: "#555" }

        standardButtons: Dialog.Ok | Dialog.Cancel

        onAccepted: {
            const ok = RecoveryApp.factoryReset()
            root.statusOk = ok
            root.statusMessage = ok
                ? qsTr("Factory reset selesai. Exit untuk memulai ulang desktop.")
                : qsTr("Factory reset gagal. Periksa izin file.")
        }
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: Math.min(parent.width, 640)
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 16
            anchors.top: parent.top
            anchors.topMargin: 32

            Text {
                text: qsTr("Reset Configuration")
                color: "white"
                font.pixelSize: 18
                font.bold: true
            }

            Text {
                text: qsTr("These actions modify the desktop configuration files without restarting "
                           + "the session. Click \"Exit to Desktop\" in the footer after applying.")
                color: "#aaa"
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#444" }

            // Reset to safe defaults
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: qsTr("Reset to Safe Defaults")
                    color: "white"
                    font.pixelSize: 14
                    font.bold: true
                }

                Text {
                    text: qsTr("Restores the last known-safe configuration baseline "
                               + "(config.safe.json). Reverts theme, layout, and compositor "
                               + "settings to a stable state.")
                    color: "#bbb"
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                Button {
                    text: qsTr("Reset to Safe Defaults")
                    onClicked: {
                        const ok = RecoveryApp.resetToSafeDefaults()
                        root.statusOk = ok
                        root.statusMessage = ok
                            ? qsTr("Safe defaults applied. Exit to restart the desktop.")
                            : qsTr("Failed to apply safe defaults. Check permissions.")
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#444" }

            // Rollback to previous
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                enabled: RecoveryApp.hasPreviousConfig

                Text {
                    text: qsTr("Rollback to Previous Configuration")
                    color: parent.enabled ? "white" : "#666"
                    font.pixelSize: 14
                    font.bold: true
                }

                Text {
                    text: RecoveryApp.hasPreviousConfig
                        ? qsTr("Restores the configuration that was active before the last change "
                               + "(config.prev.json).")
                        : qsTr("No previous configuration available.")
                    color: parent.enabled ? "#bbb" : "#555"
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                Button {
                    text: qsTr("Rollback to Previous")
                    enabled: RecoveryApp.hasPreviousConfig
                    onClicked: {
                        const ok = RecoveryApp.rollbackToPrevious()
                        root.statusOk = ok
                        root.statusMessage = ok
                            ? qsTr("Rollback applied. Exit to restart the desktop.")
                            : qsTr("Failed to rollback. Check permissions.")
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#444" }

            // Factory reset
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: qsTr("Factory Reset Konfigurasi")
                    color: "#ff8a80"
                    font.pixelSize: 14
                    font.bold: true
                }

                Text {
                    text: qsTr("Hapus semua file konfigurasi (aktif, sebelumnya, safe baseline, "
                               + "dan semua snapshot) lalu tulis ulang default minimal. "
                               + "Data pengguna tidak terpengaruh.")
                    color: "#bbb"
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                Button {
                    text: qsTr("Factory Reset…")
                    palette.button: "#7b1a1a"
                    palette.buttonText: "#ffdddd"
                    onClicked: factoryResetConfirmDialog.open()
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#444" }

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

            Item { height: 24 }
        }
    }
}
