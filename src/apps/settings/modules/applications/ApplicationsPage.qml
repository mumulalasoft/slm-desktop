import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../../../../../Qml/apps/settings/components"

Flickable {
    id: root
    anchors.fill: parent
    contentHeight: mainLayout.implicitHeight + 40
    clip: true

    property string highlightSettingId: ""
    property var browserBinding: SettingsApp.createBindingFor("applications", "default-browser", "Google Chrome")
    property var mailBinding: SettingsApp.createBindingFor("applications", "default-mail", "Mail")
    property var browserPolicy: SettingsApp.settingPolicy("applications", "default-browser")
    property var mailPolicy: SettingsApp.settingPolicy("applications", "default-mail")
    property var browserGrantState: SettingsApp.settingGrantState("applications", "default-browser")
    property var mailGrantState: SettingsApp.settingGrantState("applications", "default-mail")
    property bool browserAuthorized: !Boolean(browserPolicy.requiresAuthorization)
    property bool mailAuthorized: !Boolean(mailPolicy.requiresAuthorization)
    property bool browserAuthPending: false
    property bool mailAuthPending: false
    property string browserAuthReason: ""
    property string mailAuthReason: ""

    ColumnLayout {
        id: mainLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 24
        anchors.top: parent.top
        anchors.topMargin: 24
        spacing: 20

        SettingGroup {
            title: qsTr("Default Applications")
            Layout.fillWidth: true

            SettingCard {
                objectName: "default-browser"
                highlighted: root.highlightSettingId === "default-browser"
                label: qsTr("Web Browser")
                description: qsTr("Used to open links.")
                Layout.fillWidth: true
                ComboBox {
                    id: browserCombo
                    model: ["Google Chrome", "Firefox", "Epiphany"]
                    currentIndex: Math.max(0, model.indexOf(String(root.browserBinding.value)))
                    Layout.preferredWidth: 240
                    enabled: !root.browserAuthPending
                    onActivated: {
                        if (Boolean(root.browserPolicy.requiresAuthorization) && !root.browserAuthorized) {
                            currentIndex = Math.max(0, model.indexOf(String(root.browserBinding.value)))
                            if (!root.browserAuthPending) {
                                root.browserAuthPending = true
                                SettingsApp.requestSettingAuthorization("applications", "default-browser")
                            }
                            return
                        }
                        root.browserBinding.value = currentText
                    }
                }
            }

            AuthorizationHint {
                requiresAuthorization: Boolean(root.browserPolicy.requiresAuthorization)
                authorized: root.browserAuthorized
                pending: root.browserAuthPending
                allowAlways: Boolean(root.browserGrantState.allowAlways)
                denyAlways: Boolean(root.browserGrantState.denyAlways)
                reason: root.browserAuthReason
                defaultMessage: "Permission required to change default browser."
                onAuthorizationDecision: function(decision) {
                    if (root.browserAuthPending) {
                        return
                    }
                    root.browserAuthPending = true
                    SettingsApp.requestSettingAuthorizationWithDecision("applications",
                                                                        "default-browser",
                                                                        decision)
                }
                onResetRequested: {
                    SettingsApp.clearSettingGrant("applications", "default-browser")
                    root.browserGrantState = SettingsApp.settingGrantState("applications", "default-browser")
                    root.browserAuthorized = !Boolean(root.browserPolicy.requiresAuthorization)
                    root.browserAuthReason = ""
                }
            }

            SettingCard {
                objectName: "default-mail"
                highlighted: root.highlightSettingId === "default-mail"
                label: qsTr("Mail Application")
                description: qsTr("Used to compose emails.")
                Layout.fillWidth: true
                ComboBox {
                    id: mailCombo
                    model: ["Mail", "Thunderbird", "Geary"]
                    currentIndex: Math.max(0, model.indexOf(String(root.mailBinding.value)))
                    Layout.preferredWidth: 240
                    enabled: !root.mailAuthPending
                    onActivated: {
                        if (Boolean(root.mailPolicy.requiresAuthorization) && !root.mailAuthorized) {
                            currentIndex = Math.max(0, model.indexOf(String(root.mailBinding.value)))
                            if (!root.mailAuthPending) {
                                root.mailAuthPending = true
                                SettingsApp.requestSettingAuthorization("applications", "default-mail")
                            }
                            return
                        }
                        root.mailBinding.value = currentText
                    }
                }
            }

            AuthorizationHint {
                requiresAuthorization: Boolean(root.mailPolicy.requiresAuthorization)
                authorized: root.mailAuthorized
                pending: root.mailAuthPending
                allowAlways: Boolean(root.mailGrantState.allowAlways)
                denyAlways: Boolean(root.mailGrantState.denyAlways)
                reason: root.mailAuthReason
                defaultMessage: "Permission required to change default mail application."
                onAuthorizationDecision: function(decision) {
                    if (root.mailAuthPending) {
                        return
                    }
                    root.mailAuthPending = true
                    SettingsApp.requestSettingAuthorizationWithDecision("applications",
                                                                        "default-mail",
                                                                        decision)
                }
                onResetRequested: {
                    SettingsApp.clearSettingGrant("applications", "default-mail")
                    root.mailGrantState = SettingsApp.settingGrantState("applications", "default-mail")
                    root.mailAuthorized = !Boolean(root.mailPolicy.requiresAuthorization)
                    root.mailAuthReason = ""
                }
            }
        }
    }

    Connections {
        target: SettingsApp
        function onSettingAuthorizationFinished(requestId, moduleId, settingId, allowed, reason) {
            if (moduleId !== "applications") {
                return
            }
            if (settingId === "default-browser") {
                root.browserAuthPending = false
                root.browserAuthReason = reason || ""
                root.browserGrantState = SettingsApp.settingGrantState("applications", "default-browser")
                if (allowed) {
                    root.browserAuthorized = true
                }
                browserCombo.currentIndex = Math.max(0, browserCombo.model.indexOf(String(root.browserBinding.value)))
            } else if (settingId === "default-mail") {
                root.mailAuthPending = false
                root.mailAuthReason = reason || ""
                root.mailGrantState = SettingsApp.settingGrantState("applications", "default-mail")
                if (allowed) {
                    root.mailAuthorized = true
                }
                mailCombo.currentIndex = Math.max(0, mailCombo.model.indexOf(String(root.mailBinding.value)))
            }
        }
    }
}
