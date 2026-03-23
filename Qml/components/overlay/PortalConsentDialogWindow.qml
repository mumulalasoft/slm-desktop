import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop

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

    readonly property var mediation: payload && payload.mediation ? payload.mediation : ({})
    readonly property string appLabel: {
        var appId = String(payload && payload.appId ? payload.appId : "")
        if (appId.length) {
            return appId
        }
        return String(payload && payload.desktopFileId ? payload.desktopFileId : "Unknown application")
    }
    readonly property string capabilityLabel: String(payload && payload.capability ? payload.capability : "Capability")
    readonly property string mediationScope: String(mediation && mediation.scope ? mediation.scope : "session")
    readonly property string mediationReason: {
        var reason = String(mediation && mediation.reason ? mediation.reason : "")
        if (reason.length) {
            return reason
        }
        return String(payload && payload.reason ? payload.reason : "Permission required")
    }

    title: "Permission Request"
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
        forceActiveFocus()
    }

    Shortcut {
        sequence: "Escape"
        enabled: root.visible
        onActivated: root.cancelRequested()
    }

    bodyComponent: Component {
        ColumnLayout {
            spacing: 8
            implicitHeight: 180

            Label {
                Layout.fillWidth: true
                text: root.appLabel
                color: Theme.color("textPrimary")
                font.pixelSize: Theme.fontSize("subtitle")
                font.weight: Theme.fontWeight("semibold")
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                text: "Requests " + root.capabilityLabel
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

                    Label {
                        Layout.fillWidth: true
                        text: "Action: " + String(mediation && mediation.action ? mediation.action : "consent")
                        color: Theme.color("textPrimary")
                        font.pixelSize: Theme.fontSize("small")
                        elide: Text.ElideRight
                    }
                    Label {
                        Layout.fillWidth: true
                        text: "Scope: " + root.mediationScope
                        color: Theme.color("textSecondary")
                        font.pixelSize: Theme.fontSize("small")
                        elide: Text.ElideRight
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: String(mediation && mediation.conflict_session ? mediation.conflict_session : "").length > 0
                        text: "Conflict session: " + String(mediation && mediation.conflict_session ? mediation.conflict_session : "")
                        color: Theme.color("textSecondary")
                        font.pixelSize: Theme.fontSize("small")
                        elide: Text.ElideMiddle
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                text: root.mediationReason
                color: Theme.color("warning")
                font.pixelSize: Theme.fontSize("small")
                wrapMode: Text.WordWrap
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

            Button {
                text: "Deny"
                onClicked: root.denyRequested()
            }

            Button {
                text: "Allow Once"
                highlighted: true
                defaultAction: true
                onClicked: root.allowOnceRequested()
            }

            Button {
                text: "Always Allow"
                onClicked: root.allowAlwaysRequested()
            }
        }
    }
}
