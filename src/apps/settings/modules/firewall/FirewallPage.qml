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
    property string blockNote: ""
    property int ipSortIndex: 0
    property int ipFilterIndex: 0
    property string blockResultText: ""
    property bool blockResultOk: true
    property string appRuleAppId: ""
    property int appRuleDecisionIndex: 1
    property int appRuleDirectionIndex: 0
    property string appResultText: ""
    property bool appResultOk: true
    property string promptPidText: "-1"
    property int promptDirectionIndex: 0
    property bool promptRemember: false
    property string promptResultText: ""
    property bool promptResultOk: true
    property bool connectionRemember: false
    property string connectionResultText: ""
    property bool connectionResultOk: true
    readonly property bool devPromptSimulationEnabled: Qt.application.arguments.indexOf("--firewall-dev") !== -1

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
    readonly property var ipSortModes: [
        { value: "latest", label: qsTr("Latest") },
        { value: "most_hits", label: qsTr("Most Hits") },
        { value: "target_asc", label: qsTr("Target A-Z") }
    ]
    readonly property var ipFilterModes: [
        { value: "all", label: qsTr("All") },
        { value: "temporary", label: qsTr("Temporary") },
        { value: "permanent", label: qsTr("Permanent") },
        { value: "active_hits", label: qsTr("Hits > 0") }
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
    readonly property var connectionDirections: [
        { value: "incoming", label: qsTr("Incoming") },
        { value: "outgoing", label: qsTr("Outgoing") }
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
        var note = String(blockNote || "").trim()
        if (note.length) {
            payload.note = note
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
            blockNote = ""
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

    function quarantineApp(appId) {
        var id = String(appId || "").trim()
        if (!id.length) {
            appResultOk = false
            appResultText = qsTr("App ID cannot be empty.")
            return false
        }

        var ok = FirewallServiceClient.setAppPolicy({
            appId: id,
            decision: "deny",
            direction: "both",
            remember: true
        })
        appResultOk = ok
        appResultText = ok
                ? qsTr("Application quarantined (incoming and outgoing blocked).")
                : qsTr("Failed to quarantine application.")
        if (ok) {
            FirewallServiceClient.refreshAppPolicies()
        }
        return ok
    }

    function unquarantineApp(appId) {
        var id = String(appId || "").trim()
        if (!id.length) {
            appResultOk = false
            appResultText = qsTr("App ID cannot be empty.")
            return false
        }

        var all = FirewallServiceClient.appPolicies || []
        var targets = []
        for (var i = 0; i < all.length; ++i) {
            var row = all[i] || {}
            if (String(row.appId || "") !== id) {
                continue
            }
            if (String(row.decision || "") !== "deny") {
                continue
            }
            if (String(row.direction || "") !== "both") {
                continue
            }
            var policyId = String(row.policyId || "")
            if (policyId.length) {
                targets.push(policyId)
            }
        }

        if (targets.length === 0) {
            appResultOk = false
            appResultText = qsTr("No quarantine rule found for this app.")
            return false
        }

        var removed = 0
        for (var j = 0; j < targets.length; ++j) {
            if (FirewallServiceClient.removeAppPolicy(targets[j])) {
                removed += 1
            }
        }
        var ok = removed === targets.length
        appResultOk = ok
        appResultText = ok
                ? qsTr("Application unquarantined.")
                : qsTr("Failed to fully unquarantine application.")
        FirewallServiceClient.refreshAppPolicies()
        return ok
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

    function parseIsoMs(value) {
        var text = String(value || "").trim()
        if (!text.length) {
            return 0
        }
        var ms = Date.parse(text)
        return isNaN(ms) ? 0 : ms
    }

    function ipPolicyPrimaryTarget(entry) {
        var p = entry || {}
        var targets = p.targets || []
        if (targets.length > 0) {
            return String(targets[0] || "")
        }
        return ""
    }

    function sortedIpPolicies() {
        var allItems = (FirewallServiceClient.ipPolicies || []).slice(0)
        var filterMode = ipFilterModes[ipFilterIndex]
                ? String(ipFilterModes[ipFilterIndex].value || "all")
                : "all"
        var items = []
        for (var i = 0; i < allItems.length; ++i) {
            var row = allItems[i] || {}
            var temporary = Boolean(row.temporary)
            var hits = Number(row.hitCount || 0)
            if (filterMode === "temporary" && !temporary) {
                continue
            }
            if (filterMode === "permanent" && temporary) {
                continue
            }
            if (filterMode === "active_hits" && hits <= 0) {
                continue
            }
            items.push(row)
        }
        var mode = ipSortModes[ipSortIndex] ? String(ipSortModes[ipSortIndex].value || "latest") : "latest"
        items.sort(function(a, b) {
            var left = a || {}
            var right = b || {}
            if (mode === "most_hits") {
                var leftHits = Number(left.hitCount || 0)
                var rightHits = Number(right.hitCount || 0)
                if (leftHits !== rightHits) {
                    return rightHits - leftHits
                }
                return parseIsoMs(String(right.lastHitAt || right.createdAt || ""))
                        - parseIsoMs(String(left.lastHitAt || left.createdAt || ""))
            }
            if (mode === "target_asc") {
                return ipPolicyPrimaryTarget(left).localeCompare(ipPolicyPrimaryTarget(right))
            }
            return parseIsoMs(String(right.lastHitAt || right.createdAt || ""))
                    - parseIsoMs(String(left.lastHitAt || left.createdAt || ""))
        })
        return items
    }

    function connectionLabel(entry) {
        var row = entry || {}
        var identity = row.identity || {}
        var appName = String(identity.app_name || "Unknown App")
        var protocol = String(row.protocol || "net")
        var local = String(row.local || "-")
        var remote = String(row.remote || "-")
        return appName + " (" + protocol + ") " + local + " -> " + remote
    }

    function connectionDirection(entry) {
        var row = entry || {}
        var remote = String(row.remote || "")
        if (remote.indexOf("*:") === 0 || remote === "*") {
            return "incoming"
        }
        return "outgoing"
    }

    function promptRequestPayload() {
        var pid = Number(promptPidText)
        if (isNaN(pid)) {
            pid = -1
        }
        return {
            pid: pid,
            direction: connectionDirections[promptDirectionIndex].value
        }
    }

    function simulateEvaluateConnection() {
        var response = FirewallServiceClient.evaluateConnection(promptRequestPayload())
        var ok = Boolean(response && response.ok)
        var decision = String(response && response.decision || "unknown")
        var source = String(response && response.source || "")
        promptResultOk = ok
        promptResultText = ok
                ? qsTr("Decision: %1 (%2)").arg(decision).arg(source)
                : qsTr("Failed to evaluate connection.")
    }

    function applyPromptDecision(decision) {
        var ok = FirewallServiceClient.resolveConnectionDecision(
                    promptRequestPayload(),
                    decision,
                    promptRemember)
        promptResultOk = ok
        promptResultText = ok
                ? (promptRemember
                   ? qsTr("Decision saved as app policy.")
                   : qsTr("Decision applied once."))
                : qsTr("Failed to apply decision.")
    }

    function applyConnectionDecision(entry, decision) {
        var row = entry || {}
        var request = {
            pid: Number(row.pid || -1),
            direction: root.connectionDirection(row)
        }
        var ok = FirewallServiceClient.resolveConnectionDecision(
                    request,
                    decision,
                    root.connectionRemember)
        root.connectionResultOk = ok
        root.connectionResultText = ok
                ? (root.connectionRemember
                   ? qsTr("Connection decision saved as app policy.")
                   : qsTr("Connection decision applied once."))
                : qsTr("Failed to apply connection decision.")
        if (ok) {
            FirewallServiceClient.refreshAppPolicies()
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

            SettingCard {
                label: qsTr("Prompt Cooldown")
                description: qsTr("Anti-spam cooldown for repeated prompt decision (seconds)")

                RowLayout {
                    spacing: 8
                    Layout.fillWidth: true

                    SpinBox {
                        id: promptCooldownSpin
                        from: 1
                        to: 300
                        value: FirewallServiceClient.promptCooldownSeconds
                        editable: true
                        enabled: FirewallServiceClient.available && FirewallServiceClient.enabled
                        onValueModified: FirewallServiceClient.setPromptCooldownSeconds(value)
                    }

                    Text {
                        text: qsTr("%1 sec").arg(FirewallServiceClient.promptCooldownSeconds)
                        color: Theme.color("textSecondary")
                        font.pixelSize: Theme.fontSize("small")
                    }
                }
            }
        }

        SettingGroup {
            title: qsTr("Application Rules")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("Prompt Simulation")
                description: qsTr("Test evaluate + allow/deny flow and remember decision")
                visible: root.devPromptSimulationEnabled

                ColumnLayout {
                    spacing: 8
                    Layout.fillWidth: true

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        TextField {
                            Layout.preferredWidth: 160
                            placeholderText: qsTr("PID (default -1)")
                            text: root.promptPidText
                            enabled: FirewallServiceClient.available && FirewallServiceClient.enabled
                            onTextChanged: root.promptPidText = text
                        }

                        ComboBox {
                            Layout.fillWidth: true
                            model: root.connectionDirections.map(function(item) { return item.label })
                            currentIndex: root.promptDirectionIndex
                            enabled: FirewallServiceClient.available && FirewallServiceClient.enabled
                            onActivated: function(index) {
                                root.promptDirectionIndex = index
                            }
                        }

                        CheckBox {
                            text: qsTr("Remember")
                            checked: root.promptRemember
                            enabled: FirewallServiceClient.available && FirewallServiceClient.enabled
                            onToggled: root.promptRemember = checked
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Button {
                            text: qsTr("Evaluate")
                            enabled: FirewallServiceClient.available && FirewallServiceClient.enabled
                            onClicked: root.simulateEvaluateConnection()
                        }

                        Button {
                            text: qsTr("Allow")
                            enabled: FirewallServiceClient.available && FirewallServiceClient.enabled
                            onClicked: root.applyPromptDecision("allow")
                        }

                        Button {
                            text: qsTr("Deny")
                            enabled: FirewallServiceClient.available && FirewallServiceClient.enabled
                            onClicked: root.applyPromptDecision("deny")
                        }
                    }

                    Text {
                        visible: root.promptResultText.length > 0
                        text: root.promptResultText
                        color: root.promptResultOk ? Theme.color("success") : Theme.color("error")
                        font.pixelSize: Theme.fontSize("small")
                    }
                }
            }

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

                        Button {
                            text: qsTr("Quarantine")
                            enabled: FirewallServiceClient.available && FirewallServiceClient.enabled
                            onClicked: root.quarantineApp(root.appRuleAppId)
                        }

                        Button {
                            text: qsTr("Unquarantine")
                            enabled: FirewallServiceClient.available && FirewallServiceClient.enabled
                            onClicked: root.unquarantineApp(root.appRuleAppId)
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
                                    text: qsTr("Quarantine")
                                    enabled: FirewallServiceClient.available
                                             && FirewallServiceClient.enabled
                                             && String((modelData || {}).appId || "").length > 0
                                    onClicked: {
                                        var id = String((modelData || {}).appId || "")
                                        root.quarantineApp(id)
                                    }
                                }

                                Button {
                                    text: qsTr("Unquarantine")
                                    enabled: FirewallServiceClient.available
                                             && FirewallServiceClient.enabled
                                             && String((modelData || {}).appId || "").length > 0
                                    onClicked: {
                                        var id = String((modelData || {}).appId || "")
                                        root.unquarantineApp(id)
                                    }
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
                                    text: qsTr("Quarantine")
                                    enabled: FirewallServiceClient.available
                                             && FirewallServiceClient.enabled
                                             && String((modelData || {}).appId || "").length > 0
                                    onClicked: {
                                        var id = String((modelData || {}).appId || "")
                                        root.quarantineApp(id)
                                    }
                                }

                                Button {
                                    text: qsTr("Unquarantine")
                                    enabled: FirewallServiceClient.available
                                             && FirewallServiceClient.enabled
                                             && String((modelData || {}).appId || "").length > 0
                                    onClicked: {
                                        var id = String((modelData || {}).appId || "")
                                        root.unquarantineApp(id)
                                    }
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

                    TextField {
                        Layout.fillWidth: true
                        placeholderText: qsTr("Note (optional)")
                        text: root.blockNote
                        enabled: FirewallServiceClient.available && FirewallServiceClient.enabled
                        onTextChanged: root.blockNote = text
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

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            text: qsTr("Filter")
                            color: Theme.color("textSecondary")
                        }

                        ComboBox {
                            Layout.preferredWidth: 180
                            model: root.ipFilterModes.map(function(item) { return item.label })
                            currentIndex: root.ipFilterIndex
                            enabled: FirewallServiceClient.available
                            onActivated: function(index) {
                                root.ipFilterIndex = index
                            }
                        }

                        Item { Layout.fillWidth: true }

                        Label {
                            text: qsTr("Sort")
                            color: Theme.color("textSecondary")
                        }

                        ComboBox {
                            Layout.preferredWidth: 180
                            model: root.ipSortModes.map(function(item) { return item.label })
                            currentIndex: root.ipSortIndex
                            enabled: FirewallServiceClient.available
                            onActivated: function(index) {
                                root.ipSortIndex = index
                            }
                        }

                        Item { Layout.fillWidth: true }
                    }

                    Repeater {
                        model: root.sortedIpPolicies()

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
                                        var note = String(p.note || "")
                                        var temporary = Boolean(p.temporary)
                                        var duration = String(p.duration || "")
                                        var hitCount = Number(p.hitCount || 0)
                                        var lastHitAt = String(p.lastHitAt || "")
                                        var base = targets.length ? targets : qsTr("(empty)")
                                        var tail = " [" + scope + "]"
                                        if (reason.length) {
                                            tail += " - " + reason
                                        }
                                        if (note.length) {
                                            tail += " | " + qsTr("note: %1").arg(note)
                                        }
                                        if (temporary) {
                                            tail += " | " + qsTr("temporary %1").arg(duration.length ? duration : "1h")
                                        }
                                        tail += " | " + qsTr("hits: %1").arg(hitCount)
                                        if (lastHitAt.length) {
                                            tail += " | " + qsTr("last hit: %1").arg(lastHitAt)
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

        SettingGroup {
            title: qsTr("Active Connections")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("Live Network Activity")
                description: qsTr("Current app-to-network connections observed by desktop-firewalld")

                ColumnLayout {
                    spacing: 8
                    Layout.fillWidth: true

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("%1 active connection(s)").arg(FirewallServiceClient.activeConnections.length)
                            color: Theme.color("textSecondary")
                            font.pixelSize: Theme.fontSize("small")
                        }

                        Button {
                            text: qsTr("Refresh")
                            enabled: FirewallServiceClient.available
                            onClicked: FirewallServiceClient.refreshConnections()
                        }

                        CheckBox {
                            text: qsTr("Remember")
                            checked: root.connectionRemember
                            enabled: FirewallServiceClient.available && FirewallServiceClient.enabled
                            onToggled: root.connectionRemember = checked
                        }
                    }

                    Repeater {
                        model: FirewallServiceClient.activeConnections

                        delegate: Rectangle {
                            Layout.fillWidth: true
                            radius: Theme.radiusControl
                            color: Theme.color("surface")
                            border.width: Theme.borderWidthThin
                            border.color: Theme.color("panelBorder")
                            implicitHeight: connectionRow.implicitHeight + 10

                            RowLayout {
                                id: connectionRow
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 10

                                Text {
                                    Layout.fillWidth: true
                                    text: root.connectionLabel(modelData)
                                    color: Theme.color("textPrimary")
                                    font.pixelSize: Theme.fontSize("small")
                                    elide: Text.ElideRight
                                }

                                Button {
                                    text: qsTr("Allow")
                                    enabled: FirewallServiceClient.available && FirewallServiceClient.enabled
                                    onClicked: root.applyConnectionDecision(modelData, "allow")
                                }

                                Button {
                                    text: qsTr("Deny")
                                    enabled: FirewallServiceClient.available && FirewallServiceClient.enabled
                                    onClicked: root.applyConnectionDecision(modelData, "deny")
                                }
                            }
                        }
                    }

                    Text {
                        visible: root.connectionResultText.length > 0
                        text: root.connectionResultText
                        color: root.connectionResultOk ? Theme.color("success") : Theme.color("error")
                        font.pixelSize: Theme.fontSize("small")
                    }
                }
            }
        }
    }

    Component.onCompleted: {
        FirewallServiceClient.refresh()
        FirewallServiceClient.refreshAppPolicies()
        FirewallServiceClient.refreshIpPolicies()
        FirewallServiceClient.refreshConnections()
    }
}
