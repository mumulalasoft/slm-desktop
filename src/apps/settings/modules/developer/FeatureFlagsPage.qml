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

        // ── Header ────────────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    text: qsTr("Feature Flags")
                    font.pixelSize: Theme.fontSize("h2")
                    font.weight: Font.DemiBold
                    color: Theme.color("textPrimary")
                }
                Text {
                    text: qsTr("Preview and experimental features. Changes apply as indicated below each flag.")
                    font.pixelSize: Theme.fontSize("sm")
                    color: Theme.color("textSecondary")
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
            }

            Button {
                text: qsTr("Reset All")
                onClicked: resetConfirmDialog.open()
                ToolTip.text: qsTr("Disable all feature flags and restore defaults")
                ToolTip.visible: hovered
                ToolTip.delay: 600
            }
        }

        // ── Reset confirmation ────────────────────────────────────────────
        Dialog {
            id: resetConfirmDialog
            modal: true
            anchors.centerIn: Overlay.overlay
            title: qsTr("Reset All Feature Flags?")
            standardButtons: Dialog.Ok | Dialog.Cancel
            width: 320

            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSize("sm")
                color: Theme.color("textSecondary")
                text: qsTr("All feature flags will be disabled and reset to their default state. Some changes may require a service restart.")
            }

            onAccepted: {
                if (FeatureFlags) FeatureFlags.resetAll()
            }
        }

        // ── Flag list ─────────────────────────────────────────────────────
        SettingGroup {
            Layout.fillWidth: true
            label: qsTr("Available Flags")

            Column {
                width: parent.width
                spacing: 0

                Repeater {
                    model: FeatureFlags ? FeatureFlags.flags : []

                    delegate: Rectangle {
                        width: parent.width
                        height: flagRow.implicitHeight + 20
                        color: "transparent"

                        RowLayout {
                            id: flagRow
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 12

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3

                                RowLayout {
                                    spacing: 6

                                    Text {
                                        text: modelData.name
                                        font.pixelSize: Theme.fontSize("body")
                                        font.weight: Font.Medium
                                        color: Theme.color("textPrimary")
                                    }

                                    // Badge
                                    Rectangle {
                                        height: 16
                                        width: badgeText.implicitWidth + 10
                                        radius: 3
                                        color: {
                                            switch (modelData.badge) {
                                            case "experimental": return Qt.rgba(0.96, 0.7, 0.1, 0.15)
                                            case "deprecated":   return Qt.rgba(0.94, 0.2, 0.2, 0.12)
                                            default:             return Qt.rgba(0.2, 0.6, 0.9, 0.12)
                                            }
                                        }
                                        Text {
                                            id: badgeText
                                            anchors.centerIn: parent
                                            text: modelData.badge || "preview"
                                            font.pixelSize: Theme.fontSize("xs")
                                            color: {
                                                switch (modelData.badge) {
                                                case "experimental": return "#d97706"
                                                case "deprecated":   return "#dc2626"
                                                default:             return Theme.color("textSecondary")
                                                }
                                            }
                                        }
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.description
                                    font.pixelSize: Theme.fontSize("sm")
                                    color: Theme.color("textSecondary")
                                    wrapMode: Text.WordWrap
                                }

                                Text {
                                    text: {
                                        switch (modelData.effectWhen) {
                                        case "immediately":          return qsTr("Takes effect immediately")
                                        case "next-launch":          return qsTr("Takes effect on next app launch")
                                        case "restart-compositor":   return qsTr("Requires compositor restart")
                                        case "restart-session":      return qsTr("Requires session restart")
                                        default:                     return modelData.effectWhen
                                        }
                                    }
                                    font.pixelSize: Theme.fontSize("xs")
                                    color: Theme.color("textDisabled")
                                    font.italic: true
                                }
                            }

                            Switch {
                                checked: modelData.enabled
                                onToggled: {
                                    if (FeatureFlags)
                                        FeatureFlags.setEnabled(modelData.id, checked)
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

                // Empty state
                Rectangle {
                    width: parent.width
                    height: 48
                    visible: !FeatureFlags || FeatureFlags.flags.length === 0
                    color: "transparent"

                    Text {
                        anchors.centerIn: parent
                        text: qsTr("No feature flags available.")
                        font.pixelSize: Theme.fontSize("sm")
                        color: Theme.color("textDisabled")
                    }
                }
            }
        }
    }
}
