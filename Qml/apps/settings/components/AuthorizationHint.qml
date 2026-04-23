import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import SlmStyle as DSStyle

Rectangle {
    id: root
    Layout.fillWidth: true
    radius: Theme.radiusControl
    color: Theme.color("warningSubtle")
    border.width: Theme.borderWidthThin
    border.color: Theme.color("warning")
    visible: requiresAuthorization && !authorized

    property bool requiresAuthorization: false
    property bool authorized: false
    property bool pending: false
    property bool allowAlways: false
    property bool denyAlways: false
    property string reason: ""
    property string defaultMessage: qsTr("Permission required to modify this setting.")
    property string unlockText: qsTr("Unlock")

    signal unlockRequested()
    signal authorizationDecision(string decision)
    signal resetRequested()

    implicitHeight: content.implicitHeight + Theme.metric("spacingLg")

    RowLayout {
        id: content
        anchors.fill: parent
        anchors.margins: Theme.metric("spacingSm")
        spacing: Theme.metric("spacingSm")

        DSStyle.Label {
            text: "\uD83D\uDD12"
            font.pixelSize: Theme.fontSize("menu")
            color: Theme.color("textSecondary")
            Layout.alignment: Qt.AlignTop
        }

        Text {
            Layout.fillWidth: true
            text: root.reason.length > 0
                      ? qsTr("Permission required: %1").arg(root.reason)
                      : root.defaultMessage
            color: Theme.color("textSecondary")
            font.pixelSize: Theme.fontSize("small")
            wrapMode: Text.WordWrap
        }

        DSStyle.Button {
            visible: root.requiresAuthorization && !root.authorized
            enabled: !root.pending
            text: root.pending ? qsTr("Authorizing...") : root.unlockText
            onClicked: {
                if (root.pending) {
                    return
                }
                decisionMenu.popup()
            }
        }

        DSStyle.Button {
            visible: root.allowAlways || root.denyAlways
            enabled: !root.pending
            text: qsTr("Reset Decision")
            onClicked: root.resetRequested()
        }
    }

    DSStyle.Menu {
        id: decisionMenu

        DSStyle.MenuItem {
            text: qsTr("Allow Once")
            onTriggered: {
                root.authorizationDecision("allow-once")
                root.unlockRequested()
            }
        }
        DSStyle.MenuItem {
            text: qsTr("Always Allow")
            onTriggered: {
                root.authorizationDecision("allow-always")
                root.unlockRequested()
            }
        }
        MenuSeparator {}
        DSStyle.MenuItem {
            text: qsTr("Deny")
            onTriggered: {
                root.authorizationDecision("deny")
                root.unlockRequested()
            }
        }
        DSStyle.MenuItem {
            text: qsTr("Always Deny")
            onTriggered: {
                root.authorizationDecision("deny-always")
                root.unlockRequested()
            }
        }
    }
}
