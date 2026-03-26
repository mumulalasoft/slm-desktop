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

    ColumnLayout {
        id: mainCol
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 20
        spacing: 16

        Text {
            text: qsTr("Developer Overview")
            font.pixelSize: Theme.fontSize("h2")
            font.weight: Font.DemiBold
            color: Theme.color("textPrimary")
        }

        Text {
            text: qsTr("System status — all registered shell components and services.")
            font.pixelSize: Theme.fontSize("sm")
            color: Theme.color("textSecondary")
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        // ── Service status grid ──────────────────────────────────────────
        SettingGroup {
            Layout.fillWidth: true
            label: qsTr("Shell Components")

            Column {
                width: parent.width
                spacing: 1

                Repeater {
                    model: ProcessServicesController ? ProcessServicesController.components : []
                    delegate: SettingCard {
                        width: parent.width
                        label: modelData.displayName || modelData.name
                        description: modelData.description || modelData.unitName

                        control: RowLayout {
                            spacing: 8

                            Rectangle {
                                width: 8; height: 8
                                radius: 4
                                color: {
                                    switch (modelData.status) {
                                    case "active":      return Theme.color("success") || "#22c55e"
                                    case "failed":      return Theme.color("error")   || "#ef4444"
                                    case "activating":  return Theme.color("warning") || "#f59e0b"
                                    default:            return Theme.color("textDisabled")
                                    }
                                }
                            }
                            Text {
                                text: modelData.status || "unknown"
                                font.pixelSize: Theme.fontSize("xs")
                                color: Theme.color("textSecondary")
                            }
                        }
                    }
                }

                // Offline banner when svcmgr is not available
                Rectangle {
                    width: parent.width
                    height: 40
                    visible: !ProcessServicesController || !ProcessServicesController.serviceAvailable
                    color: Theme.color("warningBg") || Qt.rgba(0.96, 0.8, 0.2, 0.12)
                    radius: Theme.radiusCard

                    Text {
                        anchors.centerIn: parent
                        text: qsTr("Service manager offline — status unavailable")
                        font.pixelSize: Theme.fontSize("sm")
                        color: Theme.color("textSecondary")
                    }
                }
            }
        }

        // ── Environment service status ────────────────────────────────────
        SettingGroup {
            Layout.fillWidth: true
            label: qsTr("Environment")

            SettingCard {
                width: parent.width
                label: qsTr("Environment Service")
                description: qsTr("org.slm.Environment1")

                control: Row {
                    spacing: 6
                    Rectangle {
                        width: 8; height: 8; radius: 4
                        anchors.verticalCenter: parent.verticalCenter
                        color: EnvServiceClient && EnvServiceClient.serviceAvailable
                               ? (Theme.color("success") || "#22c55e")
                               : Theme.color("textDisabled")
                    }
                    Text {
                        text: EnvServiceClient && EnvServiceClient.serviceAvailable
                              ? qsTr("online") : qsTr("offline")
                        font.pixelSize: Theme.fontSize("xs")
                        color: Theme.color("textSecondary")
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            SettingCard {
                width: parent.width
                label: qsTr("Logger Service")
                description: qsTr("org.slm.Logger1")

                control: Row {
                    spacing: 6
                    Rectangle {
                        width: 8; height: 8; radius: 4
                        anchors.verticalCenter: parent.verticalCenter
                        color: LogsController && LogsController.serviceAvailable
                               ? (Theme.color("success") || "#22c55e")
                               : Theme.color("textDisabled")
                    }
                    Text {
                        text: LogsController && LogsController.serviceAvailable
                              ? qsTr("online") : qsTr("offline")
                        font.pixelSize: Theme.fontSize("xs")
                        color: Theme.color("textSecondary")
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }

        // ── Quick actions ─────────────────────────────────────────────────
        SettingGroup {
            Layout.fillWidth: true
            label: qsTr("Quick Actions")

            Row {
                spacing: 8
                leftPadding: 12
                bottomPadding: 12

                Button {
                    text: qsTr("Refresh Status")
                    onClicked: if (ProcessServicesController) ProcessServicesController.refresh()
                }

                Button {
                    text: qsTr("View Logs")
                    onClicked: {
                        // Navigate to logs page via parent DeveloperPage
                        const devPage = root.parent
                        if (devPage && devPage.currentPageId !== undefined)
                            devPage.currentPageId = "logs"
                    }
                }
            }
        }
    }
}
