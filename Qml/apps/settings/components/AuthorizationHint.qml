import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import SlmStyle as DSStyle

Rectangle {
    id: root
    Layout.fillWidth: true
    radius: 8
    color: "#F4F5F7"
    border.width: 1
    border.color: "#E5E7EB"
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

    implicitHeight: content.implicitHeight + 14

    RowLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 7
        spacing: 8

        DSStyle.Label {
            text: "\uD83D\uDD12"
            font.pixelSize: 13
            color: "#6B7280"
            Layout.alignment: Qt.AlignTop
        }

        Text {
            Layout.fillWidth: true
            text: root.reason.length > 0
                      ? qsTr("Permission required: %1").arg(root.reason)
                      : root.defaultMessage
            color: "#6B7280"
            font.pixelSize: 12
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
