import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../../../../../Qml/apps/settings/components"
import Slm_Desktop

Flickable {
    id: root
    anchors.fill: parent
    contentHeight: mainCol.implicitHeight + 48
    clip: true

    // ── Confirmation dialog ───────────────────────────────────────────────
    property string pendingAction: ""   // "restart" | "stop"
    property string pendingName:   ""

    Dialog {
        id: confirmDialog
        modal: true
        anchors.centerIn: Overlay.overlay
        title: root.pendingAction === "restart" ? qsTr("Restart Component?") : qsTr("Stop Component?")
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: 320

        Text {
            width: parent.width
            wrapMode: Text.WordWrap
            font.pixelSize: Theme.fontSize("sm")
            color: Theme.color("textSecondary")
            text: root.pendingAction === "restart"
                  ? qsTr("Restart <b>%1</b>? The service will be briefly unavailable.").arg(root.pendingName)
                  : qsTr("Stop <b>%1</b>? It will remain stopped until manually restarted or next login.").arg(root.pendingName)
            textFormat: Text.RichText
        }

        onAccepted: {
            if (!ProcessServicesController) return
            if (root.pendingAction === "restart")
                ProcessServicesController.restart(root.pendingName)
            else
                ProcessServicesController.stop(root.pendingName)
        }
    }

    ColumnLayout {
        id: mainCol
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 20
        spacing: 16

        // ── Header ────────────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    text: qsTr("Process & Services")
                    font.pixelSize: Theme.fontSize("h2")
                    font.weight: Font.DemiBold
                    color: Theme.color("textPrimary")
                }
                Text {
                    text: qsTr("Shell component status. Only registered shell services are shown.")
                    font.pixelSize: Theme.fontSize("sm")
                    color: Theme.color("textSecondary")
                }
            }

            Button {
                text: qsTr("Refresh")
                onClicked: if (ProcessServicesController) ProcessServicesController.refresh()
            }
        }

        // ── Error banner ──────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 36
            visible: ProcessServicesController && ProcessServicesController.lastError.length > 0
            color: Qt.rgba(0.94, 0.2, 0.2, 0.08)
            radius: Theme.radiusCard
            border.color: Qt.rgba(0.94, 0.2, 0.2, 0.3)
            border.width: 1

            Text {
                anchors.centerIn: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                text: ProcessServicesController ? ProcessServicesController.lastError : ""
                font.pixelSize: Theme.fontSize("sm")
                color: "#dc2626"
                elide: Text.ElideRight
                width: parent.width - 24
            }
        }

        // ── Offline banner ────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 40
            visible: !ProcessServicesController || !ProcessServicesController.serviceAvailable
            color: Theme.color("warningBg") || Qt.rgba(0.96, 0.8, 0.2, 0.12)
            radius: Theme.radiusCard

            Text {
                anchors.centerIn: parent
                text: qsTr("Service manager (slm-svcmgrd) is offline — component status unavailable")
                font.pixelSize: Theme.fontSize("sm")
                color: Theme.color("textSecondary")
            }
        }

        // ── Component list ────────────────────────────────────────────────
        SettingGroup {
            Layout.fillWidth: true
            label: qsTr("Registered Components")
            visible: ProcessServicesController && ProcessServicesController.serviceAvailable

            Column {
                width: parent.width
                spacing: 1

                Repeater {
                    model: ProcessServicesController ? ProcessServicesController.components : []
                    delegate: Rectangle {
                        width: parent.width
                        height: componentRow.implicitHeight + 16
                        color: "transparent"

                        RowLayout {
                            id: componentRow
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 10

                            // Status dot
                            Rectangle {
                                width: 8; height: 8; radius: 4
                                color: {
                                    switch (modelData.status) {
                                    case "active":      return Theme.color("success") || "#22c55e"
                                    case "failed":      return Theme.color("error")   || "#ef4444"
                                    case "activating":  return Theme.color("warning") || "#f59e0b"
                                    default:            return Theme.color("textDisabled")
                                    }
                                }
                            }

                            // Name + unit
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1
                                Text {
                                    text: modelData.displayName || modelData.name
                                    font.pixelSize: Theme.fontSize("body")
                                    font.weight: Font.Medium
                                    color: Theme.color("textPrimary")
                                }
                                Text {
                                    text: modelData.unitName
                                    font.pixelSize: Theme.fontSize("xs")
                                    font.family: "monospace"
                                    color: Theme.color("textSecondary")
                                }
                            }

                            // PID
                            ColumnLayout {
                                spacing: 0
                                visible: modelData.pid > 0
                                Text {
                                    text: qsTr("PID")
                                    font.pixelSize: Theme.fontSize("xs")
                                    color: Theme.color("textDisabled")
                                    horizontalAlignment: Text.AlignHCenter
                                }
                                Text {
                                    text: modelData.pid > 0 ? modelData.pid : "—"
                                    font.pixelSize: Theme.fontSize("sm")
                                    font.family: "monospace"
                                    color: Theme.color("textSecondary")
                                    horizontalAlignment: Text.AlignHCenter
                                }
                            }

                            // Memory
                            ColumnLayout {
                                spacing: 0
                                visible: modelData.memKb > 0
                                Text {
                                    text: qsTr("MEM")
                                    font.pixelSize: Theme.fontSize("xs")
                                    color: Theme.color("textDisabled")
                                    horizontalAlignment: Text.AlignHCenter
                                }
                                Text {
                                    text: modelData.memKb > 0
                                          ? (modelData.memKb < 1024
                                             ? modelData.memKb + " KB"
                                             : Math.round(modelData.memKb / 1024) + " MB")
                                          : "—"
                                    font.pixelSize: Theme.fontSize("sm")
                                    font.family: "monospace"
                                    color: Theme.color("textSecondary")
                                    horizontalAlignment: Text.AlignHCenter
                                }
                            }

                            // Status label
                            Rectangle {
                                width: 80; height: 20; radius: 3
                                color: {
                                    switch (modelData.status) {
                                    case "active":      return Qt.rgba(0.13, 0.77, 0.37, 0.12)
                                    case "failed":      return Qt.rgba(0.94, 0.2,  0.2,  0.12)
                                    case "activating":  return Qt.rgba(0.96, 0.7,  0.1,  0.12)
                                    default:            return Qt.rgba(0.5,  0.5,  0.5,  0.08)
                                    }
                                }
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.status || "unknown"
                                    font.pixelSize: Theme.fontSize("xs")
                                    color: Theme.color("textSecondary")
                                }
                            }

                            // Actions
                            Row {
                                spacing: 4

                                Button {
                                    text: qsTr("Restart")
                                    implicitHeight: 28
                                    enabled: modelData.status !== "activating"
                                    onClicked: {
                                        root.pendingAction = "restart"
                                        root.pendingName   = modelData.name
                                        confirmDialog.open()
                                    }
                                }

                                Button {
                                    text: qsTr("Stop")
                                    implicitHeight: 28
                                    enabled: modelData.status === "active"
                                    onClicked: {
                                        root.pendingAction = "stop"
                                        root.pendingName   = modelData.name
                                        confirmDialog.open()
                                    }
                                }
                            }
                        }

                        Rectangle {
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
                            anchors.right: parent.right
                            height: 1
                            color: Theme.color("panelBorder")
                            opacity: 0.5
                        }
                    }
                }
            }
        }
    }
}
