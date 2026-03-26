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

    // ── Delete confirmation popup ──────────────────────────────────────────
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
            border.color: Theme.color("panelBorder")
            border.width: 1
        }

        ColumnLayout {
            width: parent.width - 40
            spacing: 12

            Text {
                text: qsTr("Delete variable?")
                font.pixelSize: Theme.fontSize("body")
                font.weight: Font.DemiBold
                color: Theme.color("textPrimary")
            }
            Text {
                text: qsTr("Remove <b>%1</b>? Running apps are not affected.").arg(root.pendingDeleteKey)
                font.pixelSize: Theme.fontSize("sm")
                color: Theme.color("textSecondary")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                textFormat: Text.RichText
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("Cancel")
                    implicitHeight: 32
                    onClicked: deleteConfirm.close()
                    background: Rectangle {
                        radius: Theme.radiusControl
                        color: parent.hovered ? Theme.color("hoverOverlay") : "transparent"
                        border.color: Theme.color("panelBorder"); border.width: 1
                    }
                    contentItem: Text {
                        text: parent.text; font.pixelSize: Theme.fontSize("sm")
                        color: Theme.color("textPrimary")
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                }
                Button {
                    text: qsTr("Delete")
                    implicitHeight: 32
                    onClicked: {
                        EnvVarController.deleteEntry(root.pendingDeleteKey)
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

    // ── Add / Edit dialog ─────────────────────────────────────────────────
    AddEditEnvVarDialog {
        id: addEditDialog
        onAccepted: {
            if (isEdit) {
                EnvVarController.updateEntry(resultKey, resultValue, resultComment, resultMergeMode)
            } else {
                EnvVarController.addEntry(resultKey, resultValue, resultComment, resultMergeMode)
            }
        }
    }

    // ── Main layout ───────────────────────────────────────────────────────
    ColumnLayout {
        id: mainCol
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 24
        anchors.top: parent.top
        anchors.topMargin: 24
        spacing: 20

        // Change-effect banner
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: bannerRow.implicitHeight + 16
            color: Qt.rgba(0.15, 0.5, 1.0, 0.08)
            radius: Theme.radiusCard
            border.color: Qt.rgba(0.15, 0.5, 1.0, 0.2)
            border.width: 1

            RowLayout {
                id: bannerRow
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                anchors.margins: 12
                spacing: 8

                Text {
                    text: "ℹ"
                    font.pixelSize: Theme.fontSize("sm")
                    color: Theme.color("accent")
                }
                Text {
                    text: qsTr("Changes take effect for applications opened after saving. Running apps are not affected.")
                    font.pixelSize: Theme.fontSize("sm")
                    color: Theme.color("textSecondary")
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }

        // Search bar
        TextField {
            id: searchField
            Layout.fillWidth: true
            placeholderText: qsTr("Search variables…")
            font.pixelSize: Theme.fontSize("sm")
            leftPadding: 32

            Text {
                text: "🔍"
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 10
                font.pixelSize: 12
                color: Theme.color("textDisabled")
            }

            background: Rectangle {
                radius: Theme.radiusControl
                color: Theme.color("surface")
                border.color: parent.activeFocus ? Theme.color("accent") : Theme.color("panelBorder")
                border.width: parent.activeFocus ? 2 : 1
                Behavior on border.color { ColorAnimation { duration: 120 } }
            }
        }

        // Variable list
        SettingGroup {
            title: qsTr("User Variables")
            Layout.fillWidth: true

            // Table header
            Item {
                implicitHeight: 32
                Layout.fillWidth: true

                RowLayout {
                    anchors { fill: parent; leftMargin: 54; rightMargin: 12 }
                    spacing: 8

                    Text {
                        text: qsTr("KEY")
                        font.pixelSize: Theme.fontSize("xs")
                        font.weight: Font.DemiBold
                        font.letterSpacing: 0.5
                        color: Theme.color("textDisabled")
                        Layout.preferredWidth: 200
                    }
                    Text {
                        text: qsTr("VALUE")
                        font.pixelSize: Theme.fontSize("xs")
                        font.weight: Font.DemiBold
                        font.letterSpacing: 0.5
                        color: Theme.color("textDisabled")
                        Layout.fillWidth: true
                    }
                    Text {
                        text: qsTr("SCOPE")
                        font.pixelSize: Theme.fontSize("xs")
                        font.weight: Font.DemiBold
                        font.letterSpacing: 0.5
                        color: Theme.color("textDisabled")
                        Layout.preferredWidth: 50
                    }
                    Text {
                        text: qsTr("MODIFIED")
                        font.pixelSize: Theme.fontSize("xs")
                        font.weight: Font.DemiBold
                        font.letterSpacing: 0.5
                        color: Theme.color("textDisabled")
                        Layout.preferredWidth: 70
                        horizontalAlignment: Text.AlignRight
                    }
                    Item { Layout.preferredWidth: 60 }
                }

                // Bottom border for header
                Rectangle {
                    anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                    height: 1; color: Theme.color("panelBorder")
                }
            }

            // Rows
            Repeater {
                model: EnvVarController.model

                delegate: EnvVarRow {
                    entryKey:        model.key
                    entryValue:      model.value
                    entryEnabled:    model.enabled
                    entrySensitive:  model.sensitive
                    entrySeverity:   model.severity
                    entryModifiedAt: model.modifiedAt
                    entryComment:    model.comment

                    visible: searchField.text === ""
                          || entryKey.toLowerCase().includes(searchField.text.toLowerCase())
                          || entryValue.toLowerCase().includes(searchField.text.toLowerCase())

                    Layout.fillWidth: true

                    onEditRequested: {
                        addEditDialog.openForEdit(entryKey, entryValue, entryComment, model.mergeMode)
                    }
                    onDeleteRequested: {
                        root.pendingDeleteKey = entryKey
                        deleteConfirm.open()
                    }
                }
            }

            // Empty state
            Item {
                visible: EnvVarController.model.count === 0
                implicitHeight: emptyCol.implicitHeight + 32
                Layout.fillWidth: true

                ColumnLayout {
                    id: emptyCol
                    anchors.centerIn: parent
                    spacing: 8

                    Text {
                        text: "󰙬"
                        font.pixelSize: 32
                        color: Theme.color("textDisabled")
                        Layout.alignment: Qt.AlignHCenter
                    }
                    Text {
                        text: qsTr("No user variables defined.")
                        font.pixelSize: Theme.fontSize("body")
                        color: Theme.color("textSecondary")
                        Layout.alignment: Qt.AlignHCenter
                    }
                    Text {
                        text: qsTr("Variables you add here will be available\nto all applications you launch.")
                        font.pixelSize: Theme.fontSize("sm")
                        color: Theme.color("textDisabled")
                        horizontalAlignment: Text.AlignHCenter
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }
        }

        // Add Variable button
        Button {
            text: qsTr("+ Add Variable")
            Layout.alignment: Qt.AlignRight
            implicitHeight: 36
            onClicked: addEditDialog.openForAdd()
            background: Rectangle {
                radius: Theme.radiusControl
                color: parent.hovered ? Theme.color("accentHover") : Theme.color("accent")
                Behavior on color { ColorAnimation { duration: 120 } }
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: Theme.fontSize("sm")
                font.weight: Font.Medium
                color: Theme.color("accentText")
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        // Error message (from controller)
        Text {
            visible: EnvVarController.lastError !== ""
            text: "⚠  " + EnvVarController.lastError
            font.pixelSize: Theme.fontSize("sm")
            color: Theme.color("destructive")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // ── System Variables (gated by env-system-scope feature flag) ──────
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 12
            visible: FeatureFlags && FeatureFlags.isEnabled("env-system-scope")

            // Section divider
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Rectangle { height: 1; Layout.fillWidth: true; color: Theme.color("panelBorder") }
                Text {
                    text: qsTr("SYSTEM SCOPE")
                    font.pixelSize: Theme.fontSize("xs")
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.5
                    color: Theme.color("textDisabled")
                }
                Rectangle { height: 1; Layout.fillWidth: true; color: Theme.color("panelBorder") }
            }

            Text {
                text: qsTr("System variables apply to all users. Requires administrator authentication.")
                font.pixelSize: Theme.fontSize("sm")
                color: Theme.color("textSecondary")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            // Authorization gate — shows lock or spinner until authorized
            Rectangle {
                Layout.fillWidth: true
                height: authRow.implicitHeight + 20
                radius: Theme.radiusCard || 6
                color: Theme.color("surfaceAlt") || Theme.color("surface")
                border.color: Theme.color("panelBorder")
                border.width: 1
                visible: SystemEnvController
                      && (!SystemEnvController.authorized || SystemEnvController.authPending)

                RowLayout {
                    id: authRow
                    anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                    anchors.margins: 14
                    spacing: 10

                    BusyIndicator {
                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 20
                        running: SystemEnvController && SystemEnvController.authPending
                        visible: running
                    }

                    Text {
                        Layout.fillWidth: true
                        text: (SystemEnvController && SystemEnvController.authPending)
                            ? qsTr("Waiting for authentication…")
                            : qsTr("Requires administrator authentication to modify system variables.")
                        font.pixelSize: Theme.fontSize("sm")
                        color: Theme.color("textSecondary")
                        wrapMode: Text.WordWrap
                    }

                    Button {
                        text: qsTr("Unlock")
                        visible: SystemEnvController && !SystemEnvController.authorized
                              && !SystemEnvController.authPending
                        onClicked: if (SystemEnvController) SystemEnvController.requestAuth()
                    }
                }
            }

            // Error from SystemEnvController
            Text {
                visible: SystemEnvController && SystemEnvController.lastError !== ""
                text: "⚠  " + (SystemEnvController ? SystemEnvController.lastError : "")
                font.pixelSize: Theme.fontSize("sm")
                color: Theme.color("destructive")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            // System variable list (shown when authorized)
            SettingGroup {
                title: qsTr("System Variables")
                Layout.fillWidth: true
                visible: SystemEnvController && SystemEnvController.authorized

                // Table header (matches user vars header)
                Item {
                    implicitHeight: 32
                    Layout.fillWidth: true

                    RowLayout {
                        anchors { fill: parent; leftMargin: 54; rightMargin: 12 }
                        spacing: 8
                        Text {
                            text: qsTr("KEY")
                            font.pixelSize: Theme.fontSize("xs"); font.weight: Font.DemiBold
                            font.letterSpacing: 0.5; color: Theme.color("textDisabled")
                            Layout.preferredWidth: 200
                        }
                        Text {
                            text: qsTr("VALUE")
                            font.pixelSize: Theme.fontSize("xs"); font.weight: Font.DemiBold
                            font.letterSpacing: 0.5; color: Theme.color("textDisabled")
                            Layout.fillWidth: true
                        }
                        Text {
                            text: qsTr("MODIFIED")
                            font.pixelSize: Theme.fontSize("xs"); font.weight: Font.DemiBold
                            font.letterSpacing: 0.5; color: Theme.color("textDisabled")
                            Layout.preferredWidth: 70; horizontalAlignment: Text.AlignRight
                        }
                        Item { Layout.preferredWidth: 60 }
                    }
                    Rectangle {
                        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                        height: 1; color: Theme.color("panelBorder")
                    }
                }

                Repeater {
                    model: SystemEnvController ? SystemEnvController.vars : []

                    delegate: Item {
                        readonly property var entry: modelData
                        Layout.fillWidth: true
                        implicitHeight: sysRow.implicitHeight + 2

                        RowLayout {
                            id: sysRow
                            anchors { left: parent.left; right: parent.right; leftMargin: 8; rightMargin: 8 }
                            spacing: 8

                            // Enabled toggle
                            Switch {
                                implicitHeight: 20
                                checked: entry.enabled !== false
                                onToggled: SystemEnvController.writeVar(
                                    entry.key, entry.value, entry.comment || "",
                                    entry.mergeMode || "replace", checked)
                            }

                            Text {
                                text: entry.key
                                font.pixelSize: Theme.fontSize("sm")
                                font.weight: Font.Medium
                                color: Theme.color("textPrimary")
                                Layout.preferredWidth: 192
                                elide: Text.ElideRight
                            }
                            Text {
                                text: entry.value
                                font.pixelSize: Theme.fontSize("sm")
                                color: Theme.color("textSecondary")
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                            Text {
                                text: entry.modifiedAt ? entry.modifiedAt.substring(0, 10) : ""
                                font.pixelSize: Theme.fontSize("xs")
                                color: Theme.color("textDisabled")
                                Layout.preferredWidth: 70
                                horizontalAlignment: Text.AlignRight
                            }

                            Button {
                                text: qsTr("Delete")
                                implicitHeight: 26
                                onClicked: SystemEnvController.deleteVar(entry.key)
                                background: Rectangle {
                                    radius: Theme.radiusControl || 4
                                    color: parent.hovered ? Theme.color("destructiveSubtle") : "transparent"
                                    border.color: Theme.color("destructive"); border.width: 1
                                }
                                contentItem: Text {
                                    text: parent.text; font.pixelSize: Theme.fontSize("xs")
                                    color: Theme.color("destructive")
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }

                        Rectangle {
                            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                            height: 1; color: Theme.color("panelBorder"); opacity: 0.5
                        }
                    }
                }

                // Empty state
                Item {
                    visible: !SystemEnvController || SystemEnvController.vars.length === 0
                    implicitHeight: 56
                    Layout.fillWidth: true

                    Text {
                        anchors.centerIn: parent
                        text: qsTr("No system variables defined.")
                        font.pixelSize: Theme.fontSize("sm")
                        color: Theme.color("textSecondary")
                    }
                }
            }

            // Add System Variable button (only when authorized)
            Button {
                text: qsTr("+ Add System Variable")
                Layout.alignment: Qt.AlignRight
                implicitHeight: 36
                visible: SystemEnvController && SystemEnvController.authorized
                onClicked: sysAddDialog.open()
                background: Rectangle {
                    radius: Theme.radiusControl
                    color: parent.hovered ? Qt.darker(Theme.color("accent"), 1.1) : Theme.color("accent")
                    Behavior on color { ColorAnimation { duration: 120 } }
                }
                contentItem: Text {
                    text: parent.text; font.pixelSize: Theme.fontSize("sm"); font.weight: Font.Medium
                    color: Theme.color("accentText")
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }

    // ── Add System Variable dialog ─────────────────────────────────────────
    AddEditEnvVarDialog {
        id: sysAddDialog
        onAccepted: {
            if (SystemEnvController)
                SystemEnvController.writeVar(resultKey, resultValue, resultComment,
                                             resultMergeMode, true)
        }
    }
}
