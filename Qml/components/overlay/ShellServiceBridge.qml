import QtQuick 2.15
import Slm_Desktop
import "../shell/TothespotController.js" as TothespotController
import "../shell/ShellUtils.js" as ShellUtils

Item {
    id: root

    required property var shellApi
    property var portalUiBridge: null
    property var portalChooserApi: null
    property var fileManagerApi: null
    property var tothespotService: null
    property var tothespotResultsModel: null
    property var desktopSettings: null
    property bool consentDialogVisible: false
    property string consentRequestPath: ""
    property var consentPayload: ({})

    visible: false

    function currentConsentReason() {
        var mediation = root.consentPayload && root.consentPayload.mediation
                ? root.consentPayload.mediation : ({})
        var reason = String((mediation && mediation.reason) ? mediation.reason : "")
        if (!reason.length) {
            reason = String((root.consentPayload && root.consentPayload.reason)
                            ? root.consentPayload.reason : "")
        }
        if (!reason.length) {
            reason = "consent-ui-unhandled"
        }
        return reason
    }

    function submitConsentDecision(decision, persist, scope, reason) {
        if (!root.portalUiBridge || !root.portalUiBridge.submitConsentResult) {
            return
        }
        var requestPath = String(root.consentRequestPath || "")
        if (!requestPath.length) {
            return
        }
        root.portalUiBridge.submitConsentResult(requestPath, {
            "decision": String(decision || "deny"),
            "persist": !!persist,
            "scope": String(scope || "session"),
            "reason": String(reason || root.currentConsentReason())
        })
        root.consentDialogVisible = false
        root.consentRequestPath = ""
        root.consentPayload = ({})
    }

    function acceptConsentOnce() {
        submitConsentDecision("allow_once", false, "session", "consent-approved")
    }

    function acceptConsentAlways() {
        submitConsentDecision("allow_always", true, "persistent", "consent-approved")
    }

    function denyConsent() {
        submitConsentDecision("deny", false, "session", "consent-denied")
    }

    function cancelConsent() {
        submitConsentDecision("cancelled", false, "session", "cancelled-by-user")
    }

    function openConsentSettings() {
        if (typeof AppExecutionGate === "undefined" || !AppExecutionGate) {
            return
        }
        var deepLink = "settings://permissions/app-secrets"
        var capability = String((root.consentPayload && root.consentPayload.capability)
                                ? root.consentPayload.capability : "")
        var capabilityLower = capability.toLowerCase()
        if (capabilityLower.indexOf("network") === 0
                || capabilityLower.indexOf("socket") >= 0
                || capabilityLower.indexOf("firewall") >= 0) {
            deepLink = "settings://firewall/mode"
        }
        var cmd = "slm-settings --deep-link " + deepLink
        var opened = AppExecutionGate.launchCommand(cmd, "", "portal-consent-open-settings")
        if (!opened && typeof AppBinaryDir !== "undefined" && String(AppBinaryDir || "").length > 0) {
            var localSettingsBin = String(AppBinaryDir) + "/slm-settings"
            opened = AppExecutionGate.launchCommand(localSettingsBin + " --deep-link " + deepLink,
                                                    "",
                                                    "portal-consent-open-settings-local")
        }
        if (!opened) {
            AppExecutionGate.launchDesktopId("slm-settings.desktop", "portal-consent-open-settings")
        }
    }

    Connections {
        target: root.portalUiBridge
        ignoreUnknownSignals: true
        function onFileChooserRequested(requestId, options) {
            if (root.portalChooserApi && root.portalChooserApi.openPortalFileChooser) {
                root.portalChooserApi.openPortalFileChooser(requestId, options)
            }
        }
        function onConsentRequested(requestPath, payload) {
            if (!root.portalUiBridge || !root.portalUiBridge.submitConsentResult) {
                return
            }
            var normalizedRequestPath = String(requestPath || "")
            if (!normalizedRequestPath.length) {
                return
            }
            if (root.consentDialogVisible) {
                root.portalUiBridge.submitConsentResult(normalizedRequestPath, {
                    "decision": "deny",
                    "persist": false,
                    "scope": "session",
                    "reason": "consent-ui-busy"
                })
                return
            }
            root.consentRequestPath = normalizedRequestPath
            root.consentPayload = payload || ({})
            root.consentDialogVisible = true
        }
    }

    Connections {
        target: root.fileManagerApi
        ignoreUnknownSignals: true
        function onStorageLocationsUpdated(locations) {
            if (!root.shellApi.portalFileChooserVisible) {
                return
            }
            if (root.portalChooserApi && root.portalChooserApi.portalChooserRefreshStoragePlaces) {
                root.portalChooserApi.portalChooserRefreshStoragePlaces()
            }
        }
    }

    Connections {
        target: root.tothespotService
        ignoreUnknownSignals: true
        function onSearchProfileChanged(profileId) {
            TothespotController.refreshProfileMeta(root.shellApi)
            TothespotController.refreshTelemetryMeta(root.shellApi)
            if (root.shellApi.tothespotVisible
                    && String(root.shellApi.tothespotQuery || "").trim().length >= 2) {
                TothespotController.refreshResults(root.shellApi, root.tothespotResultsModel, true)
            }
        }
    }

    Connections {
        target: root.desktopSettings
        ignoreUnknownSignals: true
        function onSettingChanged(path) {
            var k = String(path || "")
            if (k === "debug.verboseLogging") {
                root.shellApi.tothespotShowDebug = !!root.desktopSettings.settingValue(
                            "debug.verboseLogging", false)
            } else if (k === "motion.debugOverlay") {
                root.shellApi.motionDebugOverlayEnabled = !!root.desktopSettings.settingValue(
                            "motion.debugOverlay", false)
                ShellUtils.refreshMotionDebugRows(root.shellApi)
            } else if (k === "motion.timeScale") {
                root.shellApi.motionTimeScale = Number(root.desktopSettings.settingValue(
                                                           "motion.timeScale", 1.0))
                ShellUtils.applyMotionTimeScale(root.shellApi)
            } else if (k === "motion.reduced") {
                root.shellApi.motionReducedEnabled = !!root.desktopSettings.settingValue(
                            "motion.reduced", false)
                ShellUtils.applyMotionTimeScale(root.shellApi)
            } else if (k === "globalAppearance.uiScale") {
                Theme.userFontScale = ShellUtils.normalizedUserFontScale(
                            root.desktopSettings.settingValue("globalAppearance.uiScale", 1.0))
            } else if (k === "tothespot.notifyClipboardResolveSuccess") {
                root.shellApi.tothespotNotifyClipboardResolveSuccess = !!root.desktopSettings.settingValue(
                            "tothespot.notifyClipboardResolveSuccess", true)
            }
        }
    }

}
