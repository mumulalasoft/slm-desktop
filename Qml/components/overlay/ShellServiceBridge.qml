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
    property var uiPreferences: null
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
        target: root.uiPreferences
        ignoreUnknownSignals: true
        function onPreferenceChanged(key, value) {
            var k = String(key || "")
            if (k === "debug/verboseLogging" || k === "debug.verboseLogging") {
                root.shellApi.tothespotShowDebug = !!value
            } else if (k === "motion/debugOverlay" || k === "motion.debugOverlay") {
                root.shellApi.motionDebugOverlayEnabled = !!value
                ShellUtils.refreshMotionDebugRows(root.shellApi)
            } else if (k === "motion/timeScale" || k === "motion.timescale") {
                root.shellApi.motionTimeScale = Number(value)
                ShellUtils.applyMotionTimeScale(root.shellApi)
            } else if (k === "motion/reduced" || k === "motion.reduced") {
                root.shellApi.motionReducedEnabled = !!value
                ShellUtils.applyMotionTimeScale(root.shellApi)
            } else if (k === "ui/fontScale" || k === "ui.fontScale") {
                Theme.userFontScale = ShellUtils.normalizedUserFontScale(value)
            } else if (k === "tothespot/notifyClipboardResolveSuccess"
                       || k === "tothespot.notifyClipboardResolveSuccess") {
                root.shellApi.tothespotNotifyClipboardResolveSuccess = !!value
            }
        }
    }
}
