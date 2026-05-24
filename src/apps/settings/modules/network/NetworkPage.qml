import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Slm_Desktop
import "file:/usr/lib/settings/components"

Flickable {
    id: root
    contentHeight: mainLayout.implicitHeight + 48
    clip: true

    property string highlightSettingId: ""

    // ── Settings bindings ────────────────────────────────────────────────────
    property var nmBinding:          SettingsApp.createBindingFor("network", "ethernet", "Disconnected")
    property var wifiEnabledBinding: SettingsApp.createBindingFor("network", "wifi", true)
    property var wifiPolicy:         SettingsApp.settingPolicy("network", "wifi")
    property var wifiGrantState:     SettingsApp.settingGrantState("network", "wifi")
    property bool wifiAuthorized:    !Boolean(wifiPolicy.requiresAuthorization)
    property bool wifiAuthPending:   false
    property bool wifiGuard:         false
    property string wifiAuthReason:  ""
    property var componentIssues:    []
    property bool hasBlockingIssues: false

    // ── Live NetworkManager (shell context property, safe-guarded) ────────────
    property var nm: (typeof NetworkManager !== "undefined") ? NetworkManager : null
    readonly property bool   isOnline:   !!nm && !!nm.online
    readonly property string connType:   nm ? String(nm.connectionType || "") : ""
    readonly property string connName:   nm ? String(nm.statusText      || "") : ""
    readonly property int    signalPct:  nm ? (nm.signalStrength || 0) : 0
    readonly property string ifaceName:  nm ? String(nm.interfaceName   || "") : ""
    readonly property string ipv4Addr:   nm ? String(nm.ipv4Address     || "") : ""
    readonly property bool   connSecure: !!nm && !!nm.activeConnectionSecure
    readonly property bool   hasNets:    nm ? !!nm.hasAvailableNetworks : false

    property var filteredNets: []

    // ── Helpers ──────────────────────────────────────────────────────────────
    function refreshComponentIssues() {
        if (typeof ComponentHealth === "undefined" || !ComponentHealth) {
            componentIssues = []; hasBlockingIssues = false; return
        }
        componentIssues = ComponentHealth.missingComponentsForDomain("network")
        if (ComponentHealth.hasBlockingMissingForDomain) {
            hasBlockingIssues = !!ComponentHealth.hasBlockingMissingForDomain("network")
        } else {
            hasBlockingIssues = (componentIssues || []).some(function(issue) {
                return String((issue || {}).severity || "required").toLowerCase() === "required"
            })
        }
    }

    function recomputeFilteredNets() {
        if (!root.nm || !root.nm.hasAvailableNetworks) { root.filteredNets = []; return }
        var nets = root.nm.availableNetworks
        var out = []
        for (var i = 0; i < nets.length && out.length < 5; ++i) {
            if (!nets[i].isActive) out.push(nets[i])
        }
        root.filteredNets = out
    }

    function signalLevel(pct) {
        if (pct >= 75) return "excellent"
        if (pct >= 50) return "good"
        if (pct >= 25) return "ok"
        if (pct > 0)   return "weak"
        return "none"
    }

    function wifiIcon(pct, secure) {
        var lvl = root.signalLevel(pct)
        return secure ? "network-wireless-signal-" + lvl + "-secure-symbolic"
                      : "network-wireless-signal-" + lvl + "-symbolic"
    }

    function iconSrc(name) {
        var n = String(name || "network-offline-symbolic")
        return "image://themeicon/" + n + "?v=" +
               ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                ? ThemeIconController.revision : 0)
    }

    readonly property string heroIconName: {
        if (!isOnline)              return "network-offline-symbolic"
        if (connType === "ethernet") return "network-wired-symbolic"
        if (connType === "vpn")      return "network-vpn-symbolic"
        if (connType === "wifi")     return root.wifiIcon(signalPct, connSecure)
        return "network-transmit-receive-symbolic"
    }

    readonly property string heroSubtitle: {
        if (!isOnline)               return qsTr("No connection")
        if (connType === "wifi")     return qsTr("Wi-Fi") + (signalPct > 0 ? "  ·  " + signalPct + "%" : "")
        if (connType === "ethernet") return qsTr("Ethernet")
        if (connType === "vpn")      return qsTr("VPN")
        return qsTr("Connected")
    }

    function refreshNm() {
        if (!root.nm) return
        root.nm.refresh()
        root.nm.refreshAvailableNetworks()
    }

    // ── Layout ───────────────────────────────────────────────────────────────
    ColumnLayout {
        id: mainLayout
        anchors.left:      parent.left
        anchors.right:     parent.right
        anchors.top:       parent.top
        anchors.margins:   24
        anchors.topMargin: 32
        spacing: 24

        // ── Component issue banner ─────────────────────────────────────────
        MissingComponentsCard {
            visible: (root.componentIssues || []).length > 0
            Layout.fillWidth: true
            issues: root.componentIssues
            message: qsTr("Networking component missing. Some network features are unavailable.")
            installHandler: function(id) { return ComponentHealth.installComponent(id) }
            onRefreshRequested: root.refreshComponentIssues()
        }

        // ── Hero status card ───────────────────────────────────────────────
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: heroRow.implicitHeight + 48
            Layout.bottomMargin: 4

            Rectangle {
                anchors.fill: parent
                radius: Theme.radiusWindow
                color: Qt.rgba(0.5, 0.5, 0.5, 0.05)
                border.color: Qt.rgba(0.5, 0.5, 0.5, 0.10)
                border.width: Theme.borderWidthThin
            }

            RowLayout {
                id: heroRow
                anchors.centerIn: parent
                spacing: 20

                Rectangle {
                    width: 72; height: 72
                    radius: Theme.radiusXxl
                    color: root.isOnline ? Qt.rgba(0.20, 0.50, 1.00, 0.10)
                                        : Qt.rgba(0.5, 0.5, 0.5, 0.07)
                    border.color: Qt.rgba(0.5, 0.5, 0.5, 0.14)
                    border.width: Theme.borderWidthThin

                    Image {
                        id: heroIcon
                        property var candidates: [root.heroIconName,
                                                   "network-wireless-symbolic",
                                                   "network-offline-symbolic"]
                        property int candidateIndex: 0
                        anchors.centerIn: parent
                        width: 34; height: 34
                        source: root.iconSrc(candidates[candidateIndex])
                        fillMode: Image.PreserveAspectFit
                        onStatusChanged: {
                            if (status === Image.Error && candidateIndex + 1 < candidates.length)
                                candidateIndex++
                        }
                        onCandidatesChanged: candidateIndex = 0
                    }
                }

                ColumnLayout {
                    spacing: 3

                    Text {
                        text: root.connName.length > 0 && root.connName !== "N/A"
                              ? root.connName
                              : (root.isOnline ? qsTr("Connected") : qsTr("Not Connected"))
                        font.pixelSize: Theme.fontSize("titleLarge")
                        font.weight: Theme.fontWeight("light")
                        color: Theme.color("textPrimary")
                        elide: Text.ElideRight
                    }

                    Text {
                        text: root.heroSubtitle
                        font.pixelSize: Theme.fontSize("body")
                        color: Theme.color("textSecondary")
                    }

                    Text {
                        visible: root.isOnline
                                 && root.ipv4Addr.length > 0
                                 && root.ipv4Addr !== "n/a"
                        text: root.ipv4Addr
                        font.pixelSize: Theme.fontSize("caption")
                        color: Theme.color("textTertiary")
                        topPadding: 2
                    }
                }
            }
        }

        // ── Wi-Fi ──────────────────────────────────────────────────────────
        SettingGroup {
            title: qsTr("Wi-Fi")
            Layout.fillWidth: true

            SettingCard {
                objectName: "wifi"
                highlighted: root.highlightSettingId === "wifi"
                label: qsTr("Wi-Fi")
                description: wifiToggle.checked
                    ? qsTr("Enabled via NetworkManager")
                    : qsTr("Disabled")
                Layout.fillWidth: true

                SettingToggle {
                    id: wifiToggle
                    checked: Boolean(root.wifiEnabledBinding.value)
                    enabled: !root.wifiAuthPending && !root.hasBlockingIssues
                    onToggled: {
                        if (root.wifiGuard) return
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
                        if (checked) root.refreshNm()
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
                defaultMessage: qsTr("Permission required to modify Wi-Fi state.")
                onAuthorizationDecision: function(decision) {
                    if (root.wifiAuthPending || root.hasBlockingIssues) return
                    root.wifiAuthPending = true
                    SettingsApp.requestSettingAuthorizationWithDecision("network", "wifi", decision)
                }
                onResetRequested: {
                    if (root.hasBlockingIssues) return
                    SettingsApp.clearSettingGrant("network", "wifi")
                    root.wifiGrantState = SettingsApp.settingGrantState("network", "wifi")
                    root.wifiAuthorized = !Boolean(root.wifiPolicy.requiresAuthorization)
                    root.wifiAuthReason = ""
                }
            }

            // Currently connected Wi-Fi network
            SettingCard {
                visible: wifiToggle.checked
                         && root.connType === "wifi"
                         && root.connName.length > 0
                         && root.connName !== "N/A"
                label: root.connName
                description: {
                    var parts = [qsTr("Connected")]
                    if (root.signalPct > 0) parts.push(root.signalPct + qsTr("% signal"))
                    if (root.connSecure)    parts.push(qsTr("Secured"))
                    return parts.join("  ·  ")
                }
                Layout.fillWidth: true

                Item {
                    width: 18; height: 14
                    Repeater {
                        model: 4
                        Rectangle {
                            width: 3; height: 5 + index * 3; radius: Theme.radiusHairline
                            x: index * 5
                            y: parent.height - height
                            color: index < Math.ceil(root.signalPct / 25)
                                   ? Theme.color("accent")
                                   : Qt.rgba(0.5, 0.5, 0.5, 0.25)
                        }
                    }
                }
            }

            // Available networks list
            ColumnLayout {
                visible: wifiToggle.checked && root.filteredNets.length > 0
                Layout.fillWidth: true
                Layout.topMargin: 4
                spacing: 2

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Available Networks")
                        font.pixelSize: Theme.fontSize("caption")
                        font.weight: Theme.fontWeight("medium")
                        color: Theme.color("textTertiary")
                        leftPadding: 4
                    }

                    ToolButton {
                        icon.source: root.iconSrc("view-refresh-symbolic")
                        icon.width: 14; icon.height: 14
                        padding: 4
                        onClicked: {
                            root.refreshNm()
                            root.recomputeFilteredNets()
                        }
                    }
                }

                Repeater {
                    model: root.filteredNets

                    delegate: Rectangle {
                        required property string ssid
                        required property int    signalStrength
                        required property bool   isSecure
                        required property bool   isActive

                        Layout.fillWidth: true
                        radius: Theme.radiusControl
                        color: netMa.containsMouse ? Qt.rgba(0.5, 0.5, 0.5, 0.08) : "transparent"
                        implicitHeight: netDelegRow.implicitHeight + 12

                        MouseArea {
                            id: netMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (!isSecure) {
                                    if (root.nm) root.nm.connectToNetwork(ssid, "", false)
                                    return
                                }
                                wifiPassDialog.targetSsid = ssid
                                wifiPassDialog.open()
                            }
                        }

                        RowLayout {
                            id: netDelegRow
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 10

                            Text {
                                Layout.fillWidth: true
                                text: ssid
                                font.pixelSize: Theme.fontSize("body")
                                color: Theme.color("textPrimary")
                                elide: Text.ElideRight
                            }

                            Text {
                                visible: isSecure
                                text: qsTr("Secured")
                                font.pixelSize: Theme.fontSize("xs")
                                color: Theme.color("textTertiary")
                            }

                            Item {
                                width: 18; height: 10
                                Layout.preferredWidth: 18
                                Layout.preferredHeight: 10
                                Layout.alignment: Qt.AlignVCenter
                                Repeater {
                                    model: 4
                                    Rectangle {
                                        width: 3; height: 4 + index * 2; radius: Theme.radiusHairline
                                        x: index * 5
                                        y: parent.height - height
                                        color: index < Math.ceil(signalStrength / 25)
                                               ? Theme.color("textSecondary")
                                               : Qt.rgba(0.5, 0.5, 0.5, 0.20)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Join hidden or unlisted network
            SettingCard {
                visible: wifiToggle.checked
                objectName: "other-network"
                label: qsTr("Other Network…")
                description: qsTr("Connect to a hidden or unlisted Wi-Fi network")
                Layout.fillWidth: true

                Button {
                    text: qsTr("Join…")
                    flat: true
                    enabled: !root.hasBlockingIssues && wifiToggle.checked
                    onClicked: {
                        wifiPassDialog.targetSsid = ""
                        wifiPassDialog.open()
                    }
                }
            }
        }

        // ── Ethernet ───────────────────────────────────────────────────────
        SettingGroup {
            title: qsTr("Ethernet")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("Status")
                description: root.connType === "ethernet" ? qsTr("Connected")
                           : root.isOnline                 ? qsTr("Active via Wi-Fi")
                                                           : qsTr("Not connected")
                Layout.fillWidth: true

                Rectangle {
                    width: 8; height: 8; radius: Theme.radiusSm
                    color: root.connType === "ethernet" ? Theme.color("success")
                         : root.isOnline                ? Theme.color("textTertiary")
                                                        : Theme.color("error")
                }
            }

            SettingCard {
                visible: root.ifaceName.length > 0 && root.ifaceName !== "n/a"
                label: qsTr("Interface")
                description: root.ifaceName
                Layout.fillWidth: true
            }

            SettingCard {
                visible: root.isOnline && root.ipv4Addr.length > 0 && root.ipv4Addr !== "n/a"
                label: qsTr("IP Address")
                description: root.ipv4Addr
                Layout.fillWidth: true
            }
        }

        // ── VPN ────────────────────────────────────────────────────────────
        SettingGroup {
            title: qsTr("VPN")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("No VPN Configured")
                description: qsTr("Add a VPN to securely connect to a private network.")
                Layout.fillWidth: true

                Button {
                    text: qsTr("Add VPN…")
                    flat: true
                    enabled: !root.hasBlockingIssues
                }
            }
        }

        // ── Proxy ──────────────────────────────────────────────────────────
        SettingGroup {
            title: qsTr("Proxy")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("Proxy")
                description: qsTr("No proxy configured.")
                Layout.fillWidth: true

                Button {
                    text: qsTr("Configure…")
                    flat: true
                    enabled: false
                }
            }
        }

        // ── Firewall ───────────────────────────────────────────────────────
        SettingGroup {
            title: qsTr("Firewall")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("Application Firewall")
                description: qsTr("App-aware firewall rules are in Security settings.")
                Layout.fillWidth: true

                Button {
                    text: qsTr("Open Firewall Settings")
                    flat: true
                    enabled: !root.hasBlockingIssues
                    onClicked: { if (SettingsApp) SettingsApp.openModuleSetting("firewall", "mode") }
                }
            }
        }
    }

    // ── Wi-Fi password dialog ────────────────────────────────────────────────
    Dialog {
        id: wifiPassDialog
        modal: true
        title: targetSsid.length > 0 ? (qsTr("Join") + " " + targetSsid) : qsTr("Join Network")
        standardButtons: Dialog.Cancel
        anchors.centerIn: Overlay.overlay

        property string targetSsid: ""
        property string errorText:  ""
        property bool   busy:       false

        onOpened: {
            wifiPassField.text  = ""
            wifiSsidField.text  = ""
            wifiPassDialog.errorText = ""
            wifiPassDialog.busy      = false
        }

        contentItem: ColumnLayout {
            spacing: 16

            // SSID field — shown only when joining "Other Network…"
            ColumnLayout {
                visible: wifiPassDialog.targetSsid.length === 0
                Layout.fillWidth: true
                spacing: 6

                Text {
                    text: qsTr("Network Name")
                    font.pixelSize: Theme.fontSize("body")
                    color: Theme.color("textSecondary")
                }

                TextField {
                    id: wifiSsidField
                    Layout.fillWidth: true
                    implicitWidth: 320
                    placeholderText: qsTr("SSID")
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    text: qsTr("Password")
                    font.pixelSize: Theme.fontSize("body")
                    color: Theme.color("textSecondary")
                }

                TextField {
                    id: wifiPassField
                    Layout.fillWidth: true
                    implicitWidth: 320
                    placeholderText: qsTr("Wi-Fi Password")
                    echoMode: showPassCheck.checked ? TextInput.Normal : TextInput.Password
                    onAccepted: wifiPassDialog.doJoin()
                }

                CheckBox {
                    id: showPassCheck
                    text: qsTr("Show password")
                }
            }

            Text {
                visible: wifiPassDialog.errorText.length > 0
                text: wifiPassDialog.errorText
                font.pixelSize: Theme.fontSize("caption")
                color: Theme.color("error")
                wrapMode: Text.Wrap
                Layout.preferredWidth: 320
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    text: wifiPassDialog.busy ? qsTr("Joining…") : qsTr("Join")
                    enabled: !wifiPassDialog.busy
                    onClicked: wifiPassDialog.doJoin()
                }
            }
        }

        function doJoin() {
            if (busy) return
            var ssid = targetSsid.length > 0 ? targetSsid : wifiSsidField.text.trim()
            if (ssid.length === 0) { errorText = qsTr("Network name is required."); return }
            if (!root.nm) { errorText = qsTr("Network service is unavailable."); return }
            busy = true; errorText = ""
            var result = root.nm.connectToNetwork(ssid, wifiPassField.text, true)
            busy = false
            if (result && result.ok) {
                close()
            } else {
                errorText = (result && result.message) ? String(result.message)
                                                       : qsTr("Could not join the network.")
            }
        }
    }

    // ── Connections ──────────────────────────────────────────────────────────
    Connections {
        target: SettingsApp
        function onSettingAuthorizationFinished(requestId, moduleId, settingId, allowed, reason) {
            if (moduleId !== "network" || settingId !== "wifi") return
            root.wifiAuthPending = false
            root.wifiAuthReason  = String(reason || "")
            root.wifiGrantState  = SettingsApp.settingGrantState("network", "wifi")
            if (allowed) root.wifiAuthorized = true
        }
    }

    Connections {
        target: root.nm
        function onAvailableNetworksChanged() { root.recomputeFilteredNets() }
        function onIsConnectedChanged()       { root.recomputeFilteredNets() }
    }

    Component.onCompleted: {
        refreshComponentIssues()
        if (root.nm) {
            root.nm.refresh()
            root.nm.refreshAvailableNetworks()
        }
    }
}
