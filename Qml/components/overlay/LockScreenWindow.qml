import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import Slm_Desktop

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
    property bool unlockBusy: false
    property date now: new Date()
    readonly property bool layerShellSupported: (typeof SecurityLayerShell !== "undefined")
                                                && !!SecurityLayerShell
                                                && !!SecurityLayerShell.supported
    property bool _layerPrepared: !layerShellSupported

    readonly property real uiScale: Math.max(0.90, Math.min(1.20,
                                   Math.min(width / 1920, height / 1080)))

    signal unlockRequested(string password)

    visible: !!rootWindow && !!rootWindow.visible && overlayVisible && root._layerPrepared
    color: "transparent"
    flags: Qt.FramelessWindowHint | (root.layerShellSupported ? 0 : Qt.WindowStaysOnTopHint)
    modality: Qt.ApplicationModal
    transientParent: root.layerShellSupported ? null : rootWindow
    title: "SLM Security Overlay"

    width: rootWindow ? rootWindow.width : Screen.width
    height: rootWindow ? rootWindow.height : Screen.height
    x: rootWindow ? rootWindow.x : 0
    y: rootWindow ? rootWindow.y : 0

    function syncSecurityLayerSurface() {
        if (!root.layerShellSupported || typeof SecurityLayerShell === "undefined" || !SecurityLayerShell) {
            return
        }
        SecurityLayerShell.setSecurityOverlay(root,
                                             Math.max(1, Math.round(root.width)),
                                             Math.max(1, Math.round(root.height)),
                                             0,
                                             0,
                                             Math.max(1, Math.round(root.width)),
                                             Math.max(1, Math.round(root.height)))
    }

    onVisibleChanged: {
        unlockBusy = false
        if (visible) {
            root.syncSecurityLayerSurface()
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
            root.now = new Date()
            requestActivate()
            passwordField.forceActiveFocus()
        }
    }

    onOverlayVisibleChanged: {
        if (overlayVisible && root.layerShellSupported && typeof SecurityLayerShell !== "undefined" && SecurityLayerShell) {
            SecurityLayerShell.onWindowAboutToShow()
        }
    }
    onWidthChanged: root.syncSecurityLayerSurface()
    onHeightChanged: root.syncSecurityLayerSurface()
    onSceneGraphInitialized: root.syncSecurityLayerSurface()

    Timer {
        id: layerShellRetryTimer
        interval: 300
        repeat: true
        running: root.visible && root.layerShellSupported
        onTriggered: root.syncSecurityLayerSurface()
    }

    Component.onCompleted: {
        if (root.layerShellSupported && typeof SecurityLayerShell !== "undefined" && SecurityLayerShell) {
            SecurityLayerShell.prepareSecurityOverlayWindow(root)
        }
        root._layerPrepared = true
        Qt.callLater(function() { root.syncSecurityLayerSurface() })
    }

    onLockFailedChanged: {
        if (root.lockFailed && !root.lockoutActive) {
            shakeAnimation.originX = centerContent.x
            shakeAnimation.restart()
            passwordField.text = ""
            passwordField.forceActiveFocus()
        }
    }

    function activateLockout(seconds) {
        lockoutActive = true
        lockoutDurationSec = seconds
        lockoutRemainingSec = seconds
        failedAttempts = 0
        lockoutTimer.restart()
        lockoutTick.restart()
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
        color: Theme.color("workspaceBackdrop")
    }

    // ── Clock + Date ─────────────────────────────────────────────────────────
    Column {
        id: clockGroup
        anchors.horizontalCenter: parent.horizontalCenter
        y: Math.round(root.height * 0.12)
        spacing: Theme.metric("spacingXs")

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: Qt.formatDateTime(root.now, "hh:mm")
            color: Theme.color("textOnGlass")
            font.pixelSize: Math.round(Theme.fontPxDisplay * 3.2)
            font.weight: Theme.fontWeight("light")
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: Qt.formatDateTime(root.now, "dddd, d MMMM")
            color: Theme.color("textOnGlass")
            font.pixelSize: Theme.fontSize("body")
            font.weight: Theme.fontWeight("medium")
        }

        Timer {
            interval: 1000
            running: root.visible
            repeat: true
            onTriggered: root.now = new Date()
        }
    }

    // ── Shake animation ───────────────────────────────────────────────────────
    SequentialAnimation {
        id: shakeAnimation
        property real originX: 0
        NumberAnimation { target: centerContent; property: "x"; from: shakeAnimation.originX;      to: shakeAnimation.originX - 14; duration: Math.max(1, Math.round(Theme.durationMicro * 0.56)); easing.type: Theme.easingStandard }
        NumberAnimation { target: centerContent; property: "x"; from: shakeAnimation.originX - 14; to: shakeAnimation.originX + 14; duration: Math.max(1, Math.round(Theme.durationMicro * 0.89)); easing.type: Theme.easingStandard }
        NumberAnimation { target: centerContent; property: "x"; from: shakeAnimation.originX + 14; to: shakeAnimation.originX - 8;  duration: Math.max(1, Math.round(Theme.durationMicro * 0.78)); easing.type: Theme.easingStandard }
        NumberAnimation { target: centerContent; property: "x"; from: shakeAnimation.originX - 8;  to: shakeAnimation.originX + 8;  duration: Math.max(1, Math.round(Theme.durationMicro * 0.67)); easing.type: Theme.easingStandard }
        NumberAnimation { target: centerContent; property: "x"; from: shakeAnimation.originX + 8;  to: shakeAnimation.originX;      duration: Math.max(1, Math.round(Theme.durationMicro * 0.56)); easing.type: Theme.easingDefault }
    }

    // ── Frosted card backdrop ─────────────────────────────────────────────────
    Rectangle {
        anchors.centerIn: centerContent
        width: centerContent.width + Math.round(48 * root.uiScale)
        height: centerContent.height + Math.round(56 * root.uiScale)
        radius: Theme.radiusHuge
        color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.07) : Qt.rgba(1, 1, 1, 0.28)
        border.width: Theme.borderWidthThin
        border.color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.13) : Qt.rgba(1, 1, 1, 0.48)
    }

    // ── Center content ────────────────────────────────────────────────────────
    Column {
        id: centerContent
        anchors.centerIn: parent
        anchors.verticalCenterOffset: Math.round(root.height * 0.05)
        spacing: Theme.metric("spacingMd")

        // Avatar
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.round(96 * root.uiScale)
            height: width
            radius: width / 2
            color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(1, 1, 1, 0.38)
            border.color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.24) : Qt.rgba(1, 1, 1, 0.56)
            border.width: Theme.borderWidthThin

            Label {
                anchors.centerIn: parent
                text: String(root.userName || "User").length > 0
                      ? String(root.userName).charAt(0).toUpperCase() : "U"
                color: Theme.color("textPrimary")
                font.pixelSize: Math.round(Theme.fontPxDisplay * 1.57)
                font.weight: Theme.fontWeight("semibold")
            }
        }

        // User display name
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.userName || "User"
            color: Theme.color("textPrimary")
            font.pixelSize: Theme.fontSize("hero")
            font.weight: Theme.fontWeight("medium")
        }

        // Password + submit (integrated pill)
        Item {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(root.width * 0.28, 340)
            height: Theme.controlHeightRegular

            TextField {
                id: passwordField
                anchors.fill: parent
                placeholderText: "Password"
                echoMode: TextInput.Password
                enabled: !root.lockoutActive && !root.unlockBusy
                leftPadding: Theme.metric("spacingMd")
                rightPadding: submitBtn.width + Theme.metric("spacingMd")
                font.pixelSize: Theme.fontSize("titleLarge")
                color: Theme.color("textPrimary")
                background: Rectangle {
                    radius: parent.height / 2
                    color: Theme.color("surface")
                    border.color: passwordField.activeFocus ? Theme.color("focusRing") : Theme.color("panelBorder")
                    border.width: Theme.borderWidthThin
                }
                onAccepted: root.unlockRequested(text)
            }

            Rectangle {
                id: submitBtn
                width: Math.round(32 * root.uiScale)
                height: Math.round(32 * root.uiScale)
                radius: height / 2
                anchors.right: parent.right
                anchors.rightMargin: Theme.metric("spacingXs")
                anchors.verticalCenter: parent.verticalCenter
                color: (root.lockoutActive || root.unlockBusy) ? Qt.rgba(0.039, 0.518, 1.0, 0.55)
                       : (submitBtnArea.containsMouse ? Theme.color("accentHover") : Theme.color("accent"))
                Behavior on color {
                    ColorAnimation { duration: Theme.durationMicro; easing.type: Theme.easingStandard }
                }

                // Spinner — visible during unlock
                Item {
                    anchors.centerIn: parent
                    visible: root.unlockBusy
                    width: Math.round(16 * root.uiScale)
                    height: Math.round(16 * root.uiScale)
                    RotationAnimator on rotation {
                        running: root.unlockBusy
                        from: 0; to: 360
                        loops: Animation.Infinite
                        duration: Theme.durationWorkspace * 2
                    }
                    Canvas {
                        anchors.fill: parent
                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.reset()
                            const lw = 2.0
                            const r = (width - lw) / 2
                            ctx.beginPath()
                            ctx.arc(width / 2, height / 2, r, Math.PI * 0.25, Math.PI * 1.75, false)
                            ctx.lineWidth = lw
                            ctx.lineCap = "round"
                            ctx.strokeStyle = "rgba(255,255,255,0.92)"
                            ctx.stroke()
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: !root.unlockBusy
                    text: "❯"
                    color: Theme.color("accentText")
                    font.pixelSize: Theme.fontSize("body")
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                MouseArea {
                    id: submitBtnArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    enabled: !root.lockoutActive && !root.unlockBusy
                    onClicked: root.unlockRequested(passwordField.text)
                }
            }
        }

        // Error + lockout message
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.lockFailed || root.lockoutActive
            text: root.lockoutActive
                  ? ("Too many attempts. Try again in " + root.lockoutRemainingSec + "s")
                  : root.errorTextForCode(root.unlockErrorCode)
            color: Theme.color("error")
            font.pixelSize: Theme.fontSize("body")
            horizontalAlignment: Text.AlignHCenter
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
