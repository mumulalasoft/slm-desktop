import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import "../../../../../Qml/apps/settings/components"

Item {
    id: root
    anchors.fill: parent

    property var selectedAppPaths: ({})
    property int ageDays: 30
    property string cleanMode: "full"

    function fmtBytes(bytes) {
        var v = Number(bytes || 0)
        if (v < 1024) return v.toFixed(0) + " B"
        if (v < 1024 * 1024) return (v / 1024).toFixed(1) + " KB"
        if (v < 1024 * 1024 * 1024) return (v / (1024 * 1024)).toFixed(1) + " MB"
        return (v / (1024 * 1024 * 1024)).toFixed(2) + " GB"
    }

    function selectedAppsAsList() {
        var out = []
        for (var k in selectedAppPaths) {
            if (selectedAppPaths[k] === true) {
                out.push(k)
            }
        }
        return out
    }

    Component.onCompleted: {
        CleanerServiceClient.requestPolicy()
        CleanerServiceClient.requestScan()
    }

    Connections {
        target: CleanerServiceClient
        function onPolicyChanged() {
            root.ageDays = CleanerServiceClient.deleteAfterDays
        }
    }

    ScrollView {
        anchors.fill: parent
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: 20
            anchors.margins: 24

            SettingGroup {
                title: qsTr("Cleaner")
                Layout.fillWidth: true

                SettingCard {
                    label: qsTr("Cache Analyzer")
                    description: CleanerServiceClient.available
                                 ? qsTr("Scan cache usage and clean selectively.")
                                 : qsTr("Cleaner daemon unavailable. Start slm-cleanerd.")
                    Layout.fillWidth: true

                    RowLayout {
                        spacing: 8
                        Button {
                            text: CleanerServiceClient.scanning ? qsTr("Scanning...") : qsTr("Scan")
                            enabled: CleanerServiceClient.available && !CleanerServiceClient.scanning
                            onClicked: CleanerServiceClient.requestScan()
                        }
                        Button {
                            text: qsTr("Preview")
                            enabled: CleanerServiceClient.available && !CleanerServiceClient.cleaning
                            onClicked: CleanerServiceClient.requestPreview({
                                "mode": root.cleanMode,
                                "days": root.ageDays,
                                "selectedApps": root.selectedAppsAsList(),
                                "clearThumbnail": true,
                                "clearFailedThumbnail": true
                            })
                        }
                        Button {
                            text: CleanerServiceClient.cleaning ? qsTr("Cleaning...") : qsTr("Clean Selected")
                            enabled: CleanerServiceClient.available && !CleanerServiceClient.cleaning
                            onClicked: CleanerServiceClient.requestClean({
                                "mode": root.cleanMode,
                                "days": root.ageDays,
                                "selectedApps": root.selectedAppsAsList(),
                                "clearThumbnail": true,
                                "clearFailedThumbnail": true
                            })
                        }
                    }

                    RowLayout {
                        spacing: 8
                        Label { text: qsTr("Mode") }
                        ComboBox {
                            model: [qsTr("Full Clean"), qsTr("Age-based"), qsTr("Smart Clean")]
                            currentIndex: root.cleanMode === "full" ? 0 : (root.cleanMode === "age" ? 1 : 2)
                            onActivated: {
                                if (currentIndex === 0) root.cleanMode = "full"
                                else if (currentIndex === 1) root.cleanMode = "age"
                                else root.cleanMode = "smart"
                            }
                        }
                        Label { text: qsTr("Days") }
                        SpinBox {
                            from: 1
                            to: 3650
                            value: root.ageDays
                            onValueModified: root.ageDays = value
                        }
                    }

                    Column {
                        spacing: 2
                        Label {
                            text: qsTr("Thumbnail cache: %1").arg(fmtBytes(CleanerServiceClient.scanResult.thumbnail ? CleanerServiceClient.scanResult.thumbnail.sizeBytes : 0))
                        }
                        Label {
                            text: qsTr("Failed thumbnails: %1").arg(fmtBytes(CleanerServiceClient.scanResult.failed_thumbnail ? CleanerServiceClient.scanResult.failed_thumbnail.sizeBytes : 0))
                        }
                        Label {
                            text: qsTr("Total cache: %1").arg(fmtBytes(CleanerServiceClient.scanResult.totalSizeBytes || 0))
                        }
                    }

                    ProgressBar {
                        from: 0
                        to: 100
                        value: CleanerServiceClient.progressPercent
                        visible: CleanerServiceClient.cleaning
                        Layout.fillWidth: true
                    }
                    Label {
                        visible: CleanerServiceClient.cleaning
                        text: CleanerServiceClient.progressMessage
                    }
                    Label {
                        visible: CleanerServiceClient.lastError.length > 0
                        text: qsTr("Error: %1").arg(CleanerServiceClient.lastError)
                        color: Theme.color("textSecondary")
                    }
                }
            }

            SettingGroup {
                title: qsTr("Policy")
                Layout.fillWidth: true

                SettingCard {
                    label: qsTr("Auto Clean")
                    description: qsTr("Enable periodic cleaner execution based on policy.")
                    Layout.fillWidth: true
                    Switch {
                        checked: CleanerServiceClient.autoClean
                        onToggled: CleanerServiceClient.updatePolicy({ "auto_clean": checked })
                    }
                }

                SettingCard {
                    label: qsTr("Max Cache Size")
                    description: qsTr("Recommended cleanup threshold in MB.")
                    Layout.fillWidth: true
                    SpinBox {
                        from: 128
                        to: 10240
                        value: CleanerServiceClient.maxCacheSizeMb
                        onValueModified: CleanerServiceClient.updatePolicy({ "max_cache_size_mb": value })
                    }
                }

                SettingCard {
                    label: qsTr("Delete After Days")
                    description: qsTr("Default age threshold for age/smart clean.")
                    Layout.fillWidth: true
                    SpinBox {
                        from: 1
                        to: 3650
                        value: CleanerServiceClient.deleteAfterDays
                        onValueModified: CleanerServiceClient.updatePolicy({ "delete_after_days": value })
                    }
                }
            }

            SettingGroup {
                title: qsTr("App Cache")
                Layout.fillWidth: true

                Repeater {
                    model: CleanerServiceClient.appCaches
                    delegate: SettingCard {
                        required property var modelData
                        Layout.fillWidth: true
                        label: String(modelData.name || "")
                        description: root.fmtBytes(modelData.sizeBytes || 0)
                        CheckBox {
                            checked: !!root.selectedAppPaths[String(modelData.path || "")]
                            onToggled: {
                                var key = String(modelData.path || "")
                                if (key.length === 0) return
                                root.selectedAppPaths[key] = checked
                            }
                        }
                    }
                }
            }

            SettingGroup {
                title: qsTr("Smart Suggestion")
                Layout.fillWidth: true

                Repeater {
                    model: CleanerServiceClient.smartSuggestions
                    delegate: Label {
                        required property var modelData
                        text: "\u2022 " + String(modelData)
                        wrapMode: Text.WordWrap
                        color: Theme.color("textSecondary")
                    }
                }

                Label {
                    visible: CleanerServiceClient.smartSuggestions.length === 0
                    text: qsTr("No suggestion for now.")
                    color: Theme.color("textSecondary")
                }
            }
        }
    }
}
