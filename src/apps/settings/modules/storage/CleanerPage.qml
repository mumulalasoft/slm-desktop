import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Slm_Desktop
import "file:/usr/lib/settings/components"

Flickable {
    id: root
    contentHeight: mainLayout.implicitHeight + 48
    clip: true

    property string highlightSettingId: ""
    property var selectedAppPaths: ({})
    property int ageDays: 30
    property string cleanMode: "full"

    function fmtBytes(bytes) {
        var v = Number(bytes || 0)
        if (v < 1024) return v.toFixed(0) + " B"
        if (v < 1048576) return (v / 1024).toFixed(1) + " KB"
        if (v < 1073741824) return (v / 1048576).toFixed(1) + " MB"
        return (v / 1073741824).toFixed(2) + " GB"
    }

    function selectedAppsAsList() {
        var out = []
        for (var k in selectedAppPaths)
            if (selectedAppPaths[k] === true) out.push(k)
        return out
    }

    function triggerClean(preview) {
        var opts = {
            "mode":                root.cleanMode,
            "days":                root.ageDays,
            "selectedApps":        root.selectedAppsAsList(),
            "clearThumbnail":      true,
            "clearFailedThumbnail": true
        }
        if (preview) CleanerServiceClient.requestPreview(opts)
        else         CleanerServiceClient.requestClean(opts)
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

    // ── Layout ────────────────────────────────────────────────────────────────
    ColumnLayout {
        id: mainLayout
        anchors.left:      parent.left
        anchors.right:     parent.right
        anchors.top:       parent.top
        anchors.margins:   24
        anchors.topMargin: 32
        spacing: 24

        // ── Hero: disk usage bar ──────────────────────────────────────────────
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: heroContent.implicitHeight + 48

            Rectangle {
                anchors.fill: parent
                radius: Theme.radiusWindow
                color: Qt.rgba(0.5, 0.5, 0.5, 0.05)
                border.color: Qt.rgba(0.5, 0.5, 0.5, 0.10)
                border.width: Theme.borderWidthThin
            }

            ColumnLayout {
                id: heroContent
                anchors.centerIn: parent
                anchors.left:  parent.left
                anchors.right: parent.right
                anchors.margins: 24
                spacing: 16

                // Title row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Rectangle {
                        width: 40; height: 40
                        radius: Theme.radiusControlLarge
                        color: Qt.rgba(0.2, 0.6, 1.0, 0.15)

                        Text {
                            anchors.centerIn: parent
                            text: "\u{1F4BE}"
                            font.pixelSize: Theme.fontSize("title")
                        }
                    }

                    ColumnLayout {
                        spacing: 2
                        Text {
                            text: qsTr("Storage")
                            font.pixelSize: Theme.fontSize("title")
                            font.weight: Theme.fontWeight("semibold")
                            color: Theme.color("textPrimary")
                        }
                        Text {
                            text: CleanerServiceClient.available
                                  ? qsTr("Total cache: %1").arg(root.fmtBytes(CleanerServiceClient.scanResult.totalSizeBytes || 0))
                                  : qsTr("Cleaner service unavailable")
                            font.pixelSize: Theme.fontSize("small")
                            color: Theme.color("textSecondary")
                        }
                    }

                    Item { Layout.fillWidth: true }

                    // Scan / Scanning indicator
                    RowLayout {
                        spacing: 8
                        visible: CleanerServiceClient.available

                        BusyIndicator {
                            visible: CleanerServiceClient.scanning
                            running: CleanerServiceClient.scanning
                            implicitWidth: 20; implicitHeight: 20
                        }

                        Button {
                            text: CleanerServiceClient.scanning ? qsTr("Scanning…") : qsTr("Scan Now")
                            enabled: !CleanerServiceClient.scanning && !CleanerServiceClient.cleaning
                            onClicked: CleanerServiceClient.requestScan()
                        }
                    }
                }

                // Usage bar — breakdown: thumbnail | app cache | (other implied)
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 10
                    visible: CleanerServiceClient.available

                    readonly property real total: Math.max(1, CleanerServiceClient.scanResult.totalSizeBytes || 1)
                    readonly property real thumbFrac: (CleanerServiceClient.scanResult.thumbnail
                        ? CleanerServiceClient.scanResult.thumbnail.sizeBytes : 0) / total
                    readonly property real failFrac: (CleanerServiceClient.scanResult.failed_thumbnail
                        ? CleanerServiceClient.scanResult.failed_thumbnail.sizeBytes : 0) / total

                    Rectangle {
                        anchors.fill: parent
                        radius: Theme.radiusSmPlus
                        color: Qt.rgba(0.5, 0.5, 0.5, 0.12)
                        clip: true

                        Row {
                            anchors.fill: parent

                            // Thumbnail cache segment
                            Rectangle {
                                width: parent.width * parent.parent.thumbFrac
                                height: parent.height
                                color: Theme.color("accent")
                                Behavior on width { NumberAnimation { duration: Theme.durationMd } }
                            }

                            // Failed thumbnail segment
                            Rectangle {
                                width: parent.width * parent.parent.failFrac
                                height: parent.height
                                color: Qt.rgba(1.0, 0.6, 0.1, 0.85)
                                Behavior on width { NumberAnimation { duration: Theme.durationMd } }
                            }
                        }
                    }
                }

                // Legend
                RowLayout {
                    spacing: 20
                    visible: CleanerServiceClient.available

                    Repeater {
                        model: [
                            {
                                color: Theme.color("accent"),
                                label: qsTr("Thumbnails"),
                                size: fmtBytes(CleanerServiceClient.scanResult.thumbnail
                                    ? CleanerServiceClient.scanResult.thumbnail.sizeBytes : 0)
                            },
                            {
                                color: Qt.rgba(1.0, 0.6, 0.1, 0.85),
                                label: qsTr("Failed Thumbnails"),
                                size: fmtBytes(CleanerServiceClient.scanResult.failed_thumbnail
                                    ? CleanerServiceClient.scanResult.failed_thumbnail.sizeBytes : 0)
                            }
                        ]

                        delegate: RowLayout {
                            required property var modelData
                            spacing: 6

                            Rectangle {
                                width: 8; height: 8
                                radius: Theme.radiusSm
                                color: modelData.color
                                Layout.alignment: Qt.AlignVCenter
                            }
                            ColumnLayout {
                                spacing: 0
                                Text {
                                    text: modelData.label
                                    font.pixelSize: Theme.fontSize("caption")
                                    color: Theme.color("textSecondary")
                                }
                                Text {
                                    text: modelData.size
                                    font.pixelSize: Theme.fontSize("caption")
                                    font.weight: Theme.fontWeight("medium")
                                    color: Theme.color("textPrimary")
                                }
                            }
                        }
                    }
                }

                // Daemon unavailable banner
                Rectangle {
                    visible: !CleanerServiceClient.available
                    Layout.fillWidth: true
                    implicitHeight: bannerRow.implicitHeight + 16
                    radius: Theme.radiusCard
                    color: Qt.rgba(1.0, 0.5, 0.0, 0.10)
                    border.color: Qt.rgba(1.0, 0.5, 0.0, 0.25)
                    border.width: Theme.borderWidthThin

                    RowLayout {
                        id: bannerRow
                        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                        anchors.margins: 12
                        spacing: 10

                        Text {
                            text: "⚠️"
                            font.pixelSize: Theme.fontSize("body")
                        }
                        Text {
                            text: qsTr("Cleaner daemon unavailable. Start slm-cleanerd to enable cache analysis.")
                            font.pixelSize: Theme.fontSize("small")
                            color: Theme.color("textSecondary")
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }

        // ── Smart Suggestions ─────────────────────────────────────────────────
        SettingGroup {
            title: qsTr("Recommendations")
            Layout.fillWidth: true
            visible: CleanerServiceClient.available && CleanerServiceClient.smartSuggestions.length > 0

            Repeater {
                model: CleanerServiceClient.smartSuggestions
                delegate: SettingCard {
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true
                    icon: "dialog-information-symbolic"
                    iconSize: 22
                    label: String(modelData)
                    hideSeparator: index === CleanerServiceClient.smartSuggestions.length - 1
                }
            }
        }

        // ── Cache Cleaner ─────────────────────────────────────────────────────
        SettingGroup {
            title: qsTr("Cache Cleaner")
            Layout.fillWidth: true

            // Mode & Age options
            SettingCard {
                objectName: "clean-mode"
                highlighted: root.highlightSettingId === "clean-mode"
                label: qsTr("Clean Mode")
                description: root.cleanMode === "full"  ? qsTr("Remove all cached data.")
                           : root.cleanMode === "age"   ? qsTr("Remove cached files older than the threshold.")
                                                        : qsTr("Analyze usage and suggest the safest items to remove.")
                Layout.fillWidth: true

                ComboBox {
                    implicitWidth: 160
                    model: [qsTr("Full Clean"), qsTr("Age-based"), qsTr("Smart Clean")]
                    currentIndex: root.cleanMode === "full" ? 0 : (root.cleanMode === "age" ? 1 : 2)
                    onActivated: {
                        if      (currentIndex === 0) root.cleanMode = "full"
                        else if (currentIndex === 1) root.cleanMode = "age"
                        else                         root.cleanMode = "smart"
                    }
                }
            }

            SettingCard {
                objectName: "clean-age"
                highlighted: root.highlightSettingId === "clean-age"
                label: qsTr("Age Threshold")
                description: qsTr("Delete files older than %1 days.").arg(root.ageDays)
                visible: root.cleanMode === "age" || root.cleanMode === "smart"
                Layout.fillWidth: true

                SpinBox {
                    from: 1; to: 3650
                    value: root.ageDays
                    onValueModified: root.ageDays = value
                }
            }

            // Progress during cleaning
            Item {
                visible: CleanerServiceClient.cleaning
                Layout.fillWidth: true
                Layout.preferredHeight: progressCol.implicitHeight + 24

                ColumnLayout {
                    id: progressCol
                    anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                    anchors.margins: 16
                    spacing: 6

                    ProgressBar {
                        from: 0; to: 100
                        value: CleanerServiceClient.progressPercent
                        Layout.fillWidth: true
                    }

                    Text {
                        text: CleanerServiceClient.progressMessage
                        font.pixelSize: Theme.fontSize("caption")
                        color: Theme.color("textSecondary")
                    }
                }
            }

            // Error banner
            Item {
                visible: CleanerServiceClient.lastError.length > 0
                Layout.fillWidth: true
                Layout.preferredHeight: errRow.implicitHeight + 16

                Rectangle {
                    anchors.fill: parent
                    anchors.leftMargin: 16; anchors.rightMargin: 16
                    radius: Theme.radiusCard
                    color: Qt.rgba(1.0, 0.2, 0.2, 0.10)

                    RowLayout {
                        id: errRow
                        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                        anchors.margins: 12
                        spacing: 8

                        Text { text: "❌"; font.pixelSize: Theme.fontSize("body") }
                        Text {
                            text: CleanerServiceClient.lastError
                            font.pixelSize: Theme.fontSize("small")
                            color: Theme.color("textSecondary")
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            // Action row
            SettingCard {
                label: qsTr("Run Cleaner")
                description: qsTr("Preview changes before applying, or clean immediately.")
                Layout.fillWidth: true
                hideSeparator: true

                RowLayout {
                    spacing: 8

                    Button {
                        text: qsTr("Preview")
                        enabled: CleanerServiceClient.available && !CleanerServiceClient.cleaning && !CleanerServiceClient.scanning
                        onClicked: root.triggerClean(true)
                    }

                    Button {
                        text: CleanerServiceClient.cleaning ? qsTr("Cleaning…") : qsTr("Clean Now")
                        enabled: CleanerServiceClient.available && !CleanerServiceClient.cleaning && !CleanerServiceClient.scanning
                        highlighted: true
                        onClicked: root.triggerClean(false)
                    }
                }
            }
        }

        // ── Policy ────────────────────────────────────────────────────────────
        SettingGroup {
            title: qsTr("Policy")
            Layout.fillWidth: true

            SettingCard {
                objectName: "auto-clean"
                highlighted: root.highlightSettingId === "auto-clean"
                label: qsTr("Auto Clean")
                description: CleanerServiceClient.autoClean
                    ? qsTr("Periodic cache cleanup is enabled.")
                    : qsTr("Cache cleanup will not run automatically.")
                Layout.fillWidth: true

                SettingToggle {
                    checked: CleanerServiceClient.autoClean
                    onToggled: CleanerServiceClient.updatePolicy({ "auto_clean": checked })
                }
            }

            SettingCard {
                objectName: "max-cache"
                highlighted: root.highlightSettingId === "max-cache"
                label: qsTr("Cache Limit")
                description: qsTr("Trigger cleanup when total cache exceeds %1 MB.").arg(CleanerServiceClient.maxCacheSizeMb)
                Layout.fillWidth: true

                SpinBox {
                    from: 128; to: 10240
                    value: CleanerServiceClient.maxCacheSizeMb
                    onValueModified: CleanerServiceClient.updatePolicy({ "max_cache_size_mb": value })
                }
            }

            SettingCard {
                objectName: "delete-after"
                highlighted: root.highlightSettingId === "delete-after"
                label: qsTr("Default Age Threshold")
                description: qsTr("Remove cache files older than this many days.")
                Layout.fillWidth: true
                hideSeparator: true

                SpinBox {
                    from: 1; to: 3650
                    value: CleanerServiceClient.deleteAfterDays
                    onValueModified: CleanerServiceClient.updatePolicy({ "delete_after_days": value })
                }
            }
        }

        // ── App Cache ─────────────────────────────────────────────────────────
        SettingGroup {
            title: qsTr("App Cache")
            Layout.fillWidth: true
            visible: CleanerServiceClient.available && CleanerServiceClient.appCaches.length > 0

            Repeater {
                model: CleanerServiceClient.appCaches
                delegate: SettingCard {
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true
                    label: String(modelData.name || "")
                    description: root.fmtBytes(modelData.sizeBytes || 0)
                    hideSeparator: index === CleanerServiceClient.appCaches.length - 1

                    SettingToggle {
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

        // Empty state when no app cache data
        Item {
            visible: CleanerServiceClient.available
                     && CleanerServiceClient.appCaches.length === 0
                     && !CleanerServiceClient.scanning
            Layout.fillWidth: true
            Layout.preferredHeight: emptyCol.implicitHeight + 32

            ColumnLayout {
                id: emptyCol
                anchors.centerIn: parent
                spacing: 6

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "✨"
                    font.pixelSize: Theme.fontSize("titleLarge")
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("No app cache data. Run a scan to see results.")
                    font.pixelSize: Theme.fontSize("small")
                    color: Theme.color("textSecondary")
                }
            }
        }
    }
}
