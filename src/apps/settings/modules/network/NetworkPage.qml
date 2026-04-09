import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Slm_Desktop
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
    readonly property var firewallModes: [
        { value: "home", label: "Home" },
        { value: "public", label: "Public" },
        { value: "custom", label: "Custom" }
    ]
    readonly property var firewallPolicies: [
        { value: "deny", label: "Deny" },
        { value: "allow", label: "Allow" },
        { value: "prompt", label: "Prompt" }
    ]

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

    function modeIndex(value) {
        for (var i = 0; i < firewallModes.length; ++i) {
            if (firewallModes[i].value === value) {
                return i
            }
        }
        return 0
    }

    function policyIndex(value) {
        for (var i = 0; i < firewallPolicies.length; ++i) {
            if (firewallPolicies[i].value === value) {
                return i
            }
        }
        return 0
    }

    ColumnLayout {
        id: contentColumn
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 20
        spacing: 24

        Text {
            text: qsTr("Network")
            font.pixelSize: Theme.fontSize("display")
            font.weight: Theme.fontWeight("bold")
            color: Theme.color("textPrimary")
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
                    color: root.nmBinding.value === "Connected"
                           ? Theme.color("success")
                           : Theme.color("error")
                    font.weight: Theme.fontWeight("medium")
                }
            }
        }

        // Firewall Section
        SettingGroup {
            title: "Firewall"
            Layout.fillWidth: true

            SettingCard {
                label: "Service Status"
                description: FirewallServiceClient.available
                             ? "Connected to desktop-firewalld"
                             : "Firewall service offline"

                Text {
                    text: FirewallServiceClient.available ? "Online" : "Offline"
                    color: FirewallServiceClient.available
                           ? Theme.color("success")
                           : Theme.color("error")
                    font.weight: Theme.fontWeight("medium")
                }
            }

            SettingCard {
                label: "Firewall"
                description: "Block incoming by default while keeping desktop networking simple"

                SettingToggle {
                    checked: FirewallServiceClient.enabled
                    enabled: FirewallServiceClient.available && !root.hasBlockingIssues
                    onToggled: FirewallServiceClient.setEnabled(checked)
                }
            }

            SettingCard {
                label: "Mode"
                description: "Choose policy profile for current network context"

                ComboBox {
                    model: root.firewallModes.map(function(item) { return item.label })
                    currentIndex: root.modeIndex(FirewallServiceClient.mode)
                    enabled: FirewallServiceClient.available && FirewallServiceClient.enabled && !root.hasBlockingIssues
                    onActivated: function(index) {
                        FirewallServiceClient.setMode(root.firewallModes[index].value)
                    }
                }
            }

            SettingCard {
                label: "Incoming Default Policy"
                description: "Default action for incoming connections"

                ComboBox {
                    model: root.firewallPolicies.map(function(item) { return item.label })
                    currentIndex: root.policyIndex(FirewallServiceClient.defaultIncomingPolicy)
                    enabled: FirewallServiceClient.available && FirewallServiceClient.enabled && !root.hasBlockingIssues
                    onActivated: function(index) {
                        FirewallServiceClient.setDefaultIncomingPolicy(root.firewallPolicies[index].value)
                    }
                }
            }

            SettingCard {
                label: "Outgoing Default Policy"
                description: "Default action for outgoing connections"

                ComboBox {
                    model: root.firewallPolicies.map(function(item) { return item.label })
                    currentIndex: root.policyIndex(FirewallServiceClient.defaultOutgoingPolicy)
                    enabled: FirewallServiceClient.available && FirewallServiceClient.enabled && !root.hasBlockingIssues
                    onActivated: function(index) {
                        FirewallServiceClient.setDefaultOutgoingPolicy(root.firewallPolicies[index].value)
                    }
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

    Component.onCompleted: {
        refreshComponentIssues()
        FirewallServiceClient.refresh()
    }
}
