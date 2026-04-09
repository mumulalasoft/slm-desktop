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
    property string appRuleAppId: ""
    property int appRuleDecisionIndex: 1
    property int appRuleDirectionIndex: 0
    property string appResultText: ""
    property bool appResultOk: true

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
    readonly property var appRuleDecisions: [
        { value: "allow", label: qsTr("Allow") },
        { value: "deny", label: qsTr("Block") },
        { value: "prompt", label: qsTr("Prompt") }
    ]
    readonly property var appRuleDirections: [
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
            FirewallServiceClient.refreshIpPolicies()
        }
    }

    function submitAppRule() {
        var appId = String(appRuleAppId || "").trim()
        if (!appId.length) {
            appResultOk = false
            appResultText = qsTr("App ID cannot be empty.")
            return
        }

        var payload = {
            appId: appId,
            decision: appRuleDecisions[appRuleDecisionIndex].value,
            direction: appRuleDirections[appRuleDirectionIndex].value,
            remember: true
        }
        var ok = FirewallServiceClient.setAppPolicy(payload)
        appResultOk = ok
        appResultText = ok
                ? qsTr("Application rule saved.")
                : qsTr("Failed to save application rule.")
        if (ok) {
            appRuleAppId = ""
            FirewallServiceClient.refreshAppPolicies()
        }
    }

    function appPoliciesByDecision(decision) {
        var out = []
        var all = FirewallServiceClient.appPolicies || []
        for (var i = 0; i < all.length; ++i) {
            var entry = all[i] || {}
            if (String(entry.decision || "") === String(decision)) {
                out.push(entry)
            }
        }
        return out
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
            title: qsTr("Application Rules")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("Add Rule")
                description: qsTr("Allow, block, or prompt by application identity")

                ColumnLayout {
                    spacing: 8
                    Layout.fillWidth: true

                    TextField {
                        Layout.fillWidth: true
                        placeholderText: qsTr("App ID (example: org.mozilla.firefox)")
                        text: root.appRuleAppId
                        enabled: FirewallServiceClient.available && FirewallServiceClient.enabled
                        onTextChanged: root.appRuleAppId = text
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        ComboBox {
                            Layout.fillWidth: true
                            model: root.appRuleDecisions.map(function(item) { return item.label })
                            currentIndex: root.appRuleDecisionIndex
                            enabled: FirewallServiceClient.available && FirewallServiceClient.enabled
                            onActivated: function(index) {
                                root.appRuleDecisionIndex = index
                            }
                        }

                        ComboBox {
                            Layout.fillWidth: true
                            model: root.appRuleDirections.map(function(item) { return item.label })
                            currentIndex: root.appRuleDirectionIndex
                            enabled: FirewallServiceClient.available && FirewallServiceClient.enabled
                            onActivated: function(index) {
                                root.appRuleDirectionIndex = index
                            }
                        }

                        Button {
                            text: qsTr("Save")
                            enabled: FirewallServiceClient.available && FirewallServiceClient.enabled
                            onClicked: root.submitAppRule()
                        }
                    }

                    Text {
                        visible: root.appResultText.length > 0
                        text: root.appResultText
                        color: root.appResultOk ? Theme.color("success") : Theme.color("error")
                        font.pixelSize: Theme.fontSize("small")
                    }
                }
            }

            SettingCard {
                label: qsTr("Allowed Apps")
                description: qsTr("Apps with explicit allow policy")

                ColumnLayout {
                    spacing: 8
                    Layout.fillWidth: true

                    Repeater {
                        model: root.appPoliciesByDecision("allow")

                        delegate: Rectangle {
                            Layout.fillWidth: true
                            radius: Theme.radiusControl
                            color: Theme.color("surface")
                            border.width: Theme.borderWidthThin
                            border.color: Theme.color("panelBorder")
                            implicitHeight: allowedRow.implicitHeight + 10

                            RowLayout {
                                id: allowedRow
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 10

                                Text {
                                    Layout.fillWidth: true
                                    text: {
                                        var p = modelData || {}
                                        var appId = String(p.appId || qsTr("(unknown app)"))
                                        var direction = String(p.direction || "incoming")
                                        return appId + " [" + direction + "]"
                                    }
                                    color: Theme.color("textPrimary")
                                    font.pixelSize: Theme.fontSize("small")
                                    elide: Text.ElideRight
                                }

                                Button {
                                    text: qsTr("Remove")
                                    enabled: FirewallServiceClient.available
                                             && FirewallServiceClient.enabled
                                             && String((modelData || {}).policyId || "").length > 0
                                    onClicked: {
                                        var id = String((modelData || {}).policyId || "")
                                        var ok = FirewallServiceClient.removeAppPolicy(id)
                                        root.appResultOk = ok
                                        root.appResultText = ok
                                                ? qsTr("Application rule removed.")
                                                : qsTr("Failed to remove application rule.")
                                    }
                                }
                            }
                        }
                    }

                    Button {
                        text: qsTr("Clear All App Rules")
                        enabled: FirewallServiceClient.available
                                 && FirewallServiceClient.enabled
                                 && FirewallServiceClient.appPolicies.length > 0
                        onClicked: {
                            var ok = FirewallServiceClient.clearAppPolicies()
                            root.appResultOk = ok
                            root.appResultText = ok
                                    ? qsTr("All application rules cleared.")
                                    : qsTr("Failed to clear application rules.")
                        }
                    }
                }
            }

            SettingCard {
                label: qsTr("Blocked Apps")
                description: qsTr("Apps with explicit deny policy")

                ColumnLayout {
                    spacing: 8
                    Layout.fillWidth: true

                    Repeater {
                        model: root.appPoliciesByDecision("deny")

                        delegate: Rectangle {
                            Layout.fillWidth: true
                            radius: Theme.radiusControl
                            color: Theme.color("surface")
                            border.width: Theme.borderWidthThin
                            border.color: Theme.color("panelBorder")
                            implicitHeight: blockedRow.implicitHeight + 10

                            RowLayout {
                                id: blockedRow
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 10

                                Text {
                                    Layout.fillWidth: true
                                    text: {
                                        var p = modelData || {}
                                        var appId = String(p.appId || qsTr("(unknown app)"))
                                        var direction = String(p.direction || "incoming")
                                        return appId + " [" + direction + "]"
                                    }
                                    color: Theme.color("textPrimary")
                                    font.pixelSize: Theme.fontSize("small")
                                    elide: Text.ElideRight
                                }

                                Button {
                                    text: qsTr("Remove")
                                    enabled: FirewallServiceClient.available
                                             && FirewallServiceClient.enabled
                                             && String((modelData || {}).policyId || "").length > 0
                                    onClicked: {
                                        var id = String((modelData || {}).policyId || "")
                                        var ok = FirewallServiceClient.removeAppPolicy(id)
                                        root.appResultOk = ok
                                        root.appResultText = ok
                                                ? qsTr("Application rule removed.")
                                                : qsTr("Failed to remove application rule.")
                                    }
                                }
                            }
                        }
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

            SettingCard {
                label: qsTr("Blocked Entries")
                description: qsTr("Current blocked IP/subnet rules from firewall policy store")

                ColumnLayout {
                    spacing: 8
                    Layout.fillWidth: true

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("%1 active rule(s)").arg(FirewallServiceClient.ipPolicies.length)
                            color: Theme.color("textSecondary")
                            font.pixelSize: Theme.fontSize("small")
                        }

                        Button {
                            text: qsTr("Clear All")
                            enabled: FirewallServiceClient.available
                                     && FirewallServiceClient.enabled
                                     && FirewallServiceClient.ipPolicies.length > 0
                            onClicked: {
                                var ok = FirewallServiceClient.clearIpPolicies()
                                root.blockResultOk = ok
                                root.blockResultText = ok
                                        ? qsTr("All blocked IP policies cleared.")
                                        : qsTr("Failed to clear blocked IP policies.")
                            }
                        }
                    }

                    Repeater {
                        model: FirewallServiceClient.ipPolicies

                        delegate: Rectangle {
                            Layout.fillWidth: true
                            radius: Theme.radiusControl
                            color: Theme.color("surface")
                            border.width: Theme.borderWidthThin
                            border.color: Theme.color("panelBorder")
                            implicitHeight: row.implicitHeight + 10

                            RowLayout {
                                id: row
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 10

                                Text {
                                    Layout.fillWidth: true
                                    text: {
                                        var p = modelData || {}
                                        var targets = (p.targets || []).join(", ")
                                        var scope = String(p.scope || "both")
                                        var reason = String(p.reason || "")
                                        var base = targets.length ? targets : qsTr("(empty)")
                                        var tail = " [" + scope + "]"
                                        if (reason.length) {
                                            tail += " - " + reason
                                        }
                                        return base + tail
                                    }
                                    color: Theme.color("textPrimary")
                                    font.pixelSize: Theme.fontSize("small")
                                    elide: Text.ElideRight
                                }

                                Button {
                                    text: qsTr("Remove")
                                    enabled: FirewallServiceClient.available
                                             && FirewallServiceClient.enabled
                                             && String((modelData || {}).policyId || "").length > 0
                                    onClicked: {
                                        var id = String((modelData || {}).policyId || "")
                                        var ok = FirewallServiceClient.removeIpPolicy(id)
                                        root.blockResultOk = ok
                                        root.blockResultText = ok
                                                ? qsTr("Blocked IP policy removed.")
                                                : qsTr("Failed to remove blocked IP policy.")
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Component.onCompleted: {
        FirewallServiceClient.refresh()
        FirewallServiceClient.refreshAppPolicies()
        FirewallServiceClient.refreshIpPolicies()
    }
}
