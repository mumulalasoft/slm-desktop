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
    }
}
