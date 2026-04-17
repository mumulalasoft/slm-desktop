import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import SlmStyle as DSStyle

AppDialog {
    id: root

    required property var hostRoot

    property bool rememberDecision: true
    property bool onlyLocal: false
    property string errorText: ""

    readonly property var pendingRows: (typeof FirewallServiceClient !== "undefined" && FirewallServiceClient)
                                       ? (FirewallServiceClient.pendingPrompts || [])
                                       : []
    readonly property int pendingCount: pendingRows.length
    readonly property var promptItem: pendingCount > 0 ? pendingRows[0] : ({})
    readonly property var request: promptItem && promptItem.request ? promptItem.request : ({})
    readonly property var evaluation: promptItem && promptItem.evaluation ? promptItem.evaluation : ({})
    readonly property var actor: evaluation && evaluation.actor ? evaluation.actor : ({})
    readonly property var target: evaluation && evaluation.target ? evaluation.target : ({})
    readonly property string promptKey: JSON.stringify(request) + "::" + JSON.stringify(evaluation)
    readonly property string normalizedDirection: {
        var raw = String(request.direction || evaluation.direction || "incoming").toLowerCase()
        return raw.length ? raw : "incoming"
    }
    readonly property bool incomingPrompt: normalizedDirection === "incoming"
    readonly property bool outgoingPrompt: normalizedDirection === "outgoing"

    title: incomingPrompt
           ? qsTr("Incoming Connection Request")
           : qsTr("Outgoing Connection Request")
    standardButtons: Dialog.NoButton
    dialogWidth: 520
    bodyPadding: 14
    footerPadding: 12
    closePolicy: Popup.NoAutoClose
    modal: false

    visible: !!hostRoot
             && hostRoot.visible
             && !hostRoot.lockScreenVisible
             && (typeof FirewallServiceClient !== "undefined")
             && FirewallServiceClient
             && FirewallServiceClient.available
             && FirewallServiceClient.enabled
             && pendingCount > 0

    x: Math.max(16, Math.round(hostRoot.width - width - 16))
    y: Math.max(16, Number(hostRoot.shellContextMenuHeight || 40) + 22)

    onPromptKeyChanged: {
        onlyLocal = false
        errorText = ""
    }

    function actorLabel() {
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
        return qsTr("Unknown app")
    }

    function endpointLabel(withPort) {
        var host = String(target.host || request.destinationHost || request.host || "")
        var ip = String(target.ip || request.destinationIp || request.remoteIp || request.ip || "")
        var port = Number(target.port || request.destinationPort || request.port || 0)
        var endpoint = host.length ? host : ip
        if (!endpoint.length) {
            return qsTr("unknown destination")
        }
        if (withPort && !isNaN(port) && port > 0) {
            return endpoint + ":" + String(port)
        }
        return endpoint
    }

    function targetIp() {
        var ip = String(target.ip || request.destinationIp || request.remoteIp || request.ip || "").trim()
        return ip
    }

    function requestDetails() {
        var direction = normalizedDirection
        var context = String(actor.context || "")
        var source = String(evaluation.source || "")
        var lines = [qsTr("Direction: %1").arg(direction)]
        if (context.length) {
            lines.push(qsTr("Context: %1").arg(context))
        }
        if (source.length) {
            lines.push(qsTr("Policy source: %1").arg(source))
        }
        return lines.join("  •  ")
    }

    function resolve(decision) {
        if (!(typeof FirewallServiceClient !== "undefined" && FirewallServiceClient)) {
            errorText = qsTr("Firewall service unavailable.")
            return
        }
        var allow = String(decision || "").toLowerCase() === "allow"
        var ok = FirewallServiceClient.resolvePendingPrompt(0,
                                                            allow ? "allow" : "deny",
                                                            rememberDecision,
                                                            allow && onlyLocal)
        if (!ok) {
            errorText = qsTr("Failed to submit decision.")
        } else {
            errorText = ""
        }
    }

    function blockTargetIp() {
        if (!(typeof FirewallServiceClient !== "undefined" && FirewallServiceClient)) {
            errorText = qsTr("Firewall service unavailable.")
            return
        }
        var ip = targetIp()
        if (!ip.length) {
            errorText = qsTr("No blockable IP target.")
            return
        }
        var response = FirewallServiceClient.setIpPolicyDetailed({
            type: "ip",
            ip: ip,
            scope: "both",
            reason: "runtime-prompt-target-block",
            temporary: false,
            duration: "",
            note: qsTr("from runtime prompt")
        })
        var ok = Boolean(response && response.ok)
        if (!ok) {
            errorText = qsTr("Failed to block target IP.")
            return
        }
        FirewallServiceClient.resolvePendingPrompt(0, "deny", false, false)
        FirewallServiceClient.refreshIpPolicies()
        errorText = ""
    }

    function openFirewallSettings() {
        if (typeof AppExecutionGate === "undefined" || !AppExecutionGate) {
            return
        }
        var deepLink = "settings://firewall/pending-prompts"
        var cmd = "slm-settings --deep-link " + deepLink
        var opened = AppExecutionGate.launchCommand(cmd, "", "firewall-runtime-prompt-open-settings")
        if (!opened && typeof AppBinaryDir !== "undefined" && String(AppBinaryDir || "").length > 0) {
            var localSettingsBin = String(AppBinaryDir) + "/slm-settings"
            opened = AppExecutionGate.launchCommand(localSettingsBin + " --deep-link " + deepLink,
                                                    "",
                                                    "firewall-runtime-prompt-open-settings-local")
        }
        if (!opened) {
            AppExecutionGate.launchDesktopId("slm-settings.desktop", "firewall-runtime-prompt-open-settings")
        }
    }

    bodyComponent: Component {
        ColumnLayout {
            spacing: 8
            implicitHeight: 170

            DSStyle.Label {
                Layout.fillWidth: true
                text: root.incomingPrompt
                      ? qsTr("%1 wants to receive a connection.").arg(root.actorLabel())
                      : qsTr("%1 wants to connect to %2.")
                            .arg(root.actorLabel())
                            .arg(root.endpointLabel(true))
                color: Theme.color("textPrimary")
                font.pixelSize: Theme.fontSize("body")
                font.weight: Theme.fontWeight("semibold")
                wrapMode: Text.WordWrap
            }

            DSStyle.Label {
                Layout.fillWidth: true
                text: root.incomingPrompt
                      ? qsTr("Target: %1").arg(root.endpointLabel(false))
                      : qsTr("Endpoint: %1").arg(root.endpointLabel(true))
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("small")
                wrapMode: Text.WordWrap
            }

            DSStyle.Label {
                Layout.fillWidth: true
                text: root.requestDetails()
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("small")
                wrapMode: Text.WordWrap
            }

            DSStyle.Label {
                Layout.fillWidth: true
                visible: root.pendingCount > 1
                text: qsTr("%1 more pending request(s)").arg(root.pendingCount - 1)
                color: Theme.color("warning")
                font.pixelSize: Theme.fontSize("small")
                wrapMode: Text.WordWrap
            }

            DSStyle.CheckBox {
                Layout.fillWidth: true
                text: qsTr("Remember this decision")
                checked: root.rememberDecision
                onToggled: root.rememberDecision = checked
            }

            DSStyle.CheckBox {
                Layout.fillWidth: true
                text: qsTr("Only local network")
                checked: root.onlyLocal
                enabled: root.visible && root.incomingPrompt
                visible: root.incomingPrompt
                onToggled: root.onlyLocal = checked
            }

            DSStyle.Label {
                Layout.fillWidth: true
                visible: root.errorText.length > 0
                text: root.errorText
                color: Theme.color("error")
                font.pixelSize: Theme.fontSize("small")
                wrapMode: Text.WordWrap
            }
        }
    }

    footerComponent: Component {
        RowLayout {
            spacing: 8

            DSStyle.Button {
                text: qsTr("Open Settings")
                onClicked: root.openFirewallSettings()
            }

            Item { Layout.fillWidth: true }

            DSStyle.Button {
                text: root.outgoingPrompt ? qsTr("Block") : qsTr("Deny")
                onClicked: root.resolve("deny")
            }

            DSStyle.Button {
                visible: root.outgoingPrompt && root.targetIp().length > 0
                text: qsTr("Block IP")
                onClicked: root.blockTargetIp()
            }

            DSStyle.Button {
                text: qsTr("Allow")
                defaultAction: true
                onClicked: root.resolve("allow")
            }
        }
    }
}
