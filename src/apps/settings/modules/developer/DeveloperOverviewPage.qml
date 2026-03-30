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
    property var daemonHealthSnapshot: (DaemonHealthClient && DaemonHealthClient.snapshot) ? DaemonHealthClient.snapshot : ({})
    property string sessionStartupMode: (typeof SessionStartupMode !== "undefined") ? String(SessionStartupMode || "normal") : "normal"
    property bool safeModeActive: (typeof SafeModeActive !== "undefined") ? !!SafeModeActive : false
    property bool userAnimationEnabled: true
    readonly property bool runtimeAnimationsEnabled: !safeModeActive && userAnimationEnabled

    function refreshAnimationPolicy() {
        if (typeof UIPreferences !== "undefined" && UIPreferences && UIPreferences.getPreference) {
            userAnimationEnabled = !!UIPreferences.getPreference("windowing.animationEnabled", true)
        } else if (typeof AnimationsEnabled !== "undefined") {
            userAnimationEnabled = !!AnimationsEnabled
        } else {
            userAnimationEnabled = true
        }
    }

    function formatTimelineTs(tsMs) {
        if (!tsMs || Number(tsMs) <= 0)
            return "-"
        return new Date(Number(tsMs)).toLocaleString()
    }

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

        SettingGroup {
            Layout.fillWidth: true
            label: qsTr("Animation Runtime")

            SettingCard {
                width: parent.width
                label: qsTr("Session Mode")
                description: root.sessionStartupMode
                control: Item { width: 1; height: 1 }
            }

            SettingCard {
                width: parent.width
                label: qsTr("Safe Mode")
                description: root.safeModeActive ? qsTr("active") : qsTr("inactive")
                control: Row {
                    spacing: 6
                    Rectangle {
                        width: 8; height: 8; radius: 4
                        anchors.verticalCenter: parent.verticalCenter
                        color: root.safeModeActive
                               ? (Theme.color("warning") || "#f59e0b")
                               : (Theme.color("success") || "#22c55e")
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        font.pixelSize: Theme.fontSize("xs")
                        color: Theme.color("textSecondary")
                        text: root.safeModeActive ? qsTr("forces animation off") : qsTr("normal policy")
                    }
                }
            }

            SettingCard {
                width: parent.width
                label: qsTr("User Animation Preference")
                description: root.userAnimationEnabled ? qsTr("enabled") : qsTr("disabled")
                control: Item { width: 1; height: 1 }
            }

            SettingCard {
                width: parent.width
                label: qsTr("Effective Animations")
                description: root.runtimeAnimationsEnabled ? qsTr("enabled") : qsTr("disabled")
                control: Row {
                    spacing: 6
                    Rectangle {
                        width: 8; height: 8; radius: 4
                        anchors.verticalCenter: parent.verticalCenter
                        color: root.runtimeAnimationsEnabled
                               ? (Theme.color("success") || "#22c55e")
                               : (Theme.color("warning") || "#f59e0b")
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        font.pixelSize: Theme.fontSize("xs")
                        color: Theme.color("textSecondary")
                        text: root.runtimeAnimationsEnabled
                              ? qsTr("normal motion profile")
                              : qsTr("reduced/minimal motion policy")
                    }
                }
            }

            SettingCard {
                width: parent.width
                label: qsTr("Quick Action")
                description: qsTr("Open animation and visual motion settings.")
                control: RowLayout {
                    spacing: 8
                    Button {
                        text: qsTr("Open Motion Settings")
                        onClicked: {
                            var opened = false
                            if (typeof SettingsApp !== "undefined" && SettingsApp && SettingsApp.openDeepLink) {
                                opened = !!SettingsApp.openDeepLink("settings://appearance/theme")
                            }
                            if (!opened && typeof SettingsApp !== "undefined" && SettingsApp && SettingsApp.openModuleSetting) {
                                SettingsApp.openModuleSetting("appearance", "theme")
                                opened = true
                            }
                            if (!opened && typeof SettingsApp !== "undefined" && SettingsApp && SettingsApp.openModule) {
                                SettingsApp.openModule("appearance")
                            }
                        }
                    }
                }
            }
        }

        SettingGroup {
            Layout.fillWidth: true
            label: qsTr("Daemon Health")

            SettingCard {
                width: parent.width
                label: qsTr("Workspace Daemon")
                description: DaemonHealthClient && DaemonHealthClient.serviceAvailable
                             ? qsTr("org.slm.WorkspaceManager1")
                             : qsTr("service offline")
                control: Row {
                    spacing: 6
                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        anchors.verticalCenter: parent.verticalCenter
                        color: {
                            if (!DaemonHealthClient || !DaemonHealthClient.serviceAvailable)
                                return Theme.color("textDisabled")
                            return root.daemonHealthSnapshot.degraded
                                   ? (Theme.color("warning") || "#f59e0b")
                                   : (Theme.color("success") || "#22c55e")
                        }
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        font.pixelSize: Theme.fontSize("xs")
                        color: Theme.color("textSecondary")
                        text: {
                            if (!DaemonHealthClient || !DaemonHealthClient.serviceAvailable)
                                return qsTr("offline")
                            return root.daemonHealthSnapshot.degraded ? qsTr("degraded") : qsTr("healthy")
                        }
                    }
                }
            }

            SettingCard {
                width: parent.width
                label: qsTr("Reason Codes")
                description: {
                    const reasons = root.daemonHealthSnapshot.reasonCodes || []
                    return reasons.length > 0 ? reasons.join(", ") : qsTr("none")
                }
                control: Item {
                    width: 1
                    height: 1
                }
            }

            SettingCard {
                width: parent.width
                label: qsTr("Recent Timeline")
                description: {
                    const list = root.daemonHealthSnapshot.timeline || []
                    return qsTr("%1 events").arg(list.length)
                }
                control: Column {
                    width: parent.width
                    spacing: 4

                    Repeater {
                        model: {
                            const rows = root.daemonHealthSnapshot.timeline || []
                            return rows.slice(Math.max(0, rows.length - 5)).reverse()
                        }
                        delegate: Text {
                            width: parent.width
                            wrapMode: Text.WordWrap
                            font.pixelSize: Theme.fontSize("xs")
                            color: Theme.color("textSecondary")
                            text: {
                                const row = modelData || {}
                                const ts = root.formatTimelineTs(row.tsMs)
                                const peer = row.peer || "unknown"
                                const code = row.code || "n/a"
                                const sev = row.severity || "n/a"
                                const msg = row.message || ""
                                return ts + " • " + peer + " • " + code + " • " + sev
                                       + (msg.length > 0 ? " • " + msg : "")
                            }
                        }
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
                    onClicked: {
                        if (ProcessServicesController)
                            ProcessServicesController.refresh()
                        if (DaemonHealthClient)
                            DaemonHealthClient.refresh()
                    }
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

    Component.onCompleted: {
        root.refreshAnimationPolicy()
        if (DaemonHealthClient)
            DaemonHealthClient.refresh()
    }

    Connections {
        target: (typeof UIPreferences !== "undefined" && UIPreferences) ? UIPreferences : null
        function onPreferenceChanged(key, value) {
            const normalized = String(key || "").replace(/\./g, "/").toLowerCase()
            if (normalized === "windowing/animationenabled") {
                root.userAnimationEnabled = !!value
            }
        }
    }
}
