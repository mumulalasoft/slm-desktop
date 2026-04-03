import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../../../../../Qml/apps/settings/components"
import Slm_Desktop

// PerAppOverridesView — Settings tab for per-application environment overrides.
//
// Context properties used:
//   PerAppOverrides  — PerAppOverridesController*
//   EnvServiceClient — EnvServiceClient*

Flickable {
    id: root
    anchors.fill: parent
    contentHeight: mainCol.implicitHeight + 48
    clip: true

    AppPickerDialog { id: appPicker }

    // ── Delete confirmation ────────────────────────────────────────────────
    property string pendingDeleteKey: ""

    Popup {
        id: deleteConfirm
        modal: true
        anchors.centerIn: Overlay.overlay
        padding: 20
        width: 280

        background: Rectangle {
            radius: Theme.radiusCard
            color: Theme.color("surface")
            border.color: Theme.color("panelBorder"); border.width: 1
        }

        ColumnLayout {
            width: parent.width - 40
            spacing: 12

            Text {
                text: qsTr("Remove override?")
                font.pixelSize: Theme.fontSize("body"); font.weight: Font.DemiBold
                color: Theme.color("textPrimary")
            }
            Text {
                text: qsTr("Remove <b>%1</b> override for <b>%2</b>?")
                        .arg(root.pendingDeleteKey)
                        .arg(PerAppOverrides.selectedAppId)
                font.pixelSize: Theme.fontSize("sm")
                color: Theme.color("textSecondary")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                textFormat: Text.RichText
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("Cancel"); implicitHeight: 32
                    onClicked: deleteConfirm.close()
                    background: Rectangle {
                        radius: Theme.radiusControl; color: "transparent"
                        border.color: Theme.color("panelBorder"); border.width: 1
                    }
                    contentItem: Text {
                        text: parent.text; font.pixelSize: Theme.fontSize("sm")
                        color: Theme.color("textPrimary")
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                }
                Button {
                    text: qsTr("Remove"); implicitHeight: 32
                    onClicked: {
                        PerAppOverrides.removeVar(root.pendingDeleteKey)
                        deleteConfirm.close()
                    }
                    background: Rectangle {
                        radius: Theme.radiusControl
                        color: parent.hovered ? Qt.darker(Theme.color("destructive"), 1.1) : Theme.color("destructive")
                    }
                    contentItem: Text {
                        text: parent.text; font.pixelSize: Theme.fontSize("sm"); font.weight: Font.Medium
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }

    // ── Add override dialog (reuse AddEditEnvVarDialog) ───────────────────
    AddEditEnvVarDialog {
        id: addDialog
        onAccepted: PerAppOverrides.addVar(resultKey, resultValue, resultMergeMode)
    }

    ColumnLayout {
        id: mainCol
        anchors { left: parent.left; right: parent.right; top: parent.top }
        anchors.margins: 24
        anchors.topMargin: 24
        spacing: 20

        // Service-offline banner
        Rectangle {
            visible: !EnvServiceClient.serviceAvailable
            Layout.fillWidth: true
            implicitHeight: offRow.implicitHeight + 12
            color: Qt.rgba(1.0, 0.5, 0.0, 0.08)
            radius: Theme.radiusCard
            border.color: Qt.rgba(1.0, 0.5, 0.0, 0.25); border.width: 1

            RowLayout {
                id: offRow
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                anchors.margins: 10; spacing: 8
                Text { text: "⚠"; font.pixelSize: Theme.fontSize("sm"); color: Theme.color("warning") }
                Text {
                    text: qsTr("Environment service is not running. Per-app overrides are unavailable.")
                    font.pixelSize: Theme.fontSize("sm"); color: Theme.color("textSecondary")
                    wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
            }
        }

        // Info banner
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: infoRow.implicitHeight + 12
            color: Qt.rgba(0.15, 0.5, 1.0, 0.08)
            radius: Theme.radiusCard
            border.color: Qt.rgba(0.15, 0.5, 1.0, 0.2); border.width: 1

            RowLayout {
                id: infoRow
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                anchors.margins: 10; spacing: 8
                Text { text: "ℹ"; font.pixelSize: Theme.fontSize("sm"); color: Theme.color("accent") }
                Text {
                    text: qsTr("Per-app overrides are applied only when the selected app is launched. " +
                                "They take priority over user-persistent and session variables.")
                    font.pixelSize: Theme.fontSize("sm"); color: Theme.color("textSecondary")
                    wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
            }
        }

        // App selector row
        RowLayout {
            Layout.fillWidth: true; spacing: 12

            Text {
                text: PerAppOverrides.selectedAppId.length > 0
                      ? PerAppOverrides.selectedAppId
                      : qsTr("No application selected")
                font.pixelSize: Theme.fontSize("body")
                font.family: PerAppOverrides.selectedAppId.length > 0 ? "monospace" : ""
                font.weight: Font.DemiBold
                color: PerAppOverrides.selectedAppId.length > 0
                       ? Theme.color("textPrimary")
                       : Theme.color("textDisabled")
                Layout.fillWidth: true
            }

            Button {
                text: qsTr("Change App…")
                implicitHeight: 32
                enabled: EnvServiceClient.serviceAvailable
                onClicked: {
                    appPicker.selectedAppId = PerAppOverrides.selectedAppId
                    appPicker.open()
                }
                background: Rectangle {
                    radius: Theme.radiusControl
                    color: parent.hovered ? Theme.color("hoverOverlay") : "transparent"
                    border.color: Theme.color("panelBorder"); border.width: 1
                }
                contentItem: Text {
                    text: parent.text; font.pixelSize: Theme.fontSize("sm")
                    color: parent.enabled ? Theme.color("textPrimary") : Theme.color("textDisabled")
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
            }
        }

        Connections {
            target: appPicker
            function onAccepted(appId) { PerAppOverrides.selectedAppId = appId }
        }

        // Variable list for selected app
        SettingGroup {
            title: PerAppOverrides.selectedAppId.length > 0
                   ? qsTr("Overrides for %1").arg(PerAppOverrides.selectedAppId)
                   : qsTr("Overrides")
            Layout.fillWidth: true
            enabled: PerAppOverrides.selectedAppId.length > 0 && EnvServiceClient.serviceAvailable

            // Header
            Item {
                implicitHeight: 32; Layout.fillWidth: true
                RowLayout {
                    anchors { fill: parent; leftMargin: 12; rightMargin: 12; spacing: 8}
                    Text {
                        text: qsTr("KEY"); font.pixelSize: Theme.fontSize("xs"); font.weight: Font.DemiBold
                        font.letterSpacing: 0.5; color: Theme.color("textDisabled")
                        Layout.preferredWidth: 200
                    }
                    Text {
                        text: qsTr("VALUE"); font.pixelSize: Theme.fontSize("xs"); font.weight: Font.DemiBold
                        font.letterSpacing: 0.5; color: Theme.color("textDisabled")
                        Layout.fillWidth: true
                    }
                    Text {
                        text: qsTr("MODE"); font.pixelSize: Theme.fontSize("xs"); font.weight: Font.DemiBold
                        font.letterSpacing: 0.5; color: Theme.color("textDisabled")
                        Layout.preferredWidth: 60; horizontalAlignment: Text.AlignRight
                    }
                    Item { Layout.preferredWidth: 36 }
                }
                Rectangle {
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                    height: 1; color: Theme.color("panelBorder")
                }
            }

            Repeater {
                model: PerAppOverrides.appVars

                delegate: Item {
                    required property var modelData
                    implicitHeight: 44; Layout.fillWidth: true

                    HoverHandler { id: rowHover }

                    RowLayout {
                        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                        anchors.leftMargin: 12; anchors.rightMargin: 12; spacing: 8

                        Text {
                            text: modelData.key
                            font.family: "monospace"; font.pixelSize: Theme.fontSize("sm"); font.weight: Font.Medium
                            color: Theme.color("textPrimary")
                            Layout.preferredWidth: 200; elide: Text.ElideRight
                        }
                        Text {
                            text: modelData.value
                            font.family: "monospace"; font.pixelSize: Theme.fontSize("sm")
                            color: Theme.color("textSecondary")
                            Layout.fillWidth: true; elide: Text.ElideRight
                        }
                        Text {
                            text: modelData.mergeMode || "replace"
                            font.pixelSize: Theme.fontSize("xs"); color: Theme.color("accent")
                            Layout.preferredWidth: 60; horizontalAlignment: Text.AlignRight
                        }
                        // Delete button
                        Button {
                            implicitWidth: 28; implicitHeight: 28
                            visible: rowHover.hovered
                            onClicked: {
                                root.pendingDeleteKey = modelData.key
                                deleteConfirm.open()
                            }
                            background: Rectangle {
                                radius: Theme.radiusControl
                                color: parent.hovered ? Qt.rgba(1,0,0,0.12) : "transparent"
                            }
                            contentItem: Text {
                                text: "✕"; font.pixelSize: 11; font.weight: Font.Medium
                                color: Theme.color("destructive")
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }

                    Rectangle {
                        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                        height: 1; color: Theme.color("panelBorder"); opacity: 0.4
                    }
                }
            }

            // Empty state
            Item {
                visible: PerAppOverrides.appVars.length === 0 && PerAppOverrides.selectedAppId.length > 0
                implicitHeight: 56; Layout.fillWidth: true
                Text {
                    anchors.centerIn: parent
                    text: qsTr("No overrides for this application.")
                    font.pixelSize: Theme.fontSize("sm"); color: Theme.color("textDisabled")
                }
            }

            // No-app state
            Item {
                visible: PerAppOverrides.selectedAppId.length === 0
                implicitHeight: 56; Layout.fillWidth: true
                Text {
                    anchors.centerIn: parent
                    text: qsTr("Select an application above to manage its overrides.")
                    font.pixelSize: Theme.fontSize("sm"); color: Theme.color("textDisabled")
                }
            }
        }

        // Add override button
        Button {
            text: qsTr("+ Add Override")
            Layout.alignment: Qt.AlignRight; implicitHeight: 36
            enabled: PerAppOverrides.selectedAppId.length > 0 && EnvServiceClient.serviceAvailable
            onClicked: addDialog.openForAdd()
            background: Rectangle {
                radius: Theme.radiusControl
                color: parent.enabled
                       ? (parent.hovered ? Theme.color("accentHover") : Theme.color("accent"))
                       : Theme.color("panelBorder")
                Behavior on color { ColorAnimation { duration: 120 } }
            }
            contentItem: Text {
                text: parent.text; font.pixelSize: Theme.fontSize("sm"); font.weight: Font.Medium
                color: parent.enabled ? Theme.color("accentText") : Theme.color("textDisabled")
                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
            }
        }

        // Error
        Text {
            visible: PerAppOverrides.lastError.length > 0
            text: "⚠  " + PerAppOverrides.lastError
            font.pixelSize: Theme.fontSize("sm"); color: Theme.color("destructive")
            wrapMode: Text.WordWrap; Layout.fillWidth: true
        }
    }
}
