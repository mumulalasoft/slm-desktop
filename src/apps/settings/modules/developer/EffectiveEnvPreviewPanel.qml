import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../../../../../Qml/apps/settings/components"
import Slm_Desktop

// EffectiveEnvPreviewPanel — shows the fully-merged effective environment
// as resolved by slm-envd.  Requires EffectiveEnvPreview context property.

ColumnLayout {
    id: root
    spacing: 8

    // ── Service-offline banner ─────────────────────────────────────────────
    Rectangle {
        visible: !EnvServiceClient.serviceAvailable
        Layout.fillWidth: true
        implicitHeight: offlineRow.implicitHeight + 12
        color: Qt.rgba(1.0, 0.5, 0.0, 0.08)
        radius: Theme.radiusCard
        border.color: Qt.rgba(1.0, 0.5, 0.0, 0.25)
        border.width: 1

        RowLayout {
            id: offlineRow
            anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
            anchors.margins: 10
            spacing: 8
            Text {
                text: "⚠"
                font.pixelSize: Theme.fontSize("sm")
                color: Theme.color("warning")
            }
            Text {
                text: qsTr("Environment service (slm-envd) is not running. " +
                            "Showing stored variables only.")
                font.pixelSize: Theme.fontSize("sm")
                color: Theme.color("textSecondary")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
    }

    // ── Header row ────────────────────────────────────────────────────────
    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        Text {
            text: qsTr("Effective Environment")
            font.pixelSize: Theme.fontSize("body")
            font.weight: Font.DemiBold
            color: Theme.color("textPrimary")
            Layout.fillWidth: true
        }

        Text {
            text: EffectiveEnvPreview.statusText
            font.pixelSize: Theme.fontSize("xs")
            color: Theme.color("textDisabled")
        }

        Button {
            text: qsTr("Refresh")
            implicitHeight: 28
            enabled: EnvServiceClient.serviceAvailable
            onClicked: EffectiveEnvPreview.refresh()
            background: Rectangle {
                radius: Theme.radiusControl
                color: parent.hovered ? Theme.color("hoverOverlay") : "transparent"
                border.color: Theme.color("panelBorder"); border.width: 1
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: Theme.fontSize("xs")
                color: parent.enabled ? Theme.color("textPrimary") : Theme.color("textDisabled")
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment:   Text.AlignVCenter
            }
        }
    }

    // ── Filter field ──────────────────────────────────────────────────────
    TextField {
        id: filterField
        Layout.fillWidth: true
        placeholderText: qsTr("Filter resolved variables…")
        font.pixelSize: Theme.fontSize("sm")
        onTextChanged: EffectiveEnvPreview.filterText = text
        background: Rectangle {
            radius: Theme.radiusControl
            color: Theme.color("surface")
            border.color: parent.activeFocus ? Theme.color("accent") : Theme.color("panelBorder")
            border.width: parent.activeFocus ? 2 : 1
            Behavior on border.color { ColorAnimation { duration: 120 } }
        }
    }

    // ── Loading indicator ─────────────────────────────────────────────────
    BusyIndicator {
        visible: EffectiveEnvPreview.loading
        Layout.alignment: Qt.AlignHCenter
        implicitWidth: 24; implicitHeight: 24
    }

    // ── Variable list ─────────────────────────────────────────────────────
    SettingGroup {
        Layout.fillWidth: true
        title: ""

        Repeater {
            model: EffectiveEnvPreview.entries

            delegate: Item {
                required property var modelData
                implicitHeight: rowContent.implicitHeight + 16
                Layout.fillWidth: true

                RowLayout {
                    id: rowContent
                    anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                    anchors.leftMargin: 12; anchors.rightMargin: 12
                    spacing: 12

                    Text {
                        text: modelData.key
                        font.family: "monospace"
                        font.pixelSize: Theme.fontSize("sm")
                        font.weight: Font.Medium
                        color: Theme.color("textPrimary")
                        Layout.preferredWidth: 200
                        elide: Text.ElideRight
                    }
                    Text {
                        text: modelData.value
                        font.family: "monospace"
                        font.pixelSize: Theme.fontSize("sm")
                        color: Theme.color("textSecondary")
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        wrapMode: Text.NoWrap
                    }
                    // Layer badge
                    Rectangle {
                        implicitWidth: layerLabel.implicitWidth + 10
                        implicitHeight: 18
                        radius: 4
                        color: layerColor(modelData.layer)
                        opacity: 0.85

                        Text {
                            id: layerLabel
                            anchors.centerIn: parent
                            text: layerName(modelData.layer)
                            font.pixelSize: 9
                            font.weight: Font.Medium
                            color: "white"
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
            visible: EffectiveEnvPreview.entries.length === 0 && !EffectiveEnvPreview.loading
            implicitHeight: 64
            Layout.fillWidth: true
            Text {
                anchors.centerIn: parent
                text: qsTr("No variables to show.")
                font.pixelSize: Theme.fontSize("sm")
                color: Theme.color("textDisabled")
            }
        }
    }

    // ── helpers ───────────────────────────────────────────────────────────
    function layerName(layer) {
        switch (layer) {
        case 0: return "default"
        case 1: return "system"
        case 2: return "user"
        case 3: return "session"
        case 4: return "per-app"
        case 5: return "launch"
        default: return "?"
        }
    }

    function layerColor(layer) {
        switch (layer) {
        case 2: return Theme.color("accent")
        case 3: return "#6c5ce7"
        case 4: return "#00b894"
        case 5: return "#e17055"
        default: return Theme.color("textDisabled")
        }
    }
}
