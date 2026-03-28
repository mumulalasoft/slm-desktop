import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../../../../Qml/apps/settings/components"

Flickable {
    id: root
    contentHeight: contentColumn.height
    clip: true

    property var nmBinding: SettingsApp.createBindingFor("network", "ethernet", "Disconnected")
    property var wifiEnabledBinding: SettingsApp.createBindingFor("network", "wifi", true)
    property var wifiPolicy: SettingsApp.settingPolicy("network", "wifi")
    property var wifiGrantState: SettingsApp.settingGrantState("network", "wifi")
    property bool wifiAuthorized: !Boolean(wifiPolicy.requiresAuthorization)
    property bool wifiAuthPending: false
    property bool wifiGuard: false
    property string wifiAuthReason: ""
    property var componentIssues: []
    property bool hasBlockingIssues: false

    function refreshComponentIssues() {
        if (typeof ComponentHealth === "undefined" || !ComponentHealth) {
            componentIssues = []
            hasBlockingIssues = false
            return
        }
        componentIssues = ComponentHealth.missingComponentsForDomain("network")
        if (ComponentHealth.hasBlockingMissingForDomain) {
            hasBlockingIssues = !!ComponentHealth.hasBlockingMissingForDomain("network")
        } else {
            hasBlockingIssues = (componentIssues || []).some(function(issue) {
                var level = String((issue || {}).severity || "required").toLowerCase()
                return level === "required"
            })
        }
    }

    ColumnLayout {
        id: contentColumn
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 20
        spacing: 24

        Text {
            text: "Network"
            font.pixelSize: 28
            font.weight: Font.Bold
            color: "#ffffff"
        }

        MissingComponentsCard {
            Layout.fillWidth: true
            issues: root.componentIssues
            message: qsTr("Networking component missing. Some network features are unavailable.")
            installHandler: function(componentId) {
                return ComponentHealth.installComponent(componentId)
            }
            onRefreshRequested: root.refreshComponentIssues()
        }

        // Wi-Fi Section
        SettingGroup {
            title: "Wi-Fi"
            Layout.fillWidth: true

            SettingCard {
                label: "Wi-Fi"
                description: "Connect to wireless networks"

                SettingToggle {
                    id: wifiToggle
                    checked: Boolean(root.wifiEnabledBinding.value)
                    enabled: !root.wifiAuthPending && !root.hasBlockingIssues
                    onToggled: {
                        if (root.wifiGuard) {
                            return
                        }
                        if (Boolean(root.wifiPolicy.requiresAuthorization) && !root.wifiAuthorized) {
                            root.wifiGuard = true
                            checked = !checked
                            root.wifiGuard = false
                            if (!root.wifiAuthPending) {
                                root.wifiAuthPending = true
                                SettingsApp.requestSettingAuthorization("network", "wifi")
                            }
                            return
                        }
                        root.wifiEnabledBinding.value = checked
                    }
                }
            }

            AuthorizationHint {
                requiresAuthorization: Boolean(root.wifiPolicy.requiresAuthorization)
                authorized: root.wifiAuthorized
                pending: root.wifiAuthPending
                allowAlways: Boolean(root.wifiGrantState.allowAlways)
                denyAlways: Boolean(root.wifiGrantState.denyAlways)
                reason: root.wifiAuthReason
                defaultMessage: "Permission required to modify Wi-Fi state."
                onAuthorizationDecision: function(decision) {
                    if (root.wifiAuthPending || root.hasBlockingIssues) {
                        return
                    }
                    root.wifiAuthPending = true
                    SettingsApp.requestSettingAuthorizationWithDecision("network", "wifi", decision)
                }
                onResetRequested: {
                    if (root.hasBlockingIssues) {
                        return
                    }
                    SettingsApp.clearSettingGrant("network", "wifi")
                    root.wifiGrantState = SettingsApp.settingGrantState("network", "wifi")
                    root.wifiAuthorized = !Boolean(root.wifiPolicy.requiresAuthorization)
                    root.wifiAuthReason = ""
                }
            }

            SettingCard {
                label: "Wi-Fi Radio"
                description: wifiToggle.checked ? "Enabled via NetworkManager" : "Disabled"
                Layout.fillWidth: true
            }
        }

        // Ethernet Section
        SettingGroup {
            title: "Ethernet"
            Layout.fillWidth: true

            SettingCard {
                label: "Network Status"
                description: "System-wide connection state"
                
                Text {
                    text: root.nmBinding.value
                    color: root.nmBinding.value === "Connected" ? "#4CAF50" : "#F44336"
                    font.weight: Font.Medium
                }
            }
        }

        // VPN Section
        SettingGroup {
            title: "VPN"
            Layout.fillWidth: true

            SettingCard {
                label: "Add VPN..."
                description: "Configure secure connections"
                
                Button {
                    text: "Add"
                    flat: true
                    enabled: !root.hasBlockingIssues
                }
            }
        }
    }

    Connections {
        target: SettingsApp
        function onSettingAuthorizationFinished(requestId, moduleId, settingId, allowed, reason) {
            if (moduleId !== "network" || settingId !== "wifi") {
                return
            }
            root.wifiAuthPending = false
            root.wifiAuthReason = reason || ""
            root.wifiGrantState = SettingsApp.settingGrantState("network", "wifi")
            if (allowed) {
                root.wifiAuthorized = true
            }
        }
    }

    Component.onCompleted: refreshComponentIssues()
}
