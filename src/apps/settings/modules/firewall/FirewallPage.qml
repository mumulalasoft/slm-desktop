import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Slm_Desktop
import "../../../../../Qml/apps/settings/components"

Flickable {
    id: root
    anchors.fill: parent
    contentHeight: contentColumn.height + 32
    clip: true

    readonly property var firewallModes: [
        { value: "home", label: qsTr("Home") },
        { value: "public", label: qsTr("Public") },
        { value: "custom", label: qsTr("Custom") }
    ]
    readonly property var firewallPolicies: [
        { value: "deny", label: qsTr("Deny") },
        { value: "allow", label: qsTr("Allow") },
        { value: "prompt", label: qsTr("Prompt") }
    ]

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
        spacing: 20

        Text {
            text: qsTr("Firewall")
            font.pixelSize: Theme.fontSize("display")
            font.weight: Theme.fontWeight("bold")
            color: Theme.color("textPrimary")
        }

        SettingGroup {
            title: qsTr("Service")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("Status")
                description: FirewallServiceClient.available
                             ? qsTr("Connected to desktop-firewalld")
                             : qsTr("Firewall service offline")

                Text {
                    text: FirewallServiceClient.available ? qsTr("Online") : qsTr("Offline")
                    color: FirewallServiceClient.available
                           ? Theme.color("success")
                           : Theme.color("error")
                    font.weight: Theme.fontWeight("medium")
                }
            }

            SettingCard {
                label: qsTr("Firewall")
                description: qsTr("Secure by default: deny incoming, allow established and loopback")

                SettingToggle {
                    checked: FirewallServiceClient.enabled
                    enabled: FirewallServiceClient.available
                    onToggled: FirewallServiceClient.setEnabled(checked)
                }
            }
        }

        SettingGroup {
            title: qsTr("Mode")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("Firewall Mode")
                description: qsTr("Home allows trusted LAN apps, Public blocks all incoming, Custom for manual control")

                ComboBox {
                    model: root.firewallModes.map(function(item) { return item.label })
                    currentIndex: root.modeIndex(FirewallServiceClient.mode)
                    enabled: FirewallServiceClient.available && FirewallServiceClient.enabled
                    onActivated: function(index) {
                        FirewallServiceClient.setMode(root.firewallModes[index].value)
                    }
                }
            }
        }

        SettingGroup {
            title: qsTr("Default Policy")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("Incoming")
                description: qsTr("Fallback action when no app rule matches incoming traffic")

                ComboBox {
                    model: root.firewallPolicies.map(function(item) { return item.label })
                    currentIndex: root.policyIndex(FirewallServiceClient.defaultIncomingPolicy)
                    enabled: FirewallServiceClient.available && FirewallServiceClient.enabled
                    onActivated: function(index) {
                        FirewallServiceClient.setDefaultIncomingPolicy(root.firewallPolicies[index].value)
                    }
                }
            }

            SettingCard {
                label: qsTr("Outgoing")
                description: qsTr("Fallback action when no app rule matches outgoing traffic")

                ComboBox {
                    model: root.firewallPolicies.map(function(item) { return item.label })
                    currentIndex: root.policyIndex(FirewallServiceClient.defaultOutgoingPolicy)
                    enabled: FirewallServiceClient.available && FirewallServiceClient.enabled
                    onActivated: function(index) {
                        FirewallServiceClient.setDefaultOutgoingPolicy(root.firewallPolicies[index].value)
                    }
                }
            }
        }
    }

    Component.onCompleted: FirewallServiceClient.refresh()
}

