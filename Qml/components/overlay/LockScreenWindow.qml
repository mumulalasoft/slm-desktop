import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15

Window {
    id: root

    required property var rootWindow
    property bool overlayVisible: false
    property string userName: "User"
    property bool lockFailed: false
    property string unlockErrorCode: ""
    property int failedAttempts: 0
    property bool lockoutActive: false
    property int lockoutRemainingSec: 0
    property int lockoutDurationSec: 10
    property int lockoutLevel: 0

    signal unlockRequested(string password)

    visible: !!rootWindow && !!rootWindow.visible && overlayVisible
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    modality: Qt.ApplicationModal
    transientParent: rootWindow
    title: "Lock Screen"

    width: rootWindow ? rootWindow.width : Screen.width
    height: rootWindow ? rootWindow.height : Screen.height
    x: rootWindow ? rootWindow.x : 0
    y: rootWindow ? rootWindow.y : 0

    onVisibleChanged: {
        if (visible) {
            lockFailed = false
            unlockErrorCode = ""
            failedAttempts = 0
            lockoutActive = false
            lockoutRemainingSec = 0
            lockoutDurationSec = 10
            lockoutLevel = 0
            lockoutTimer.stop()
            lockoutTick.stop()
            passwordField.text = ""
            requestActivate()
            passwordField.forceActiveFocus()
        }
    }

    Item {
        anchors.fill: parent
        focus: root.visible
        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Escape) {
                event.accepted = true
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#4f000000"
    }

    Rectangle {
        width: Math.min(parent.width * 0.58, 920)
        height: Math.min(parent.height * 0.54, 540)
        anchors.centerIn: parent
        radius: 32
        color: "#08ffffff"
        border.width: 0
    }

    Column {
        id: centerContent
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        spacing: 16

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            y: -Math.round(root.height * 0.13)
            text: Qt.formatDateTime(new Date(), "hh:mm")
            color: "#f2ffffff"
            font.pixelSize: 74
            font.weight: Font.Light

            Timer {
                interval: 1000
                running: root.visible
                repeat: true
                onTriggered: parent.text = Qt.formatDateTime(new Date(), "hh:mm")
            }
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 122
            height: 122
            radius: 61
            color: "#40ffffff"
            border.color: "#66ffffff"
            border.width: 1

            Label {
                anchors.centerIn: parent
                text: String(root.userName || "User").length > 0 ? String(root.userName).charAt(0).toUpperCase() : "U"
                color: "#ffffff"
                font.pixelSize: 44
                font.weight: Font.DemiBold
            }
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 10

            TextField {
                id: passwordField
                width: Math.min(root.width * 0.34, 360)
                height: 44
                placeholderText: "Password"
                echoMode: TextInput.Password
                enabled: !root.lockoutActive
                leftPadding: 16
                rightPadding: 16
                font.pixelSize: 17
                color: "#0f1c2d"
                background: Rectangle {
                    radius: height / 2
                    color: "#b8d9e2ef"
                    border.color: passwordField.activeFocus ? "#e8ffffff" : "#99ffffff"
                    border.width: 1
                }
                onAccepted: root.unlockRequested(text)
            }

            Button {
                width: 44
                height: 44
                text: "❯"
                enabled: !root.lockoutActive
                onClicked: root.unlockRequested(passwordField.text)
                background: Rectangle {
                    radius: width / 2
                    color: parent.down ? "#0a5fb5" : "#0a84ff"
                }
                contentItem: Text {
                    text: parent.text
                    color: "#ffffff"
                    font.pixelSize: 20
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.lockFailed || root.lockoutActive
            text: root.lockoutActive
                  ? ("Too many attempts. Try again in " + root.lockoutRemainingSec + "s")
                  : root.errorTextForCode(root.unlockErrorCode)
            color: "#ffdd8f8f"
            font.pixelSize: 14
        }
    }

    function errorTextForCode(code) {
        if (code === "empty-password") {
            return "Password is required"
        }
        if (code === "service-unavailable") {
            return "Unlock service unavailable"
        }
        if (code === "dbus-call-failed") {
            return "Unlock request failed"
        }
        if (code === "rate-limited") {
            return "Too many attempts"
        }
        if (code === "pam-auth-failed" || code === "pam-acct-invalid") {
            return "Incorrect password"
        }
        if (code === "pam-unavailable") {
            return "Authentication unavailable"
        }
        if (code === "unlock-failed") {
            return "Failed to unlock session"
        }
        return "Unlock failed"
    }

    Timer {
        id: lockoutTimer
        interval: Math.max(1, root.lockoutDurationSec) * 1000
        repeat: false
        onTriggered: {
            root.lockoutActive = false
            root.lockoutRemainingSec = 0
            root.lockFailed = false
            passwordField.forceActiveFocus()
        }
    }

    Timer {
        id: lockoutTick
        interval: 1000
        repeat: true
        onTriggered: {
            if (!root.lockoutActive) {
                stop()
                return
            }
            root.lockoutRemainingSec = Math.max(0, root.lockoutRemainingSec - 1)
        }
    }

}
