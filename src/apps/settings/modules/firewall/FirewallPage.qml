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
    property string blockTarget: ""
    property int blockTypeIndex: 0
    property int blockScopeIndex: 2
    property bool blockTemporary: false
    property string blockDuration: "24h"
    property string blockResultText: ""
    property bool blockResultOk: true

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
    readonly property var blockTypes: [
        { value: "ip", label: qsTr("Single IP") },
        { value: "subnet", label: qsTr("Subnet (CIDR)") }
    ]
    readonly property var blockScopes: [
        { value: "incoming", label: qsTr("Incoming") },
        { value: "outgoing", label: qsTr("Outgoing") },
        { value: "both", label: qsTr("Both") }
    ]

    function modeIndex(value) {
        for (var i = 0; i < firewallModes.length; ++i) {
            if (firewallModes[i].value === value) {
                return i
            }
        }
        return 0
    }

    function submitIpBlock() {
        var target = String(blockTarget || "").trim()
        if (!target.length) {
            blockResultOk = false
            blockResultText = qsTr("Target address cannot be empty.")
            return
        }

        var payload = {
            type: blockTypes[blockTypeIndex].value,
            scope: blockScopes[blockScopeIndex].value,
            reason: "manual-ui-block",
            temporary: blockTemporary
        }
        if (blockTypes[blockTypeIndex].value === "subnet") {
            payload.cidr = target
        } else {
            payload.ip = target
        }
        if (blockTemporary) {
            payload.duration = String(blockDuration || "24h")
        }

        var ok = FirewallServiceClient.setIpPolicy(payload)
        blockResultOk = ok
        blockResultText = ok
                ? qsTr("IP policy applied.")
                : qsTr("Failed to apply IP policy.")
        if (ok) {
            blockTarget = ""
        }
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

        SettingGroup {
            title: qsTr("IP Block")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("Block Address or Network")
                description: qsTr("Add immediate deny rule by IP or CIDR subnet")

                ColumnLayout {
                    spacing: 8
                    Layout.fillWidth: true

                    TextField {
                        Layout.fillWidth: true
                        placeholderText: root.blockTypeIndex === 0
                                         ? qsTr("Example: 203.0.113.10")
                                         : qsTr("Example: 203.0.113.0/24")
                        text: root.blockTarget
                        enabled: FirewallServiceClient.available && FirewallServiceClient.enabled
                        onTextChanged: root.blockTarget = text
                    }

                    RowLayout {
                        spacing: 8
                        Layout.fillWidth: true

                        ComboBox {
                            Layout.fillWidth: true
                            model: root.blockTypes.map(function(item) { return item.label })
                            currentIndex: root.blockTypeIndex
                            enabled: FirewallServiceClient.available && FirewallServiceClient.enabled
                            onActivated: function(index) {
                                root.blockTypeIndex = index
                            }
                        }

                        ComboBox {
                            Layout.fillWidth: true
                            model: root.blockScopes.map(function(item) { return item.label })
                            currentIndex: root.blockScopeIndex
                            enabled: FirewallServiceClient.available && FirewallServiceClient.enabled
                            onActivated: function(index) {
                                root.blockScopeIndex = index
                            }
                        }
                    }

                    RowLayout {
                        spacing: 8
                        Layout.fillWidth: true

                        CheckBox {
                            text: qsTr("Temporary")
                            checked: root.blockTemporary
                            enabled: FirewallServiceClient.available && FirewallServiceClient.enabled
                            onToggled: root.blockTemporary = checked
                        }

                        ComboBox {
                            Layout.preferredWidth: 120
                            model: [qsTr("1h"), qsTr("24h"), qsTr("7d")]
                            currentIndex: 1
                            enabled: root.blockTemporary && FirewallServiceClient.available && FirewallServiceClient.enabled
                            onActivated: function(index) {
                                var values = ["1h", "24h", "7d"]
                                root.blockDuration = values[index]
                            }
                        }

                        Item { Layout.fillWidth: true }

                        Button {
                            text: qsTr("Block")
                            enabled: FirewallServiceClient.available && FirewallServiceClient.enabled
                            onClicked: root.submitIpBlock()
                        }
                    }

                    Text {
                        visible: root.blockResultText.length > 0
                        text: root.blockResultText
                        color: root.blockResultOk ? Theme.color("success") : Theme.color("error")
                        font.pixelSize: Theme.fontSize("small")
                    }
                }
            }
        }
    }

    Component.onCompleted: FirewallServiceClient.refresh()
}
