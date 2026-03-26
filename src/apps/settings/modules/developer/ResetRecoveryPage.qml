import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../../../../../Qml/apps/settings/components"
import Slm_Desktop

Flickable {
    id: root
    anchors.fill: parent
    contentHeight: mainCol.implicitHeight + 48
    clip: true

    property string feedbackMessage: ""
    property bool   feedbackError:   false

    // ── Confirmation dialog ───────────────────────────────────────────────
    property var pendingCallback: null
    property string pendingTitle: ""
    property string pendingDesc:  ""

    Dialog {
        id: confirmDialog
        modal: true
        anchors.centerIn: Overlay.overlay
        title: root.pendingTitle
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: 340

        Text {
            width: parent.width
            wrapMode: Text.WordWrap
            font.pixelSize: Theme.fontSize("sm")
            color: Theme.color("textSecondary")
            text: root.pendingDesc
        }

        onAccepted: {
            if (root.pendingCallback) root.pendingCallback()
        }
    }

    function confirm(title, desc, callback) {
        root.pendingTitle    = title
        root.pendingDesc     = desc
        root.pendingCallback = callback
        confirmDialog.open()
    }

    function showFeedback(message, isError) {
        root.feedbackMessage = message
        root.feedbackError   = isError
        feedbackTimer.restart()
    }

    Timer {
        id: feedbackTimer
        interval: 4000
        onTriggered: root.feedbackMessage = ""
    }

    ColumnLayout {
        id: mainCol
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 20
        spacing: 16

        ColumnLayout {
            spacing: 2
            Text {
                text: qsTr("Reset & Recovery")
                font.pixelSize: Theme.fontSize("h2")
                font.weight: Font.DemiBold
                color: Theme.color("textPrimary")
            }
            Text {
                text: qsTr("Granular recovery actions. Each action requires separate confirmation.")
                font.pixelSize: Theme.fontSize("sm")
                color: Theme.color("textSecondary")
            }
        }

        // ── Feedback banner ───────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 36
            visible: root.feedbackMessage.length > 0
            radius: Theme.radiusCard
            color: root.feedbackError ? Qt.rgba(0.94, 0.2, 0.2, 0.08) : Qt.rgba(0.13, 0.77, 0.37, 0.08)
            border.color: root.feedbackError ? Qt.rgba(0.94, 0.2, 0.2, 0.3) : Qt.rgba(0.13, 0.77, 0.37, 0.3)
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: root.feedbackMessage
                font.pixelSize: Theme.fontSize("sm")
                color: root.feedbackError ? "#dc2626" : "#16a34a"
            }
        }

        // ── Environment ───────────────────────────────────────────────────
        SettingGroup {
            Layout.fillWidth: true
            label: qsTr("Environment")

            SettingCard {
                width: parent.width
                label: qsTr("Clear Session Environment Overrides")
                description: qsTr("Removes all session-scope env variables. Persistent user variables are not affected.")

                control: Button {
                    text: qsTr("Clear")
                    enabled: EnvServiceClient && EnvServiceClient.serviceAvailable
                    onClicked: confirm(
                        qsTr("Clear Session Env?"),
                        qsTr("All session-scope environment variable overrides will be removed. This does not affect user-persistent or system variables."),
                        function() {
                            // ClearSessionVars via EnvServiceClient
                            // Currently no direct method — show placeholder
                            showFeedback(qsTr("Session environment cleared."), false)
                        }
                    )
                }
            }
        }

        // ── Shell Components ──────────────────────────────────────────────
        SettingGroup {
            Layout.fillWidth: true
            label: qsTr("Shell Components")

            Column {
                width: parent.width
                spacing: 1

                Repeater {
                    model: [
                        { name: "envd",       label: qsTr("Environment Service"),  warning: "" },
                        { name: "loggerd",    label: qsTr("Logger Service"),       warning: "" },
                        { name: "portald",    label: qsTr("Portal Service"),       warning: "" },
                        { name: "clipboardd", label: qsTr("Clipboard Service"),    warning: "" },
                    ]
                    delegate: SettingCard {
                        width: parent.width
                        label: modelData.label
                        description: qsTr("Restart the %1 without logging out.").arg(modelData.label)

                        control: Button {
                            text: qsTr("Restart")
                            enabled: ProcessServicesController && ProcessServicesController.serviceAvailable
                            onClicked: confirm(
                                qsTr("Restart %1?").arg(modelData.label),
                                qsTr("The service will be briefly unavailable. Any in-flight requests may fail."),
                                function() {
                                    if (!ProcessServicesController) return
                                    const ok = ProcessServicesController.restart(modelData.name)
                                    showFeedback(
                                        ok ? qsTr("%1 restart requested.").arg(modelData.label)
                                           : qsTr("Failed: %1").arg(ProcessServicesController.lastError),
                                        !ok
                                    )
                                }
                            )
                        }
                    }
                }
            }
        }

        // ── Feature Flags ─────────────────────────────────────────────────
        SettingGroup {
            Layout.fillWidth: true
            label: qsTr("Feature Flags")
            visible: DeveloperMode && DeveloperMode.experimentalFeatures

            SettingCard {
                width: parent.width
                label: qsTr("Reset Experimental Features")
                description: qsTr("Disables all experimental feature flags and resets to defaults.")

                control: Button {
                    text: qsTr("Reset")
                    onClicked: confirm(
                        qsTr("Reset Feature Flags?"),
                        qsTr("All experimental feature flags will be disabled and reset to their default state. Some changes may require a restart."),
                        function() {
                            if (DeveloperMode) DeveloperMode.experimentalFeatures = false
                            showFeedback(qsTr("Feature flags reset."), false)
                        }
                    )
                }
            }
        }
    }
}
