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
                    text: qsTr("App Sandbox")
                    font.pixelSize: Theme.fontSize("h2")
                    font.weight: Font.DemiBold
                    color: Theme.color("textPrimary")
                }
                Text {
                    text: qsTr("Inspect Flatpak sandbox permissions for installed applications.")
                    font.pixelSize: Theme.fontSize("sm")
                    color: Theme.color("textSecondary")
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
            }

            Button {
                text: qsTr("Refresh")
                enabled: !AppSandbox.loading
                onClicked: AppSandbox.refresh()
                ToolTip.text: qsTr("Re-detect sandbox tools and reload app list")
                ToolTip.visible: hovered
                ToolTip.delay: 600
            }
        }

        // ── System status row ─────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: qsTr("Runtime:")
                font.pixelSize: Theme.fontSize("sm")
                color: Theme.color("textSecondary")
            }

            // Flatpak badge
            Rectangle {
                height: 22
                width: flatpakLabel.implicitWidth + 16
                radius: 4
                color: AppSandbox.flatpakAvailable
                    ? Theme.color("accentSubtle")
                    : Theme.color("controlBg")
                border.color: AppSandbox.flatpakAvailable
                    ? Theme.color("accent")
                    : Theme.color("panelBorder")
                border.width: 1

                Text {
                    id: flatpakLabel
                    anchors.centerIn: parent
                    text: AppSandbox.flatpakAvailable
                        ? qsTr("Flatpak ✓")
                        : qsTr("Flatpak — not found")
                    font.pixelSize: Theme.fontSize("xs")
                    color: AppSandbox.flatpakAvailable
                        ? Theme.color("accent")
                        : Theme.color("textDisabled")
                }
            }

            // Bubblewrap badge
            Rectangle {
                height: 22
                width: bwrapLabel.implicitWidth + 16
                radius: 4
                color: AppSandbox.bubblewrapAvailable
                    ? Theme.color("accentSubtle")
                    : Theme.color("controlBg")
                border.color: AppSandbox.bubblewrapAvailable
                    ? Theme.color("accent")
                    : Theme.color("panelBorder")
                border.width: 1

                Text {
                    id: bwrapLabel
                    anchors.centerIn: parent
                    text: AppSandbox.bubblewrapAvailable
                        ? qsTr("Bubblewrap ✓")
                        : qsTr("Bubblewrap — not found")
                    font.pixelSize: Theme.fontSize("xs")
                    color: AppSandbox.bubblewrapAvailable
                        ? Theme.color("accent")
                        : Theme.color("textDisabled")
                }
            }

            Item { Layout.fillWidth: true }
        }

        // ── Error banner ──────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: errorText.implicitHeight + 16
            radius: Theme.radiusCard || 6
            color: Theme.color("destructiveSubtle") || "#fff0f0"
            border.color: Theme.color("destructive") || "#c0392b"
            border.width: 1
            visible: AppSandbox.error.length > 0

            Text {
                id: errorText
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 12
                text: AppSandbox.error
                font.pixelSize: Theme.fontSize("sm")
                color: Theme.color("destructive") || "#c0392b"
                wrapMode: Text.WordWrap
            }
        }

        // ── Loading indicator ─────────────────────────────────────────────
        Item {
            Layout.fillWidth: true
            height: 40
            visible: AppSandbox.loading

            BusyIndicator {
                anchors.centerIn: parent
                running: AppSandbox.loading
            }
        }

        // ── No Flatpak message ────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: noFlatpakCol.implicitHeight + 32
            radius: Theme.radiusCard || 6
            color: Theme.color("surfaceAlt") || Theme.color("surface")
            border.color: Theme.color("panelBorder")
            border.width: 1
            visible: !AppSandbox.loading && !AppSandbox.flatpakAvailable

            ColumnLayout {
                id: noFlatpakCol
                anchors.centerIn: parent
                spacing: 6

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Flatpak not available")
                    font.pixelSize: Theme.fontSize("body")
                    font.weight: Font.DemiBold
                    color: Theme.color("textPrimary")
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Install Flatpak to inspect sandboxed applications.")
                    font.pixelSize: Theme.fontSize("sm")
                    color: Theme.color("textSecondary")
                }
            }
        }

        // ── Empty state (Flatpak available, no apps) ──────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: noAppsCol.implicitHeight + 32
            radius: Theme.radiusCard || 6
            color: Theme.color("surfaceAlt") || Theme.color("surface")
            border.color: Theme.color("panelBorder")
            border.width: 1
            visible: !AppSandbox.loading
                && AppSandbox.flatpakAvailable
                && AppSandbox.apps.length === 0
                && AppSandbox.error.length === 0

            ColumnLayout {
                id: noAppsCol
                anchors.centerIn: parent
                spacing: 6

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("No Flatpak apps installed")
                    font.pixelSize: Theme.fontSize("body")
                    font.weight: Font.DemiBold
                    color: Theme.color("textPrimary")
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Installed Flatpak applications will appear here.")
                    font.pixelSize: Theme.fontSize("sm")
                    color: Theme.color("textSecondary")
                }
            }
        }

        // ── App list ──────────────────────────────────────────────────────
        Repeater {
            model: AppSandbox.apps
            visible: !AppSandbox.loading && AppSandbox.apps.length > 0

            delegate: Rectangle {
                Layout.fillWidth: true
                height: appCardCol.implicitHeight + 20
                radius: Theme.radiusCard || 6
                color: Theme.color("surface")
                border.color: Theme.color("panelBorder")
                border.width: 1

                readonly property var app: modelData
                readonly property var perms: app.permissions || {}
                readonly property string level: app.sandboxLevel || "full"

                ColumnLayout {
                    id: appCardCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 12
                    spacing: 6

                    // App name row + sandbox level badge
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Text {
                                text: app.name || app.id
                                font.pixelSize: Theme.fontSize("body")
                                font.weight: Font.DemiBold
                                color: Theme.color("textPrimary")
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Text {
                                text: app.id
                                    + (app.version ? ("  ·  " + app.version) : "")
                                    + (app.origin  ? ("  ·  " + app.origin)  : "")
                                font.pixelSize: Theme.fontSize("xs")
                                color: Theme.color("textDisabled")
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }

                        // Sandbox level badge
                        Rectangle {
                            height: 20
                            width: levelLabel.implicitWidth + 12
                            radius: 4
                            color: {
                                if (level === "open")    return Theme.color("destructiveSubtle") || "#fff0f0"
                                if (level === "partial") return Theme.color("warningSubtle")     || "#fffbea"
                                return Theme.color("accentSubtle")
                            }
                            border.color: {
                                if (level === "open")    return Theme.color("destructive") || "#c0392b"
                                if (level === "partial") return Theme.color("warning")     || "#d4a017"
                                return Theme.color("accent")
                            }
                            border.width: 1

                            Text {
                                id: levelLabel
                                anchors.centerIn: parent
                                text: {
                                    if (level === "open")    return qsTr("open")
                                    if (level === "partial") return qsTr("partial")
                                    return qsTr("full")
                                }
                                font.pixelSize: Theme.fontSize("xs")
                                font.weight: Font.Medium
                                color: {
                                    if (level === "open")    return Theme.color("destructive") || "#c0392b"
                                    if (level === "partial") return Theme.color("warning")     || "#d4a017"
                                    return Theme.color("accent")
                                }
                            }
                        }
                    }

                    // Permission pills row
                    Flow {
                        Layout.fillWidth: true
                        spacing: 4

                        // Network
                        Loader {
                            active: perms.network === true
                            sourceComponent: permPill
                            onLoaded: item.pillText = qsTr("network")
                        }

                        // Filesystem entries
                        Repeater {
                            model: perms.filesystem || []
                            delegate: Loader {
                                active: true
                                sourceComponent: permPill
                                onLoaded: item.pillText = qsTr("fs: ") + modelData
                            }
                        }

                        // Sockets
                        Repeater {
                            model: perms.sockets || []
                            delegate: Loader {
                                active: true
                                sourceComponent: permPill
                                onLoaded: item.pillText = modelData
                            }
                        }

                        // Devices
                        Repeater {
                            model: perms.devices || []
                            delegate: Loader {
                                active: true
                                sourceComponent: permPill
                                onLoaded: item.pillText = qsTr("dev: ") + modelData
                            }
                        }

                        // D-Bus session count
                        Loader {
                            readonly property int count: (perms.dbusSession || []).length
                            active: count > 0
                            sourceComponent: permPill
                            onLoaded: item.pillText = qsTr("D-Bus session ×") + count
                        }

                        // D-Bus system count
                        Loader {
                            readonly property int count: (perms.dbusSystem || []).length
                            active: count > 0
                            sourceComponent: permPill
                            onLoaded: item.pillText = qsTr("D-Bus system ×") + count
                        }
                    }
                }
            }
        }
    }

    // ── Reusable pill component ────────────────────────────────────────────
    Component {
        id: permPill

        Rectangle {
            property string pillText: ""
            height: 20
            width: pillInner.implicitWidth + 12
            radius: 3
            color: Theme.color("controlBg")
            border.color: Theme.color("panelBorder")
            border.width: 1

            Text {
                id: pillInner
                anchors.centerIn: parent
                text: pillText
                font.pixelSize: Theme.fontSize("xs")
                color: Theme.color("textSecondary")
            }
        }
    }
}
