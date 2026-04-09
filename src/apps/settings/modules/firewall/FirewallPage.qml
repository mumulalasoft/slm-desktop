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
    property bool pendingPromptRemember: false
    property bool pendingShowSafestOnly: false
    property bool pendingSortByRisk: false
    property int pendingTriagePresetIndex: 1
    property int pendingConfirmPresetIndex: -1
    property string pendingConfirmPresetAction: ""
    property int quickBlockNowEpochSec: Math.floor(Date.now() / 1000)
    property int quickBlockLastRemainingSec: -1
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
    readonly property var pendingTriagePresets: [
        {
            label: qsTr("Conservative"),
            showSafestOnly: true,
            sortByRisk: true,
            action: "none"
        },
        {
            label: qsTr("Balanced"),
            showSafestOnly: false,
            sortByRisk: true,
            action: "deny-riskiest"
        },
        {
            label: qsTr("Aggressive"),
            showSafestOnly: false,
            sortByRisk: true,
            action: "deny-riskiest-then-allow-safest"
        }
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

    function isAppQuarantined(appId) {
        var id = String(appId || "").trim()
        if (!id.length) {
            return false
        }
        var all = FirewallServiceClient.appPolicies || []
        for (var i = 0; i < all.length; ++i) {
            var row = all[i] || {}
            if (String(row.appId || "") !== id) {
                continue
            }
            if (String(row.decision || "") === "deny" && String(row.direction || "") === "both") {
                return true
            }
        }
        return false
    }

    function quarantinedApps() {
        var out = []
        var seen = {}
        var all = FirewallServiceClient.appPolicies || []
        for (var i = 0; i < all.length; ++i) {
            var row = all[i] || {}
            if (String(row.decision || "") !== "deny" || String(row.direction || "") !== "both") {
                continue
            }
            var appId = String(row.appId || "").trim()
            if (!appId.length || seen[appId]) {
                continue
            }
            seen[appId] = true
            out.push({
                appId: appId,
                appName: String(row.appName || ""),
                policyId: String(row.policyId || "")
            })
        }
        return out
    }

    function clearQuarantineRules() {
        var all = FirewallServiceClient.appPolicies || []
        var targets = []
        for (var i = 0; i < all.length; ++i) {
            var row = all[i] || {}
            if (String(row.decision || "") !== "deny" || String(row.direction || "") !== "both") {
                continue
            }
            var policyId = String(row.policyId || "")
            if (policyId.length) {
                targets.push(policyId)
            }
        }

        if (targets.length === 0) {
            appResultOk = false
            appResultText = qsTr("No quarantine rules to clear.")
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
                ? qsTr("All quarantine rules cleared.")
                : qsTr("Failed to clear some quarantine rules.")
        FirewallServiceClient.refreshAppPolicies()
        return ok
    }

    function parseIsoMs(value) {
        var text = String(value || "").trim()
        if (!text.length) {
            return 0
        }
        var ms = Date.parse(text)
        return isNaN(ms) ? 0 : ms
    }

    function parseDurationSeconds(value) {
        var raw = String(value || "").trim().toLowerCase()
        if (!raw.length) {
            return 0
        }
        var match = raw.match(/^(\d+)\s*([smhd]?)$/)
        if (!match) {
            return 0
        }
        var amount = Number(match[1])
        if (isNaN(amount) || amount <= 0) {
            return 0
        }
        var unit = String(match[2] || "")
        if (unit === "m") {
            return amount * 60
        }
        if (unit === "h") {
            return amount * 3600
        }
        if (unit === "d") {
            return amount * 86400
        }
        return amount
    }

    function currentQuickBlockPolicy() {
        var id = String(FirewallServiceClient.lastQuickBlockPolicyId || "").trim()
        if (!id.length) {
            return null
        }
        var all = FirewallServiceClient.ipPolicies || []
        for (var i = 0; i < all.length; ++i) {
            var row = all[i] || {}
            if (String(row.policyId || "") === id) {
                return row
            }
        }
        return null
    }

    function quickBlockRemainingSeconds() {
        var policy = root.currentQuickBlockPolicy()
        if (!policy || !Boolean(policy.temporary)) {
            return -1
        }
        var createdAtMs = root.parseIsoMs(String(policy.createdAt || ""))
        if (createdAtMs <= 0) {
            return -1
        }
        var durationSec = root.parseDurationSeconds(String(policy.duration || "1h"))
        if (durationSec <= 0) {
            durationSec = 3600
        }
        var expirySec = Math.floor(createdAtMs / 1000) + durationSec
        var remaining = expirySec - root.quickBlockNowEpochSec
        return remaining > 0 ? remaining : 0
    }

    function formatRemainingSeconds(seconds) {
        var total = Number(seconds || 0)
        if (isNaN(total) || total <= 0) {
            return "0s"
        }
        var h = Math.floor(total / 3600)
        var m = Math.floor((total % 3600) / 60)
        var s = total % 60
        if (h > 0) {
            return String(h) + "h " + String(m) + "m"
        }
        if (m > 0) {
            return String(m) + "m " + String(s) + "s"
        }
        return String(s) + "s"
    }

    function quickBlockCountdownColor(remainingSeconds) {
        var remaining = Number(remainingSeconds)
        if (isNaN(remaining) || remaining < 0) {
            return Theme.color("textSecondary")
        }
        if (remaining <= 60) {
            return Theme.color("error")
        }
        if (remaining <= 300) {
            return Theme.color("warning")
        }
        return Theme.color("textSecondary")
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
        var scriptTarget = String(identity.script_target || "")
        var protocol = String(row.protocol || "net")
        var local = String(row.local || "-")
        var remote = String(row.remote || "-")
        var actor = scriptTarget.length ? scriptTarget : appName
        return actor + " (" + protocol + ") " + local + " -> " + remote
    }

    function connectionDirection(entry) {
        var row = entry || {}
        var remote = String(row.remote || "")
        if (remote.indexOf("*:") === 0 || remote === "*") {
            return "incoming"
        }
        return "outgoing"
    }

    function extractRemoteIp(entry) {
        var row = entry || {}
        var remote = String(row.remote || "").trim()
        if (!remote.length || remote === "*" || remote.indexOf("*:") === 0) {
            return ""
        }
        if (remote.indexOf("[") === 0) {
            var close = remote.indexOf("]")
            if (close > 1) {
                return remote.substring(1, close)
            }
            return ""
        }
        var colon = remote.lastIndexOf(":")
        if (colon <= 0) {
            return remote
        }
        return remote.substring(0, colon)
    }

    function ipv4Subnet24(ip) {
        var value = String(ip || "").trim()
        var parts = value.split(".")
        if (parts.length !== 4) {
            return ""
        }
        for (var i = 0; i < parts.length; ++i) {
            var n = Number(parts[i])
            if (isNaN(n) || n < 0 || n > 255) {
                return ""
            }
        }
        return parts[0] + "." + parts[1] + "." + parts[2] + ".0/24"
    }

    function quickBlockConnectionIp(entry, temporary) {
        var ip = root.extractRemoteIp(entry)
        if (!ip.length) {
            root.connectionResultOk = false
            root.connectionResultText = qsTr("Remote endpoint has no blockable IP.")
            return false
        }
        var identity = (entry || {}).identity || {}
        var appName = String(identity.app_name || "")
        var note = appName.length
                ? qsTr("from active connection: %1").arg(appName)
                : qsTr("from active connection")
        var payload = {
            type: "ip",
            ip: ip,
            scope: "both",
            reason: "active-connection-quick-block",
            note: note
        }
        if (temporary) {
            payload.temporary = true
            payload.duration = "1h"
        }
        var response = FirewallServiceClient.setIpPolicyDetailed(payload)
        var ok = Boolean(response && response.ok)
        var policy = response && response.policy ? response.policy : {}
        var policyId = String(policy.policyId || "")
        root.connectionResultOk = ok
        root.connectionResultText = ok
                ? (temporary
                   ? qsTr("Remote IP blocked for 1h: %1").arg(ip)
                   : qsTr("Remote IP blocked permanently: %1").arg(ip))
                : qsTr("Failed to block remote IP.")
        if (ok) {
            FirewallServiceClient.lastQuickBlockPolicyId = policyId
            FirewallServiceClient.lastQuickBlockTarget = ip
            FirewallServiceClient.refreshIpPolicies()
            FirewallServiceClient.refreshConnections()
        }
        return ok
    }

    function quickBlockConnectionSubnet24(entry) {
        var ip = root.extractRemoteIp(entry)
        if (!ip.length) {
            root.connectionResultOk = false
            root.connectionResultText = qsTr("Remote endpoint has no blockable IP.")
            return false
        }
        var cidr = root.ipv4Subnet24(ip)
        if (!cidr.length) {
            root.connectionResultOk = false
            root.connectionResultText = qsTr("Subnet quick block supports IPv4 endpoints only.")
            return false
        }
        var identity = (entry || {}).identity || {}
        var appName = String(identity.app_name || "")
        var note = appName.length
                ? qsTr("from active connection: %1").arg(appName)
                : qsTr("from active connection")
        var response = FirewallServiceClient.setIpPolicyDetailed({
            type: "subnet",
            cidr: cidr,
            scope: "both",
            reason: "active-connection-subnet-quick-block",
            note: note
        })
        var ok = Boolean(response && response.ok)
        var policy = response && response.policy ? response.policy : {}
        var policyId = String(policy.policyId || "")
        root.connectionResultOk = ok
        root.connectionResultText = ok
                ? qsTr("Remote subnet blocked: %1").arg(cidr)
                : qsTr("Failed to block remote subnet.")
        if (ok) {
            FirewallServiceClient.lastQuickBlockPolicyId = policyId
            FirewallServiceClient.lastQuickBlockTarget = cidr
            FirewallServiceClient.refreshIpPolicies()
            FirewallServiceClient.refreshConnections()
        }
        return ok
    }

    function undoLastQuickBlock() {
        var id = String(FirewallServiceClient.lastQuickBlockPolicyId || "").trim()
        if (!id.length) {
            root.connectionResultOk = false
            root.connectionResultText = qsTr("No quick block to undo.")
            return false
        }
        var ok = FirewallServiceClient.removeIpPolicy(id)
        root.connectionResultOk = ok
        root.connectionResultText = ok
                ? qsTr("Quick block removed: %1").arg(String(FirewallServiceClient.lastQuickBlockTarget || id))
                : qsTr("Failed to undo quick block.")
        if (ok) {
            FirewallServiceClient.lastQuickBlockPolicyId = ""
            FirewallServiceClient.lastQuickBlockTarget = ""
            FirewallServiceClient.refreshIpPolicies()
            FirewallServiceClient.refreshConnections()
        }
        return ok
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

    function evaluateTargetLabel(response) {
        var target = response && response.target ? response.target : {}
        var ip = String(target.ip || "")
        var host = String(target.host || "")
        var port = Number(target.port || 0)
        var endpoint = host.length ? host : ip
        if (!endpoint.length) {
            return ""
        }
        if (!isNaN(port) && port > 0) {
            return endpoint + ":" + String(port)
        }
        return endpoint
    }

    function evaluateActorLabel(response) {
        var actor = response && response.actor ? response.actor : {}
        var scriptTarget = String(actor.scriptTarget || "")
        var appName = String(actor.appName || "")
        var executable = String(actor.executable || "")
        if (scriptTarget.length) {
            return scriptTarget
        }
        if (appName.length) {
            return appName
        }
        if (executable.length) {
            return executable
        }
        return qsTr("Unknown actor")
    }

    function pendingPromptLabel(item) {
        var row = item || {}
        var evaluation = row.evaluation || {}
        var actor = root.evaluateActorLabel(evaluation)
        var target = root.evaluateTargetLabel(evaluation)
        var source = String(evaluation.source || "policy")
        if (target.length) {
            return actor + " -> " + target + " (" + source + ")"
        }
        return actor + " (" + source + ")"
    }

    function pendingPromptDetails(item) {
        var row = item || {}
        var request = row.request || {}
        var evaluation = row.evaluation || {}
        var duplicateCount = Number(row.duplicateCount || 0)
        var direction = String(request.direction || evaluation.direction || "unknown")
        var source = String(evaluation.source || "policy")
        var receivedAt = String(row.receivedAt || "")
        var receivedLabel = ""
        if (receivedAt.length) {
            var ms = Date.parse(receivedAt)
            if (!isNaN(ms)) {
                receivedLabel = new Date(ms).toLocaleString()
            } else {
                receivedLabel = receivedAt
            }
        }
        var text = qsTr("Direction: %1 • Source: %2").arg(direction).arg(source)
        if (receivedLabel.length) {
            text += "\n" + qsTr("Received: %1").arg(receivedLabel)
        }
        if (!isNaN(duplicateCount) && duplicateCount > 0) {
            text += "\n" + qsTr("Suppressed duplicate(s): %1").arg(duplicateCount)
        }
        text += "\n" + qsTr("Risk score: %1").arg(root.pendingPromptRiskScore(row))
        return text
    }

    function pendingPromptRemainingSeconds(item) {
        var row = item || {}
        var receivedAt = String(row.receivedAt || "").trim()
        if (!receivedAt.length) {
            return -1
        }
        var receivedMs = root.parseIsoMs(receivedAt)
        if (receivedMs <= 0) {
            return -1
        }
        var ttlSec = Number(FirewallServiceClient.pendingPromptTtlSeconds || 900)
        if (isNaN(ttlSec) || ttlSec <= 0) {
            ttlSec = 900
        }
        var elapsedSec = root.quickBlockNowEpochSec - Math.floor(receivedMs / 1000)
        if (elapsedSec < 0) {
            elapsedSec = 0
        }
        var remaining = ttlSec - elapsedSec
        return remaining > 0 ? remaining : 0
    }

    function isPrivateOrLocalIpv4(ip) {
        var raw = String(ip || "").trim()
        var parts = raw.split(".")
        if (parts.length !== 4) {
            return false
        }
        var o1 = Number(parts[0])
        var o2 = Number(parts[1])
        var o3 = Number(parts[2])
        var o4 = Number(parts[3])
        if (isNaN(o1) || isNaN(o2) || isNaN(o3) || isNaN(o4)) {
            return false
        }
        if (o1 < 0 || o1 > 255 || o2 < 0 || o2 > 255 || o3 < 0 || o3 > 255 || o4 < 0 || o4 > 255) {
            return false
        }
        if (o1 === 10) {
            return true
        }
        if (o1 === 172 && o2 >= 16 && o2 <= 31) {
            return true
        }
        if (o1 === 192 && o2 === 168) {
            return true
        }
        if (o1 === 127) {
            return true
        }
        if (o1 === 169 && o2 === 254) {
            return true
        }
        return false
    }

    function pendingPromptTargetIsLocal(item) {
        var row = item || {}
        var evaluation = row.evaluation || {}
        var request = row.request || {}
        var target = evaluation.target || {}
        var ip = String(target.ip || request.destinationIp || request.sourceIp || request.remoteIp || request.ip || "").trim()
        var host = String(target.host || request.destinationHost || request.host || "").trim().toLowerCase()
        if (ip.length && root.isPrivateOrLocalIpv4(ip)) {
            return true
        }
        if (host === "localhost" || host === "localhost.localdomain") {
            return true
        }
        return false
    }

    function pendingPromptSourceTrusted(item) {
        var row = item || {}
        var evaluation = row.evaluation || {}
        var identity = evaluation.identity || {}
        var source = String(evaluation.source || "").trim().toLowerCase()
        var trustLevel = String(identity.trust_level || "").trim().toLowerCase()
        if (trustLevel === "system" || trustLevel === "trusted") {
            return true
        }
        return source.indexOf("trusted") >= 0
    }

    function pendingPromptIsSafest(item) {
        return root.pendingPromptSourceTrusted(item) && root.pendingPromptTargetIsLocal(item)
    }

    function pendingPromptRiskScore(item) {
        var row = item || {}
        var evaluation = row.evaluation || {}
        var identity = evaluation.identity || {}
        var processKind = String(evaluation.processKind || "").trim().toLowerCase()
        var trustLevel = String(identity.trust_level || "").trim().toLowerCase()
        var source = String(evaluation.source || "").trim().toLowerCase()
        var duplicateCount = Number(row.duplicateCount || 0)
        var remainingSec = root.pendingPromptRemainingSeconds(row)
        var score = 0

        if (!root.pendingPromptSourceTrusted(row)) {
            score += 3
        }
        if (!root.pendingPromptTargetIsLocal(row)) {
            score += 3
        }

        if (trustLevel === "unknown") {
            score += 2
        } else if (trustLevel === "suspicious") {
            score += 4
        }

        if (processKind === "unknown") {
            score += 2
        } else if (processKind === "network_tools" || processKind === "interpreter") {
            score += 1
        }

        if (source.indexOf("default") >= 0 || source.indexOf("prompt") >= 0) {
            score += 1
        }

        if (!isNaN(duplicateCount) && duplicateCount > 0) {
            score += Math.min(3, duplicateCount)
        }

        if (remainingSec >= 0 && remainingSec <= 120) {
            score += 1
        }

        return score
    }

    function pendingPromptRiskTier(item) {
        var score = root.pendingPromptRiskScore(item)
        if (score >= 8) {
            return "high"
        }
        if (score >= 5) {
            return "medium"
        }
        return "low"
    }

    function pendingPromptRiskTierText(item) {
        var tier = root.pendingPromptRiskTier(item)
        if (tier === "high") {
            return qsTr("Risk: High")
        }
        if (tier === "medium") {
            return qsTr("Risk: Medium")
        }
        return qsTr("Risk: Low")
    }

    function pendingPromptRiskTierColor(item) {
        var tier = root.pendingPromptRiskTier(item)
        if (tier === "high") {
            return Theme.color("error")
        }
        if (tier === "medium") {
            return Theme.color("warning")
        }
        return Theme.color("success")
    }

    function pendingPromptIsRiskiest(item) {
        if (root.pendingPromptIsSafest(item)) {
            return false
        }
        return root.pendingPromptRiskScore(item) >= 6
    }

    function pendingSafestCount() {
        var rows = FirewallServiceClient.pendingPrompts || []
        var count = 0
        for (var i = 0; i < rows.length; ++i) {
            if (root.pendingPromptIsSafest(rows[i])) {
                count += 1
            }
        }
        return count
    }

    function pendingRiskiestCount() {
        var rows = FirewallServiceClient.pendingPrompts || []
        var count = 0
        for (var i = 0; i < rows.length; ++i) {
            if (root.pendingPromptIsRiskiest(rows[i])) {
                count += 1
            }
        }
        return count
    }

    function pendingPromptRows() {
        var source = FirewallServiceClient.pendingPrompts || []
        var out = []
        for (var i = 0; i < source.length; ++i) {
            var row = source[i] || {}
            var safest = root.pendingPromptIsSafest(row)
            if (root.pendingShowSafestOnly && !safest) {
                continue
            }
            out.push({
                sourceIndex: i,
                item: row,
                safest: safest
            })
        }
        if (root.pendingSortByRisk) {
            out.sort(function(left, right) {
                var l = left || {}
                var r = right || {}
                var leftItem = l.item || {}
                var rightItem = r.item || {}
                var leftRisk = root.pendingPromptRiskScore(leftItem)
                var rightRisk = root.pendingPromptRiskScore(rightItem)
                if (leftRisk !== rightRisk) {
                    return rightRisk - leftRisk
                }
                var leftRemaining = root.pendingPromptRemainingSeconds(leftItem)
                var rightRemaining = root.pendingPromptRemainingSeconds(rightItem)
                if (leftRemaining < 0) {
                    leftRemaining = 2147483647
                }
                if (rightRemaining < 0) {
                    rightRemaining = 2147483647
                }
                if (leftRemaining !== rightRemaining) {
                    return leftRemaining - rightRemaining
                }
                return Number(l.sourceIndex || 0) - Number(r.sourceIndex || 0)
            })
        }
        return out
    }

    function visiblePendingPromptCount() {
        return root.pendingPromptRows().length
    }

    function allowSafestPendingPrompts() {
        var rows = FirewallServiceClient.pendingPrompts || []
        var resolved = 0
        for (var i = rows.length - 1; i >= 0; --i) {
            if (!root.pendingPromptIsSafest(rows[i])) {
                continue
            }
            if (FirewallServiceClient.resolvePendingPrompt(i, "allow", root.pendingPromptRemember)) {
                resolved += 1
            }
        }
        root.connectionResultOk = resolved > 0
        root.connectionResultText = resolved > 0
                ? qsTr("Allowed %1 safest pending prompt(s).").arg(resolved)
                : qsTr("No safest pending prompt matched.")
        return resolved
    }

    function denyRiskiestPendingPrompts() {
        var rows = FirewallServiceClient.pendingPrompts || []
        var resolved = 0
        for (var i = rows.length - 1; i >= 0; --i) {
            if (!root.pendingPromptIsRiskiest(rows[i])) {
                continue
            }
            if (FirewallServiceClient.resolvePendingPrompt(i, "deny", root.pendingPromptRemember)) {
                resolved += 1
            }
        }
        root.connectionResultOk = resolved > 0
        root.connectionResultText = resolved > 0
                ? qsTr("Denied %1 riskiest pending prompt(s).").arg(resolved)
                : qsTr("No riskiest pending prompt matched.")
        return resolved
    }

    function applyPendingTriagePreset(index) {
        var idx = Number(index)
        if (isNaN(idx) || idx < 0 || idx >= root.pendingTriagePresets.length) {
            idx = 0
        }
        root.pendingTriagePresetIndex = idx
        var preset = root.pendingTriagePresets[idx] || {}
        root.pendingShowSafestOnly = Boolean(preset.showSafestOnly)
        root.pendingSortByRisk = Boolean(preset.sortByRisk)
    }

    function executePendingTriagePreset(index) {
        var idx = Number(index)
        if (isNaN(idx) || idx < 0 || idx >= root.pendingTriagePresets.length) {
            idx = root.pendingTriagePresetIndex
        }
        root.applyPendingTriagePreset(idx)
        var preset = root.pendingTriagePresets[idx] || {}
        var action = String(preset.action || "none")
        if (action === "none") {
            root.connectionResultOk = true
            root.connectionResultText = qsTr("Preset applied: %1").arg(String(preset.label || "Conservative"))
            return
        }
        if (action === "deny-riskiest") {
            root.denyRiskiestPendingPrompts()
            return
        }
        if (action === "deny-riskiest-then-allow-safest") {
            var denied = root.denyRiskiestPendingPrompts()
            var allowed = root.allowSafestPendingPrompts()
            root.connectionResultOk = denied > 0 || allowed > 0
            root.connectionResultText = root.connectionResultOk
                    ? qsTr("Aggressive triage applied: denied %1 riskiest, allowed %2 safest.")
                          .arg(denied)
                          .arg(allowed)
                    : qsTr("Aggressive triage found no matching pending prompt.")
            return
        }
        root.connectionResultOk = false
        root.connectionResultText = qsTr("Unsupported triage preset action.")
    }

    function triagePresetNeedsConfirmation(index) {
        var idx = Number(index)
        if (isNaN(idx) || idx < 0 || idx >= root.pendingTriagePresets.length) {
            idx = root.pendingTriagePresetIndex
        }
        var preset = root.pendingTriagePresets[idx] || {}
        return String(preset.action || "none") !== "none"
    }

    function requestExecutePendingTriagePreset() {
        var idx = root.pendingTriagePresetIndex
        root.applyPendingTriagePreset(idx)
        if (!root.triagePresetNeedsConfirmation(idx)) {
            root.executePendingTriagePreset(idx)
            return
        }
        var preset = root.pendingTriagePresets[idx] || {}
        root.pendingConfirmPresetIndex = idx
        root.pendingConfirmPresetAction = String(preset.action || "")
        triagePresetConfirmDialog.open()
    }

    function pendingPromptExpiryText(item) {
        var remainingSec = root.pendingPromptRemainingSeconds(item)
        if (remainingSec < 0) {
            return ""
        }
        return qsTr("Expires in: %1").arg(root.formatRemainingSeconds(remainingSec))
    }

    function pendingPromptCountdownColor(item) {
        var remainingSec = root.pendingPromptRemainingSeconds(item)
        if (remainingSec < 0) {
            return Theme.color("textSecondary")
        }
        if (remainingSec <= 60) {
            return Theme.color("error")
        }
        if (remainingSec <= 300) {
            return Theme.color("warning")
        }
        return Theme.color("textSecondary")
    }

    function pendingPromptCountdownWeight(item) {
        var remainingSec = root.pendingPromptRemainingSeconds(item)
        return remainingSec >= 0 && remainingSec <= 60
                ? Theme.fontWeight("medium")
                : Theme.fontWeight("normal")
    }

    function simulateEvaluateConnection() {
        var response = FirewallServiceClient.evaluateConnection(promptRequestPayload())
        var ok = Boolean(response && response.ok)
        var decision = String(response && response.decision || "unknown")
        var source = String(response && response.source || "")
        var actor = root.evaluateActorLabel(response)
        var target = root.evaluateTargetLabel(response)
        promptResultOk = ok
        promptResultText = ok
                ? (target.length
                   ? qsTr("Decision: %1 (%2)\nActor: %3\nTarget: %4").arg(decision).arg(source).arg(actor).arg(target)
                   : qsTr("Decision: %1 (%2)\nActor: %3").arg(decision).arg(source).arg(actor))
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
        var target = root.extractRemoteIp(row)
        if (!target.length) {
            target = String(row.remote || "-")
        }
        var ok = FirewallServiceClient.resolveConnectionDecision(
                    request,
                    decision,
                    root.connectionRemember)
        root.connectionResultOk = ok
        root.connectionResultText = ok
                ? (root.connectionRemember
                   ? qsTr("Connection decision saved for %1.").arg(target)
                   : qsTr("Connection decision applied once for %1.").arg(target))
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
                                        var quarantined = root.isAppQuarantined(appId)
                                        var suffix = quarantined ? " • " + qsTr("Quarantined") : ""
                                        return appId + " [" + direction + "]" + suffix
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
                                        var quarantined = root.isAppQuarantined(appId)
                                        var suffix = quarantined ? " • " + qsTr("Quarantined") : ""
                                        return appId + " [" + direction + "]" + suffix
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

            SettingCard {
                label: qsTr("Quarantined Apps")
                description: qsTr("Apps blocked for both incoming and outgoing traffic")

                ColumnLayout {
                    spacing: 8
                    Layout.fillWidth: true

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("%1 quarantined app(s)").arg(root.quarantinedApps().length)
                            color: Theme.color("textSecondary")
                            font.pixelSize: Theme.fontSize("small")
                        }

                        Button {
                            text: qsTr("Clear Quarantine Rules")
                            enabled: FirewallServiceClient.available
                                     && FirewallServiceClient.enabled
                                     && root.quarantinedApps().length > 0
                            onClicked: root.clearQuarantineRules()
                        }
                    }

                    Repeater {
                        model: root.quarantinedApps()

                        delegate: Rectangle {
                            Layout.fillWidth: true
                            radius: Theme.radiusControl
                            color: Theme.color("surface")
                            border.width: Theme.borderWidthThin
                            border.color: Theme.color("panelBorder")
                            implicitHeight: quarantineRow.implicitHeight + 10

                            RowLayout {
                                id: quarantineRow
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 10

                                Text {
                                    Layout.fillWidth: true
                                    text: {
                                        var p = modelData || {}
                                        var appId = String(p.appId || qsTr("(unknown app)"))
                                        var appName = String(p.appName || "")
                                        return appName.length ? appName + " (" + appId + ")" : appId
                                    }
                                    color: Theme.color("textPrimary")
                                    font.pixelSize: Theme.fontSize("small")
                                    elide: Text.ElideRight
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
                label: qsTr("Pending Prompts")
                description: qsTr("Incoming prompt requests that need allow/deny decision")

                ColumnLayout {
                    spacing: 8
                    Layout.fillWidth: true

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            Layout.fillWidth: true
                            text: root.pendingShowSafestOnly
                                  ? qsTr("%1 pending prompt(s) shown • %2 total")
                                        .arg(root.visiblePendingPromptCount())
                                        .arg(FirewallServiceClient.pendingPrompts.length)
                                  : qsTr("%1 pending prompt(s)").arg(FirewallServiceClient.pendingPrompts.length)
                            color: Theme.color("textSecondary")
                            font.pixelSize: Theme.fontSize("small")
                        }

                        CheckBox {
                            text: qsTr("Remember")
                            checked: root.pendingPromptRemember
                            enabled: FirewallServiceClient.available && FirewallServiceClient.enabled
                            onToggled: root.pendingPromptRemember = checked
                        }

                        CheckBox {
                            text: qsTr("Show safest only")
                            checked: root.pendingShowSafestOnly
                            enabled: FirewallServiceClient.available
                            onToggled: root.pendingShowSafestOnly = checked
                        }

                        CheckBox {
                            text: qsTr("Sort by risk")
                            checked: root.pendingSortByRisk
                            enabled: FirewallServiceClient.available
                            onToggled: root.pendingSortByRisk = checked
                        }

                        Button {
                            text: qsTr("Allow Safest")
                            enabled: FirewallServiceClient.available
                                     && FirewallServiceClient.enabled
                                     && root.pendingSafestCount() > 0
                            onClicked: root.allowSafestPendingPrompts()
                        }

                        Button {
                            text: qsTr("Deny Riskiest")
                            enabled: FirewallServiceClient.available
                                     && FirewallServiceClient.enabled
                                     && root.pendingRiskiestCount() > 0
                            onClicked: root.denyRiskiestPendingPrompts()
                        }

                        Button {
                            text: qsTr("Allow All")
                            enabled: FirewallServiceClient.available
                                     && FirewallServiceClient.enabled
                                     && FirewallServiceClient.pendingPrompts.length > 0
                            onClicked: {
                                var count = FirewallServiceClient.resolveAllPendingPrompts("allow",
                                                                                          root.pendingPromptRemember)
                                root.connectionResultOk = count > 0
                                root.connectionResultText = count > 0
                                        ? qsTr("Allowed %1 pending prompt(s).").arg(count)
                                        : qsTr("No pending prompt was allowed.")
                            }
                        }

                        Button {
                            text: qsTr("Deny All")
                            enabled: FirewallServiceClient.available
                                     && FirewallServiceClient.enabled
                                     && FirewallServiceClient.pendingPrompts.length > 0
                            onClicked: {
                                var count = FirewallServiceClient.resolveAllPendingPrompts("deny",
                                                                                          root.pendingPromptRemember)
                                root.connectionResultOk = count > 0
                                root.connectionResultText = count > 0
                                        ? qsTr("Denied %1 pending prompt(s).").arg(count)
                                        : qsTr("No pending prompt was denied.")
                            }
                        }

                        Button {
                            text: qsTr("Clear")
                            enabled: FirewallServiceClient.available
                                     && FirewallServiceClient.pendingPrompts.length > 0
                            onClicked: FirewallServiceClient.clearPendingPrompts()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            text: qsTr("Triage Preset")
                            color: Theme.color("textSecondary")
                        }

                        ComboBox {
                            Layout.preferredWidth: 190
                            model: root.pendingTriagePresets.map(function(item) { return item.label })
                            currentIndex: root.pendingTriagePresetIndex
                            enabled: FirewallServiceClient.available && FirewallServiceClient.enabled
                            onActivated: function(index) {
                                root.applyPendingTriagePreset(index)
                            }
                        }

                        Button {
                            text: qsTr("Apply Preset")
                            enabled: FirewallServiceClient.available && FirewallServiceClient.enabled
                            onClicked: root.requestExecutePendingTriagePreset()
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Preset can update filter/sort and optional batch action.")
                            color: Theme.color("textSecondary")
                            font.pixelSize: Theme.fontSize("small")
                            wrapMode: Text.WordWrap
                        }
                    }

                    Repeater {
                        model: root.pendingPromptRows()

                        delegate: Rectangle {
                            readonly property var rowData: modelData || {}
                            readonly property var promptItem: rowData.item || {}
                            readonly property int sourceIndex: Number(rowData.sourceIndex || -1)
                            Layout.fillWidth: true
                            radius: Theme.radiusControl
                            color: Theme.color("surface")
                            border.width: Theme.borderWidthThin
                            border.color: Theme.color("panelBorder")
                            implicitHeight: pendingRow.implicitHeight + 10

                            RowLayout {
                                id: pendingRow
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 10

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Text {
                                        Layout.fillWidth: true
                                        text: root.pendingPromptLabel(promptItem)
                                        color: Theme.color("textPrimary")
                                        font.pixelSize: Theme.fontSize("small")
                                        wrapMode: Text.WordWrap
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        visible: root.pendingPromptIsSafest(promptItem)
                                        text: qsTr("Safe candidate")
                                        color: Theme.color("success")
                                        font.pixelSize: Theme.fontSize("small")
                                        font.weight: Theme.fontWeight("medium")
                                        wrapMode: Text.WordWrap
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: root.pendingPromptRiskTierText(promptItem)
                                        color: root.pendingPromptRiskTierColor(promptItem)
                                        font.pixelSize: Theme.fontSize("small")
                                        font.weight: Theme.fontWeight("medium")
                                        wrapMode: Text.WordWrap
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: root.pendingPromptDetails(promptItem)
                                        color: Theme.color("textSecondary")
                                        font.pixelSize: Theme.fontSize("small")
                                        wrapMode: Text.WordWrap
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        visible: root.pendingPromptRemainingSeconds(promptItem) >= 0
                                        text: root.pendingPromptExpiryText(promptItem)
                                        color: root.pendingPromptCountdownColor(promptItem)
                                        font.pixelSize: Theme.fontSize("small")
                                        font.weight: root.pendingPromptCountdownWeight(promptItem)
                                        wrapMode: Text.WordWrap
                                    }
                                }

                                Button {
                                    text: qsTr("Allow")
                                    enabled: FirewallServiceClient.available
                                             && FirewallServiceClient.enabled
                                             && sourceIndex >= 0
                                    onClicked: {
                                        var ok = FirewallServiceClient.resolvePendingPrompt(sourceIndex, "allow", root.pendingPromptRemember)
                                        root.connectionResultOk = ok
                                        root.connectionResultText = ok
                                                ? qsTr("Pending prompt allowed.")
                                                : qsTr("Failed to resolve pending prompt.")
                                    }
                                }

                                Button {
                                    text: qsTr("Deny")
                                    enabled: FirewallServiceClient.available
                                             && FirewallServiceClient.enabled
                                             && sourceIndex >= 0
                                    onClicked: {
                                        var ok = FirewallServiceClient.resolvePendingPrompt(sourceIndex, "deny", root.pendingPromptRemember)
                                        root.connectionResultOk = ok
                                        root.connectionResultText = ok
                                                ? qsTr("Pending prompt denied.")
                                                : qsTr("Failed to resolve pending prompt.")
                                    }
                                }
                            }
                        }
                    }
                }
            }

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

                                ToolButton {
                                    text: qsTr("Block IP")
                                    enabled: FirewallServiceClient.available
                                             && FirewallServiceClient.enabled
                                             && root.extractRemoteIp(modelData).length > 0
                                    onClicked: blockIpMenu.open()

                                    Menu {
                                        id: blockIpMenu

                                        MenuItem {
                                            text: qsTr("Block 1h")
                                            onTriggered: root.quickBlockConnectionIp(modelData, true)
                                        }

                                        MenuItem {
                                            text: qsTr("Block Permanent")
                                            onTriggered: root.quickBlockConnectionIp(modelData, false)
                                        }

                                        MenuItem {
                                            text: qsTr("Block Subnet (/24)")
                                            onTriggered: root.quickBlockConnectionSubnet24(modelData)
                                        }
                                    }
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

                    Button {
                        text: qsTr("Undo Last Quick Block")
                        enabled: FirewallServiceClient.available
                                 && FirewallServiceClient.enabled
                                 && FirewallServiceClient.lastQuickBlockPolicyId.length > 0
                        onClicked: root.undoLastQuickBlock()
                    }

                    Text {
                        visible: {
                            var remaining = root.quickBlockRemainingSeconds()
                            return FirewallServiceClient.lastQuickBlockPolicyId.length > 0 && remaining >= 0
                        }
                        text: qsTr("Quick block expires in %1")
                              .arg(root.formatRemainingSeconds(root.quickBlockRemainingSeconds()))
                        color: root.quickBlockCountdownColor(root.quickBlockRemainingSeconds())
                        font.pixelSize: Theme.fontSize("small")
                        font.weight: root.quickBlockRemainingSeconds() <= 60
                                     ? Theme.fontWeight("medium")
                                     : Theme.fontWeight("normal")
                        Layout.fillWidth: true
                    }

                    Text {
                        visible: FirewallServiceClient.lastQuickBlockPolicyId.length === 0
                                 && FirewallServiceClient.quickBlockUndoNotice.length > 0
                        text: FirewallServiceClient.quickBlockUndoNotice
                        color: Theme.color("textSecondary")
                        font.pixelSize: Theme.fontSize("small")
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }
        }
    }

    Dialog {
        id: triagePresetConfirmDialog
        modal: true
        title: qsTr("Apply Triage Preset?")
        standardButtons: Dialog.Ok | Dialog.Cancel

        contentItem: ColumnLayout {
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: {
                    var preset = root.pendingTriagePresets[root.pendingConfirmPresetIndex] || {}
                    return qsTr("Preset \"%1\" will run batch action on pending prompts. Continue?")
                            .arg(String(preset.label || ""))
                }
                wrapMode: Text.WordWrap
                color: Theme.color("textPrimary")
                font.pixelSize: Theme.fontSize("small")
            }

            Text {
                Layout.fillWidth: true
                text: {
                    var action = String(root.pendingConfirmPresetAction || "")
                    if (action === "deny-riskiest") {
                        return qsTr("Action: deny riskiest prompts.")
                    }
                    if (action === "deny-riskiest-then-allow-safest") {
                        return qsTr("Action: deny riskiest prompts, then allow safest prompts.")
                    }
                    return qsTr("Action: update triage view only.")
                }
                wrapMode: Text.WordWrap
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("small")
            }
        }

        onAccepted: {
            if (root.pendingConfirmPresetIndex >= 0) {
                root.executePendingTriagePreset(root.pendingConfirmPresetIndex)
            }
            root.pendingConfirmPresetIndex = -1
            root.pendingConfirmPresetAction = ""
        }

        onRejected: {
            root.pendingConfirmPresetIndex = -1
            root.pendingConfirmPresetAction = ""
        }
    }

    Component.onCompleted: {
        root.applyPendingTriagePreset(root.pendingTriagePresetIndex)
        FirewallServiceClient.refresh()
        FirewallServiceClient.refreshAppPolicies()
        FirewallServiceClient.refreshIpPolicies()
        FirewallServiceClient.refreshConnections()
        root.quickBlockLastRemainingSec = root.quickBlockRemainingSeconds()
    }

    Timer {
        interval: 1000
        repeat: true
        running: true
        onTriggered: {
            root.quickBlockNowEpochSec = Math.floor(Date.now() / 1000)
            var remaining = root.quickBlockRemainingSeconds()
            if (root.quickBlockLastRemainingSec > 0
                    && remaining === 0
                    && FirewallServiceClient.lastQuickBlockPolicyId.length > 0) {
                FirewallServiceClient.refreshIpPolicies()
                FirewallServiceClient.refreshConnections()
            }
            if (root.quickBlockNowEpochSec % 5 === 0) {
                FirewallServiceClient.prunePendingPrompts()
            }
            root.quickBlockLastRemainingSec = remaining
        }
    }
}
