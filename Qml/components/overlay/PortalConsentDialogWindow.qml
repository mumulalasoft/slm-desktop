import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle as DSStyle

AppDialog {
    id: root

    required property var hostRoot
    required property var bridge

    property string requestPath: ""
    property var payload: ({})

    signal allowOnceRequested()
    signal allowAlwaysRequested()
    signal denyRequested()
    signal cancelRequested()
    signal openSettingsRequested()

    readonly property var mediation: payload && payload.mediation ? payload.mediation : ({})
    readonly property string appLabel: {
        var appId = String(payload && payload.appId ? payload.appId : "")
        if (appId.length) {
            return appId
        }
        return String(payload && payload.desktopFileId ? payload.desktopFileId : "Unknown application")
    }
    readonly property string capabilityLabel: String(payload && payload.capability ? payload.capability : "Capability")
    readonly property bool isSecretCapability: capabilityLabel.indexOf("Secret.") === 0
    readonly property bool isSecretReadCapability: capabilityLabel === "Secret.Read"
    readonly property bool isSecretWriteCapability: capabilityLabel === "Secret.Store"
    readonly property bool isSecretDeleteCapability: capabilityLabel === "Secret.Delete"
    readonly property bool isNetworkCapability: {
        var normalized = capabilityLabel.toLowerCase()
        return normalized.indexOf("network") === 0
                || normalized.indexOf("socket") >= 0
                || normalized.indexOf("firewall") >= 0
    }
    readonly property bool persistentEligible: {
        if (!payload || payload.persistentEligible === undefined) {
            return true
        }
        return !!payload.persistentEligible
    }
    readonly property string mediationScope: String(mediation && mediation.scope ? mediation.scope : "session")
    readonly property string mediationReason: {
        var reason = String(mediation && mediation.reason ? mediation.reason : "")
        if (reason.length) {
            return reason
        }
        return String(payload && payload.reason ? payload.reason : "Permission required")
    }
    readonly property string consentTitleText: {
        if (isSecretReadCapability) {
            return "Allow Access to Saved Sign-in Data?"
        }
        if (isSecretWriteCapability) {
            return "Allow This App to Save Sign-in Data?"
        }
        if (isSecretDeleteCapability) {
            return "Allow This App to Remove Saved Sign-in Data?"
        }
        if (isSecretCapability) {
            return "Allow Access to Sensitive Data?"
        }
        return "Permission Request"
    }
    readonly property string consentBodyText: {
        if (isSecretReadCapability) {
            return "This app wants to read saved tokens, passwords, or session keys."
        }
        if (isSecretWriteCapability) {
            return "This app wants to store sensitive data for faster sign-in."
        }
        if (isSecretDeleteCapability) {
            return "This app wants to remove previously saved sensitive data."
        }
        if (isSecretCapability) {
            return "This app wants to access sensitive app secrets."
        }
        return "Requests " + capabilityLabel
    }
    readonly property string consentHintText: {
        if (isSecretCapability) {
            return "You can change this later in Settings > Permissions."
        }
        return mediationReason
    }
    property bool deleteAlwaysConfirm: false

    title: consentTitleText
    standardButtons: Dialog.NoButton
    dialogWidth: 560
    bodyPadding: 14
    footerPadding: 12
    closePolicy: Popup.NoAutoClose
    modal: true

    x: Math.round((hostRoot.width - width) * 0.5)
    y: Math.round((hostRoot.height - height) * 0.5)

    onVisibleChanged: {
        if (!visible) {
            return
        }
        deleteAlwaysConfirm = false
        forceActiveFocus()
    }

    Shortcut {
        sequence: "Escape"
        enabled: root.visible
        onActivated: root.cancelRequested()
    }

    Shortcut {
        sequence: "Return"
        enabled: root.visible
        onActivated: root.allowOnceRequested()
    }

    Shortcut {
        sequence: "Enter"
        enabled: root.visible
        onActivated: root.allowOnceRequested()
    }

    bodyComponent: Component {
        ColumnLayout {
            spacing: 8
            implicitHeight: 180

            DSStyle.Label {
                Layout.fillWidth: true
                text: root.appLabel
                color: Theme.color("textPrimary")
                font.pixelSize: Theme.fontSize("subtitle")
                font.weight: Theme.fontWeight("semibold")
                elide: Text.ElideRight
            }

            DSStyle.Label {
                Layout.fillWidth: true
                text: root.consentBodyText
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("small")
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.fillWidth: true
                radius: Theme.radiusControl
                color: Theme.color("fileManagerSearchBg")
                border.width: Theme.borderWidthThin
                border.color: Theme.color("fileManagerControlBorder")
                implicitHeight: detailsCol.implicitHeight + 12

                ColumnLayout {
                    id: detailsCol
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4

                    DSStyle.Label {
                        Layout.fillWidth: true
                        visible: !root.isSecretCapability
                        text: "Action: " + String(mediation && mediation.action ? mediation.action : "consent")
                        color: Theme.color("textPrimary")
                        font.pixelSize: Theme.fontSize("small")
                        elide: Text.ElideRight
                    }
                    DSStyle.Label {
                        Layout.fillWidth: true
                        visible: !root.isSecretCapability
                        text: "Scope: " + root.mediationScope
                        color: Theme.color("textSecondary")
                        font.pixelSize: Theme.fontSize("small")
                        elide: Text.ElideRight
                    }
                    DSStyle.Label {
                        Layout.fillWidth: true
                        visible: String(mediation && mediation.conflict_session ? mediation.conflict_session : "").length > 0
                        text: "Conflict session: " + String(mediation && mediation.conflict_session ? mediation.conflict_session : "")
                        color: Theme.color("textSecondary")
                        font.pixelSize: Theme.fontSize("small")
                        elide: Text.ElideMiddle
                    }
                }
            }

            DSStyle.Label {
                Layout.fillWidth: true
                text: root.consentHintText
                color: root.isSecretCapability ? Theme.color("textSecondary") : Theme.color("warning")
                font.pixelSize: Theme.fontSize("small")
                wrapMode: Text.WordWrap
            }

            DSStyle.CheckBox {
                objectName: "secretDeleteConfirmCheckBox"
                Layout.fillWidth: true
                visible: root.isSecretDeleteCapability && root.persistentEligible
                text: "I understand this can remove saved sign-in data for this app."
                checked: root.deleteAlwaysConfirm
                onToggled: root.deleteAlwaysConfirm = checked
            }
        }
    }

    footerComponent: Component {
        RowLayout {
            implicitHeight: Math.max(34, Theme.metric("controlHeightRegular"))
            spacing: 8

            Item {
                Layout.fillWidth: true
            }

            DSStyle.Button {
                visible: root.isSecretCapability
                text: root.isNetworkCapability ? "Open Firewall Settings" : "Open Security Settings"
                onClicked: root.openSettingsRequested()
            }

            DSStyle.Button {
                text: root.isSecretCapability ? "Don't Allow" : "Deny"
                onClicked: root.denyRequested()
            }

            DSStyle.Button {
                id: allowOnceButton
                text: "Allow Once"
                defaultAction: true
                onClicked: root.allowOnceRequested()
            }

            DSStyle.Button {
                objectName: "consentAlwaysAllowButton"
                text: "Always Allow"
                visible: root.persistentEligible
                enabled: !root.isSecretDeleteCapability || root.deleteAlwaysConfirm
                onClicked: root.allowAlwaysRequested()
            }
        }
    }
}
