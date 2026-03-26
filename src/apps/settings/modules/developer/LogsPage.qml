import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../../../../../Qml/apps/settings/components"
import Slm_Desktop

Item {
    id: root
    anchors.fill: parent

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12

        // ── Header ────────────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    text: qsTr("Logs & Events")
                    font.pixelSize: Theme.fontSize("h2")
                    font.weight: Font.DemiBold
                    color: Theme.color("textPrimary")
                }
                Text {
                    text: qsTr("Aggregated log stream from shell components.")
                    font.pixelSize: Theme.fontSize("sm")
                    color: Theme.color("textSecondary")
                }
            }

            // Stream toggle
            Button {
                text: LogsController && LogsController.streaming
                      ? qsTr("Stop Stream") : qsTr("Start Stream")
                enabled: LogsController && LogsController.serviceAvailable
                highlighted: LogsController && LogsController.streaming
                onClicked: {
                    if (!LogsController) return
                    if (LogsController.streaming)
                        LogsController.stopStream()
                    else
                        LogsController.startStream()
                }
            }

            Button {
                text: qsTr("Refresh")
                enabled: LogsController && LogsController.serviceAvailable
                onClicked: if (LogsController) LogsController.refresh()
            }

            Button {
                text: qsTr("Export")
                enabled: LogsController && LogsController.serviceAvailable
                onClicked: {
                    if (!LogsController) return
                    const path = LogsController.exportBundle()
                    exportPathBanner.filePath = path
                    exportPathBanner.visible = true
                    exportHideTimer.restart()
                }
            }
        }

        // ── Filters ───────────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: qsTr("Source:")
                font.pixelSize: Theme.fontSize("sm")
                color: Theme.color("textSecondary")
            }

            ComboBox {
                id: sourceCombo
                implicitWidth: 180
                model: {
                    const sources = LogsController ? LogsController.sources : []
                    return [qsTr("All")] .concat(sources)
                }
                onCurrentIndexChanged: {
                    if (!LogsController) return
                    LogsController.filterSource = currentIndex === 0 ? "" : currentText
                }
            }

            Text {
                text: qsTr("Level:")
                font.pixelSize: Theme.fontSize("sm")
                color: Theme.color("textSecondary")
            }

            ComboBox {
                id: levelCombo
                implicitWidth: 130
                model: [qsTr("All"), "debug", "info", "warning", "error", "critical"]
                onCurrentIndexChanged: {
                    if (!LogsController) return
                    LogsController.filterLevel = currentIndex === 0 ? "" : currentText
                }
            }

            Item { Layout.fillWidth: true }

            Text {
                text: LogsController
                      ? qsTr("%1 entries").arg(LogsController.entries.length)
                      : ""
                font.pixelSize: Theme.fontSize("xs")
                color: Theme.color("textDisabled")
            }
        }

        // ── Export path banner ────────────────────────────────────────────
        Rectangle {
            id: exportPathBanner
            Layout.fillWidth: true
            height: 34
            visible: false
            radius: Theme.radiusCard
            color: Theme.color("surface")
            border.color: Theme.color("panelBorder")
            border.width: 1

            property string filePath: ""

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8

                Text {
                    text: qsTr("Exported to:")
                    font.pixelSize: Theme.fontSize("xs")
                    color: Theme.color("textSecondary")
                }
                Text {
                    Layout.fillWidth: true
                    text: exportPathBanner.filePath
                    font.pixelSize: Theme.fontSize("xs")
                    font.family: "monospace"
                    color: Theme.color("textPrimary")
                    elide: Text.ElideMiddle
                }
                Button {
                    text: qsTr("Copy Path")
                    implicitHeight: 24
                    onClicked: {
                        clipboardHelper.text = exportPathBanner.filePath
                        exportPathBanner.visible = false
                    }
                }
            }

            Timer {
                id: exportHideTimer
                interval: 8000
                onTriggered: exportPathBanner.visible = false
            }
        }

        TextEdit { id: clipboardHelper; visible: false
            onTextChanged: { selectAll(); copy() }
        }

        // ── Offline banner ────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 40
            visible: !LogsController || !LogsController.serviceAvailable
            color: Theme.color("warningBg") || Qt.rgba(0.96, 0.8, 0.2, 0.12)
            radius: Theme.radiusCard

            Text {
                anchors.centerIn: parent
                text: qsTr("Logger service (slm-loggerd) is offline. Start it to see logs.")
                font.pixelSize: Theme.fontSize("sm")
                color: Theme.color("textSecondary")
            }
        }

        // ── Log list ──────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.color("surface")
            border.color: Theme.color("panelBorder")
            border.width: 1
            radius: Theme.radiusCard
            clip: true

            ListView {
                id: logList
                anchors.fill: parent
                anchors.margins: 1
                clip: true
                model: LogsController ? LogsController.entries : []
                spacing: 0

                // Auto-scroll when streaming
                onCountChanged: {
                    if (LogsController && LogsController.streaming)
                        Qt.callLater(function() { logList.positionViewAtEnd() })
                }

                delegate: Rectangle {
                    width: logList.width
                    height: msgText.implicitHeight + 10
                    color: {
                        switch (modelData.level) {
                        case "error":
                        case "critical": return Qt.rgba(0.94, 0.2, 0.2, 0.06)
                        case "warning":  return Qt.rgba(0.96, 0.7, 0.1, 0.06)
                        default:         return index % 2 === 0 ? "transparent" : Qt.rgba(0,0,0,0.015)
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        anchors.topMargin: 5
                        anchors.bottomMargin: 5
                        spacing: 8

                        // Level badge
                        Rectangle {
                            Layout.preferredWidth: 60
                            height: 18
                            radius: 3
                            color: {
                                switch (modelData.level) {
                                case "error":
                                case "critical": return Qt.rgba(0.94, 0.2, 0.2, 0.15)
                                case "warning":  return Qt.rgba(0.96, 0.7, 0.1, 0.15)
                                case "debug":    return Qt.rgba(0.5, 0.5, 0.5, 0.1)
                                default:         return Qt.rgba(0.2, 0.6, 0.9, 0.1)
                                }
                            }
                            Text {
                                anchors.centerIn: parent
                                text: modelData.level || "info"
                                font.pixelSize: Theme.fontSize("xs")
                                font.weight: Font.Medium
                                color: {
                                    switch (modelData.level) {
                                    case "error":
                                    case "critical": return "#dc2626"
                                    case "warning":  return "#d97706"
                                    default:         return Theme.color("textSecondary")
                                    }
                                }
                            }
                        }

                        // Source
                        Text {
                            Layout.preferredWidth: 90
                            text: modelData.source || ""
                            font.pixelSize: Theme.fontSize("xs")
                            font.family: "monospace"
                            color: Theme.color("textSecondary")
                            elide: Text.ElideRight
                        }

                        // Timestamp
                        Text {
                            Layout.preferredWidth: 90
                            text: {
                                const ts = modelData.timestamp || ""
                                return ts.length > 19 ? ts.substring(11, 19) : ts
                            }
                            font.pixelSize: Theme.fontSize("xs")
                            font.family: "monospace"
                            color: Theme.color("textDisabled")
                        }

                        // Message
                        Text {
                            id: msgText
                            Layout.fillWidth: true
                            text: modelData.message || ""
                            font.pixelSize: Theme.fontSize("sm")
                            font.family: "monospace"
                            color: Theme.color("textPrimary")
                            wrapMode: Text.WrapAnywhere
                        }
                    }
                }

                // Empty state
                Text {
                    anchors.centerIn: parent
                    visible: logList.count === 0
                    text: LogsController && LogsController.serviceAvailable
                          ? qsTr("No log entries. Press Refresh or Start Stream.")
                          : ""
                    font.pixelSize: Theme.fontSize("sm")
                    color: Theme.color("textDisabled")
                }
            }
        }
    }
}
