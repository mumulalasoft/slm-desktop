import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../../../../Qml/apps/settings/components"

Flickable {
    id: root
    contentHeight: contentColumn.height
    clip: true
    property var powerModeBinding: SettingsApp.createBindingFor("power", "power-mode", "Balanced")
    property var suspendBinding: SettingsApp.createBinding("settings:power/suspend", true)
    property var powerModePolicy: SettingsApp.settingPolicy("power", "power-mode")
    property var suspendPolicy: SettingsApp.settingPolicy("power", "suspend")
    property var powerModeGrantState: SettingsApp.settingGrantState("power", "power-mode")
    property var suspendGrantState: SettingsApp.settingGrantState("power", "suspend")
    property bool powerModeAuthorized: !Boolean(powerModePolicy.requiresAuthorization)
    property bool suspendAuthorized: !Boolean(suspendPolicy.requiresAuthorization)
    property bool powerModeAuthPending: false
    property bool suspendAuthPending: false
    property bool suspendGuard: false
    property string powerModeAuthReason: ""
    property string suspendAuthReason: ""

    ColumnLayout {
        id: contentColumn
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 20
        spacing: 24

        Text {
            text: "Power"
            font.pixelSize: 28
            font.weight: Font.Bold
            color: "#ffffff"
        }

        // Battery Section
        SettingGroup {
            title: "Battery"
            Layout.fillWidth: true

            SettingCard {
                label: "Battery Level"
                description: "Built-in Battery"
                
                RowLayout {
                    spacing: 8
                    ProgressBar {
                        value: 0.85
                        Layout.preferredWidth: 150
                    }
                    Text {
                        text: "85%"
                        color: "#ffffff"
                        font.weight: Font.Medium
                    }
                }
            }

            SettingCard {
                label: "State"
                description: "Discharging"
                
                Text {
                    text: "2h 45m remaining"
                    color: "#aaaaaa"
                    font.pixelSize: 12
                }
            }
        }

        // Performance Section
        SettingGroup {
            title: "Performance Mode"
            Layout.fillWidth: true

            SettingCard {
                label: "Power Mode"
                description: "Balance performance and battery life"
                
                ComboBox {
                    id: powerModeCombo
                    model: ["Power Saver", "Balanced", "Performance"]
                    currentIndex: Math.max(0, model.indexOf(String(root.powerModeBinding.value)))
                    Layout.preferredWidth: 150
                    enabled: !root.powerModeAuthPending
                    onActivated: {
                        if (Boolean(root.powerModePolicy.requiresAuthorization) && !root.powerModeAuthorized) {
                            currentIndex = Math.max(0, model.indexOf(String(root.powerModeBinding.value)))
                            if (!root.powerModeAuthPending) {
                                root.powerModeAuthPending = true
                                SettingsApp.requestSettingAuthorization("power", "power-mode")
                            }
                            return
                        }
                        root.powerModeBinding.value = currentText
                    }
                }
            }

            AuthorizationHint {
                requiresAuthorization: Boolean(root.powerModePolicy.requiresAuthorization)
                authorized: root.powerModeAuthorized
                pending: root.powerModeAuthPending
                allowAlways: Boolean(root.powerModeGrantState.allowAlways)
                denyAlways: Boolean(root.powerModeGrantState.denyAlways)
                reason: root.powerModeAuthReason
                defaultMessage: "Permission required to modify power mode."
                onAuthorizationDecision: function(decision) {
                    if (root.powerModeAuthPending) {
                        return
                    }
                    root.powerModeAuthPending = true
                    SettingsApp.requestSettingAuthorizationWithDecision("power", "power-mode", decision)
                }
                onResetRequested: {
                    SettingsApp.clearSettingGrant("power", "power-mode")
                    root.powerModeGrantState = SettingsApp.settingGrantState("power", "power-mode")
                    root.powerModeAuthorized = !Boolean(root.powerModePolicy.requiresAuthorization)
                    root.powerModeAuthReason = ""
                }
            }
        }

        // Sleep Section
        SettingGroup {
            title: "Sleep & Suspend"
            Layout.fillWidth: true

            SettingCard {
                label: "Screen Off"
                description: "Turn off screen when inactive"
                
                ComboBox {
                    model: ["1 minute", "2 minutes", "5 minutes", "10 minutes", "Never"]
                    currentIndex: 2
                    Layout.preferredWidth: 150
                }
            }

            SettingCard {
                label: "Automatic Suspend"
                description: "Suspend when inactive"
                
                SettingToggle {
                    id: suspendToggle
                    checked: Boolean(root.suspendBinding.value)
                    enabled: !root.suspendAuthPending
                    onToggled: {
                        if (root.suspendGuard) {
                            return
                        }
                        if (Boolean(root.suspendPolicy.requiresAuthorization) && !root.suspendAuthorized) {
                            root.suspendGuard = true
                            checked = !checked
                            root.suspendGuard = false
                            if (!root.suspendAuthPending) {
                                root.suspendAuthPending = true
                                SettingsApp.requestSettingAuthorization("power", "suspend")
                            }
                            return
                        }
                        root.suspendBinding.value = checked
                    }
                }
            }

            AuthorizationHint {
                requiresAuthorization: Boolean(root.suspendPolicy.requiresAuthorization)
                authorized: root.suspendAuthorized
                pending: root.suspendAuthPending
                allowAlways: Boolean(root.suspendGrantState.allowAlways)
                denyAlways: Boolean(root.suspendGrantState.denyAlways)
                reason: root.suspendAuthReason
                defaultMessage: "Permission required to modify suspend behavior."
                onAuthorizationDecision: function(decision) {
                    if (root.suspendAuthPending) {
                        return
                    }
                    root.suspendAuthPending = true
                    SettingsApp.requestSettingAuthorizationWithDecision("power", "suspend", decision)
                }
                onResetRequested: {
                    SettingsApp.clearSettingGrant("power", "suspend")
                    root.suspendGrantState = SettingsApp.settingGrantState("power", "suspend")
                    root.suspendAuthorized = !Boolean(root.suspendPolicy.requiresAuthorization)
                    root.suspendAuthReason = ""
                }
            }
        }
    }

    Connections {
        target: SettingsApp
        function onSettingAuthorizationFinished(requestId, moduleId, settingId, allowed, reason) {
            if (moduleId !== "power") {
                return
            }
            if (settingId === "power-mode") {
                root.powerModeAuthPending = false
                root.powerModeAuthReason = reason || ""
                root.powerModeGrantState = SettingsApp.settingGrantState("power", "power-mode")
                if (allowed) {
                    root.powerModeAuthorized = true
                }
                powerModeCombo.currentIndex = Math.max(0, powerModeCombo.model.indexOf(String(root.powerModeBinding.value)))
            } else if (settingId === "suspend") {
                root.suspendAuthPending = false
                root.suspendAuthReason = reason || ""
                root.suspendGrantState = SettingsApp.settingGrantState("power", "suspend")
                if (allowed) {
                    root.suspendAuthorized = true
                }
            }
        }
    }
}
