import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../../../../../Qml/apps/settings/components"

Flickable {
    id: root
    anchors.fill: parent
    contentHeight: mainLayout.implicitHeight + 40
    clip: true

    property var grants: []
    property var filteredGrants: []
    property var groupedFilteredGrants: []
    property string query: ""
    property string policyTool: "apt"
    property string policyArgs: ""
    property var policyResult: ({})
    property var secretApps: []
    property var secretConsentSummary: []
    property bool secretBusy: false
    property string secretStatus: ""
    property string pendingRevokeAppId: ""
    property bool revokeAlsoClearSecrets: false

    function resultArray(value) {
        if (!value)
            return []
        if (Array.isArray(value))
            return value
        return []
    }

    function mapSize(value) {
        if (!value)
            return 0
        var keys = Object.keys(value)
        return keys ? keys.length : 0
    }

    function evaluatePolicyNow() {
        if (!ComponentHealth || !ComponentHealth.evaluatePackagePolicy) {
            policyResult = {
                "allowed": false,
                "error": "policy-ui-unavailable",
                "message": "Komponen policy checker tidak tersedia di halaman ini."
            }
            return
        }
        policyResult = ComponentHealth.evaluatePackagePolicy(policyTool, policyArgs)
    }

    function reloadSecretApps() {
        if (!SettingsApp || !SettingsApp.listSecretApps) {
            secretApps = []
            return
        }
        secretApps = SettingsApp.listSecretApps()
    }

    function clearSecretsForApp(appId) {
        if (!SettingsApp || !SettingsApp.clearSecretDataForApp) {
            secretStatus = qsTr("Secret cleanup action is unavailable in this build.")
            return
        }
        secretBusy = true
        const result = SettingsApp.clearSecretDataForApp(String(appId || ""))
        const ok = !!(result && result.ok)
        secretStatus = ok
                ? qsTr("Secret cleanup request submitted for %1.").arg(String(appId || ""))
                : qsTr("Failed to submit cleanup for %1.").arg(String(appId || ""))
        secretBusy = false
        reloadSecretApps()
        reloadSecretConsentSummary()
    }

    function reloadSecretConsentSummary() {
        if (!SettingsApp || !SettingsApp.listSecretConsentSummary) {
            secretConsentSummary = []
            return
        }
        secretConsentSummary = SettingsApp.listSecretConsentSummary()
    }

    function revokeSecretAccessForApp(appId) {
        if (!SettingsApp || !SettingsApp.revokeSecretConsentForApp) {
            secretStatus = qsTr("Secret access revoke action is unavailable in this build.")
            return
        }
        secretBusy = true
        const result = SettingsApp.revokeSecretConsentForApp(String(appId || ""))
        const ok = !!(result && result.ok)
        if (ok) {
            const revokeCount = Number(result.revokeCount || 0)
            secretStatus = qsTr("Revoked %1 stored access grants for %2.")
                    .arg(revokeCount)
                    .arg(String(appId || ""))
        } else {
            secretStatus = qsTr("Failed to revoke access grants for %1.").arg(String(appId || ""))
        }
        secretBusy = false
        reloadSecretApps()
        reloadSecretConsentSummary()
    }

    function revokeAndMaybeClearSecretsForApp(appId, alsoClearSecrets) {
        if (!SettingsApp || !SettingsApp.revokeSecretConsentForApp) {
            secretStatus = qsTr("Secret access revoke action is unavailable in this build.")
            return
        }
        const normalizedAppId = String(appId || "")
        secretBusy = true

        const revokeResult = SettingsApp.revokeSecretConsentForApp(normalizedAppId)
        const revokeOk = !!(revokeResult && revokeResult.ok)
        const revokeCount = Number((revokeResult && revokeResult.revokeCount) || 0)

        let clearOk = true
        if (alsoClearSecrets) {
            if (!SettingsApp || !SettingsApp.clearSecretDataForApp) {
                clearOk = false
            } else {
                const clearResult = SettingsApp.clearSecretDataForApp(normalizedAppId)
                clearOk = !!(clearResult && clearResult.ok)
            }
        }

        if (revokeOk && alsoClearSecrets && clearOk) {
            secretStatus = qsTr("Revoked %1 access grants and submitted secret cleanup for %2.")
                    .arg(revokeCount)
                    .arg(normalizedAppId)
        } else if (revokeOk && alsoClearSecrets && !clearOk) {
            secretStatus = qsTr("Revoked %1 access grants for %2, but failed to submit secret cleanup.")
                    .arg(revokeCount)
                    .arg(normalizedAppId)
        } else if (revokeOk) {
            secretStatus = qsTr("Revoked %1 stored access grants for %2.")
                    .arg(revokeCount)
                    .arg(normalizedAppId)
        } else {
            secretStatus = qsTr("Failed to revoke access grants for %1.").arg(normalizedAppId)
        }

        secretBusy = false
        reloadSecretApps()
        reloadSecretConsentSummary()
    }

    function requestRevokeSecretAccessForApp(appId) {
        pendingRevokeAppId = String(appId || "")
        revokeAlsoClearSecrets = false
        if (pendingRevokeAppId.trim().length === 0) {
            secretStatus = qsTr("Invalid app id.")
            return
        }
        revokeConfirmDialog.open()
    }

    function consentInfoForApp(appId) {
        const target = String(appId || "").toLowerCase()
        for (let i = 0; i < secretConsentSummary.length; ++i) {
            const row = secretConsentSummary[i]
            if (String(row.appId || "").toLowerCase() === target) {
                return row
            }
        }
        return ({ "grantCount": 0, "lastUpdatedIso": "" })
    }

    function reloadGrants() {
        grants = SettingsApp.listSettingGrants()
        applyFilter()
    }

    function applyFilter() {
        const q = String(query || "").trim().toLowerCase()
        if (q.length === 0) {
            filteredGrants = grants
            rebuildGroups()
            return
        }
        const rows = []
        for (let i = 0; i < grants.length; ++i) {
            const g = grants[i]
            const moduleId = String(g.moduleId || "").toLowerCase()
            const settingId = String(g.settingId || "").toLowerCase()
            const decision = String(g.decision || "").toLowerCase()
            const key = moduleId + "/" + settingId
            if (moduleId.indexOf(q) >= 0
                || settingId.indexOf(q) >= 0
                || decision.indexOf(q) >= 0
                || key.indexOf(q) >= 0) {
                rows.push(g)
            }
        }
        filteredGrants = rows
        rebuildGroups()
    }

    function rebuildGroups() {
        const grouped = {}
        for (let i = 0; i < filteredGrants.length; ++i) {
            const row = filteredGrants[i]
            const moduleId = String(row.moduleId || "")
            if (!grouped[moduleId]) {
                grouped[moduleId] = []
            }
            grouped[moduleId].push(row)
        }
        const keys = Object.keys(grouped).sort()
        const list = []
        for (let j = 0; j < keys.length; ++j) {
            const key = keys[j]
            list.push({ moduleId: key, entries: grouped[key] })
        }
        groupedFilteredGrants = list
    }

    Component.onCompleted: {
        reloadGrants()
        reloadSecretApps()
        reloadSecretConsentSummary()
    }

    onQueryChanged: applyFilter()
    onGrantsChanged: applyFilter()
    onFilteredGrantsChanged: rebuildGroups()

    Connections {
        target: SettingsApp
        function onGrantsChanged() {
            root.reloadGrants()
        }
    }

    ColumnLayout {
        id: mainLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 24
        anchors.top: parent.top
        anchors.topMargin: 24
        spacing: 20

        SettingGroup {
            title: qsTr("Permission Decisions")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("Stored Decisions")
                description: qsTr("Manage cached allow/deny decisions for privileged settings.")
                Layout.fillWidth: true

                RowLayout {
                    spacing: 10
                    TextField {
                        Layout.preferredWidth: 280
                        placeholderText: qsTr("Filter decisions")
                        text: root.query
                        onTextChanged: root.query = text
                    }
                    Button {
                        text: qsTr("Refresh")
                        onClicked: {
                            root.reloadGrants()
                            root.reloadSecretApps()
                        }
                    }
                    Button {
                        text: qsTr("Reset All")
                        enabled: root.grants.length > 0
                        onClicked: {
                            SettingsApp.clearAllSettingGrants()
                            root.reloadGrants()
                        }
                    }
                }
            }

            Repeater {
                model: root.groupedFilteredGrants
                delegate: ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 6

                    Label {
                        text: String(modelData.moduleId || "")
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: "#6b7280"
                    }

                    Repeater {
                        model: modelData.entries || []
                        delegate: SettingCard {
                            required property var modelData
                            Layout.fillWidth: true
                            label: {
                                const m = String(modelData.moduleId || "")
                                const s = String(modelData.settingId || "")
                                return m + "/" + s
                            }
                            description: {
                                const decisionText = modelData.decision === "allow-always"
                                                         ? qsTr("Always Allow")
                                                         : qsTr("Always Deny")
                                const updated = String(modelData.updatedAtIso || "")
                                if (updated.length === 0) {
                                    return decisionText
                                }
                                return decisionText + " • " + updated
                            }

                            RowLayout {
                                spacing: 8
                                Label {
                                    text: modelData.decision === "allow-always" ? qsTr("Allow") : qsTr("Deny")
                                    color: modelData.decision === "allow-always" ? "#1f8f4a" : "#b42318"
                                }
                                Button {
                                    text: qsTr("Open Setting")
                                    onClicked: SettingsApp.openModuleSetting(String(modelData.moduleId || ""),
                                                                             String(modelData.settingId || ""))
                                }
                                Button {
                                    text: qsTr("Reset")
                                    onClicked: {
                                        SettingsApp.clearSettingGrant(String(modelData.moduleId || ""),
                                                                      String(modelData.settingId || ""))
                                        root.reloadGrants()
                                    }
                                }
                            }
                        }
                    }
                }
            }

            SettingCard {
                visible: root.filteredGrants.length === 0
                Layout.fillWidth: true
                label: root.grants.length === 0
                           ? qsTr("No stored decisions")
                           : qsTr("No matching decisions")
                description: root.grants.length === 0
                                 ? qsTr("Authorization decisions set to Always Allow or Always Deny will appear here.")
                                 : qsTr("Try a different filter query.")
            }
        }

        SettingGroup {
            title: qsTr("App Secrets")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("Stored Secret Metadata")
                description: qsTr("Review app secret storage footprint and clear per-app data when needed.")
                Layout.fillWidth: true

                RowLayout {
                    spacing: 10
                    Button {
                        text: qsTr("Refresh")
                        onClicked: {
                            root.reloadSecretApps()
                            root.reloadSecretConsentSummary()
                        }
                    }
                    Label {
                        visible: root.secretBusy
                        text: qsTr("Working...")
                        color: "#6b7280"
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: String(root.secretStatus).length > 0
                        text: root.secretStatus
                        wrapMode: Text.Wrap
                        color: "#6b7280"
                    }
                }
            }

            Repeater {
                model: root.secretApps
                delegate: SettingCard {
                    required property var modelData
                    Layout.fillWidth: true
                    label: String(modelData.appId || "")
                    description: {
                        const count = Number(modelData.count || 0)
                        const updated = String(modelData.lastUpdatedIso || "")
                        const consent = root.consentInfoForApp(modelData.appId)
                        const grantCount = Number(consent.grantCount || 0)
                        if (updated.length === 0) {
                            return qsTr("%1 secrets • %2 access grants").arg(count).arg(grantCount)
                        }
                        return qsTr("%1 secrets • %2 access grants • last updated %3")
                                .arg(count)
                                .arg(grantCount)
                                .arg(updated)
                    }

                    RowLayout {
                        spacing: 8
                        Button {
                            text: qsTr("Revoke Access")
                            enabled: !root.secretBusy
                            onClicked: root.requestRevokeSecretAccessForApp(String(modelData.appId || ""))
                        }
                        Button {
                            text: qsTr("Clear Secrets")
                            enabled: !root.secretBusy
                            onClicked: root.clearSecretsForApp(String(modelData.appId || ""))
                        }
                    }
                }
            }

            SettingCard {
                visible: root.secretApps.length === 0
                Layout.fillWidth: true
                label: qsTr("No app secrets")
                description: qsTr("No secret metadata is currently available.")
            }
        }

        SettingGroup {
            title: qsTr("Package Policy Check")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("Simulate Package Transaction")
                description: qsTr("Periksa trust/risk/impact sebelum menjalankan install/remove paket.")
                Layout.fillWidth: true

                RowLayout {
                    spacing: 10

                    ComboBox {
                        Layout.preferredWidth: 120
                        model: ["apt", "apt-get", "dpkg"]
                        currentIndex: Math.max(0, model.indexOf(root.policyTool))
                        onActivated: function() {
                            root.policyTool = String(currentText || "apt")
                        }
                    }

                    TextField {
                        Layout.fillWidth: true
                        placeholderText: qsTr("Contoh: install samba atau remove libc6")
                        text: root.policyArgs
                        onTextChanged: root.policyArgs = text
                        onAccepted: root.evaluatePolicyNow()
                    }

                    Button {
                        text: qsTr("Evaluate")
                        enabled: root.policyArgs.trim().length > 0
                        onClicked: root.evaluatePolicyNow()
                    }
                }
            }

            SettingCard {
                Layout.fillWidth: true
                visible: !!root.policyResult && Object.keys(root.policyResult).length > 0
                label: qsTr("Policy Result")
                description: {
                    var msg = String((root.policyResult && root.policyResult.impactMessage)
                                     ? root.policyResult.impactMessage : "")
                    if (msg.length > 0)
                        return msg
                    var fallback = String((root.policyResult && root.policyResult.message)
                                          ? root.policyResult.message : "")
                    return fallback.length > 0 ? fallback : qsTr("No impact summary available.")
                }

                ColumnLayout {
                    spacing: 8

                    RowLayout {
                        spacing: 12
                        Label {
                            text: qsTr("Allowed: ") + (root.policyResult.allowed ? qsTr("Yes") : qsTr("No"))
                            color: root.policyResult.allowed ? "#1f8f4a" : "#b42318"
                        }
                        Label {
                            text: qsTr("Trust: ") + String(root.policyResult.trustLevel || "unknown")
                            color: Theme.color("textSecondary")
                        }
                        Label {
                            text: qsTr("Risk: ") + String(root.policyResult.riskLevel || "unknown")
                            color: String(root.policyResult.riskLevel || "") === "high"
                                   ? "#b42318"
                                   : (String(root.policyResult.riskLevel || "") === "medium" ? "#b54708" : Theme.color("textSecondary"))
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: resultArray(root.policyResult.blockReasons).length > 0
                        wrapMode: Text.Wrap
                        text: qsTr("Block reasons: ") + resultArray(root.policyResult.blockReasons).join(" | ")
                        color: "#b42318"
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: resultArray(root.policyResult.warnings).length > 0
                        wrapMode: Text.Wrap
                        text: qsTr("Warnings: ") + resultArray(root.policyResult.warnings).join(" | ")
                        color: "#b54708"
                    }

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        text: qsTr("Protected touched: remove=%1 replace=%2 downgrade=%3")
                              .arg(resultArray(root.policyResult.touchedProtectedRemove).length)
                              .arg(resultArray(root.policyResult.touchedProtectedReplace).length)
                              .arg(resultArray(root.policyResult.touchedProtectedDowngrade).length)
                        color: Theme.color("textSecondary")
                    }

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        text: qsTr("Package sources detected: %1").arg(root.mapSize(root.policyResult.packageSources))
                        color: Theme.color("textSecondary")
                    }
                }
            }
        }
    }

    Dialog {
        id: revokeConfirmDialog
        title: qsTr("Revoke Secret Access")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: Overlay.overlay

        contentItem: ColumnLayout {
            width: 360
            spacing: 10

            Label {
                Layout.fillWidth: true
                text: qsTr("Revoke stored secret access grants for %1?").arg(root.pendingRevokeAppId)
                wrapMode: Text.Wrap
            }
            CheckBox {
                Layout.fillWidth: true
                text: qsTr("Also clear stored secrets")
                checked: root.revokeAlsoClearSecrets
                onToggled: root.revokeAlsoClearSecrets = checked
            }
        }

        onAccepted: {
            root.revokeAndMaybeClearSecretsForApp(root.pendingRevokeAppId, root.revokeAlsoClearSecrets)
            root.pendingRevokeAppId = ""
            root.revokeAlsoClearSecrets = false
        }
        onRejected: {
            root.pendingRevokeAppId = ""
            root.revokeAlsoClearSecrets = false
        }
    }
}
