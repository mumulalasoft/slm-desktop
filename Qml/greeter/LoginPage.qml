import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import "../components/system"

Rectangle {
    id: root
    color: Qt.rgba(0.043, 0.067, 0.094, 1.0)
    focus: true

    // ── State ────────────────────────────────────────────────────────────────
    property string notificationMessage: ""
    property date   now: new Date()
    property bool   capsLockOn: false
    property string selectedMode: "normal"
    property bool   installBusy: false
    property string installStatusText: ""
    property var    missingIssues: []
    property var    blockingMissingIssues: []

    property int    selectedUserIndex: -1
    property bool   loginBusy: false

    readonly property real uiScale: Math.max(0.90, Math.min(1.20,
                               Math.min(width / 1920, height / 1080)))
    readonly property var  usersList: (GreeterApp && GreeterApp.usersList)
                                      ? GreeterApp.usersList : []
    readonly property bool hasUsers: root.usersList.length > 0

    // ── Helpers ──────────────────────────────────────────────────────────────
    function pickAvatar(user) {
        if (!user) return ""
        const av = String(user.avatar || "")
        return av
    }

    function userInitial(value) {
        if (!value || value.length === 0) return "?"
        return value.charAt(0).toUpperCase()
    }

    function currentUserName() {
        if (selectedUserIndex >= 0 && selectedUserIndex < root.usersList.length) {
            const u = root.usersList[selectedUserIndex]
            if (u && u.name) return String(u.name)
        }
        return userField.text.length > 0 ? userField.text : "User"
    }

    function currentUserAvatar() {
        if (selectedUserIndex >= 0 && selectedUserIndex < root.usersList.length) {
            const u = root.usersList[selectedUserIndex]
            if (u && u.avatar) {
                const avatar = String(u.avatar)
                // Avoid probing per-user file:// avatars in greetd; missing
                // .face.icon is common and produces noisy startup warnings.
                if (avatar.indexOf("file://") === 0)
                    return ""
                return avatar
            }
        }
        return ""
    }

    function selectUser(index) {
        if (index < 0 || index >= root.usersList.length) return
        selectedUserIndex = index
        const u = root.usersList[index]
        if (u && u.name) userField.text = String(u.name)
    }

    function submitLogin() {
        if (root.loginBusy) return
        notificationMessage = ""
        if ((root.blockingMissingIssues || []).length > 0) {
            notificationMessage = "Komponen inti sesi masih hilang. Pasang komponen wajib dulu sebelum login."
            return
        }
        if (GreeterApp) {
            root.loginBusy = true
            GreeterApp.login(userField.text.trim(),
                             passwordField.text,
                             root.selectedMode)
        }
    }

    function refreshMissingIssues() {
        if (typeof MissingComponents === "undefined" || !MissingComponents) {
            root.missingIssues = []
            root.blockingMissingIssues = []
            return
        }
        root.missingIssues = MissingComponents.missingComponentsForDomain("greeter")
        root.blockingMissingIssues = MissingComponents.blockingMissingComponentsForDomain("greeter")
    }

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_CapsLock) {
            capsLockOn = !capsLockOn
            event.accepted = false
        }
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
            submitLogin()
    }

    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: root.now = new Date()
    }

    Component.onCompleted: {
        // Pre-fill username.
        if (root.hasUsers) {
            selectedUserIndex = 0
            userField.text = String(root.usersList[0].name || "")
        } else if (GreeterApp && GreeterApp.lastUser.length > 0) {
            userField.text = GreeterApp.lastUser
        }
        passwordField.forceActiveFocus()
        root.refreshMissingIssues()
    }

    // ── Background ───────────────────────────────────────────────────────────
    Image {
        anchors.fill: parent
        source: GreeterApp ? GreeterApp.backgroundSource : ""
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
    }

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.227)
    }

    // ── Clock + Date ──────────────────────────────────────────────────────────
    Column {
        id: clockGroup
        anchors.horizontalCenter: parent.horizontalCenter
        y: Math.round(root.height * 0.12)
        spacing: Math.round(6 * root.uiScale)

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: Qt.formatDateTime(root.now, "hh:mm")
            color: Qt.rgba(1, 1, 1, 0.961)
            font.pixelSize: Math.round(80 * root.uiScale)
            font.weight: Theme.fontWeight("light")
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: Qt.formatDateTime(root.now, "dddd, d MMMM")
            color: Qt.rgba(1, 1, 1, 0.867)
            font.pixelSize: Math.round(17 * root.uiScale)
            font.weight: Theme.fontWeight("medium")
        }
    }

    // ── Shake animation (on login error) ─────────────────────────────────────
    SequentialAnimation {
        id: shakeAnimation
        property real originX: 0
        NumberAnimation { target: centerContent; property: "x"; from: shakeAnimation.originX;       to: shakeAnimation.originX - 14; duration: Math.max(1, Math.round(Theme.durationMicro * 0.56)); easing.type: Theme.easingStandard }
        NumberAnimation { target: centerContent; property: "x"; from: shakeAnimation.originX - 14;  to: shakeAnimation.originX + 14; duration: Math.max(1, Math.round(Theme.durationMicro * 0.89)); easing.type: Theme.easingStandard }
        NumberAnimation { target: centerContent; property: "x"; from: shakeAnimation.originX + 14;  to: shakeAnimation.originX - 8;  duration: Math.max(1, Math.round(Theme.durationMicro * 0.78)); easing.type: Theme.easingStandard }
        NumberAnimation { target: centerContent; property: "x"; from: shakeAnimation.originX - 8;   to: shakeAnimation.originX + 8;  duration: Math.max(1, Math.round(Theme.durationMicro * 0.67)); easing.type: Theme.easingStandard }
        NumberAnimation { target: centerContent; property: "x"; from: shakeAnimation.originX + 8;   to: shakeAnimation.originX;      duration: Math.max(1, Math.round(Theme.durationMicro * 0.56)); easing.type: Theme.easingDefault }
    }

    // ── Card backdrop ─────────────────────────────────────────────────────────
    Rectangle {
        anchors.centerIn: centerContent
        width: centerContent.width + Math.round(48 * root.uiScale)
        height: centerContent.height + Math.round(56 * root.uiScale)
        radius: Math.round(20 * root.uiScale)
        color: Qt.rgba(0, 0.04, 0.12, 0.32)
        border.width: Theme.borderWidthThin
        border.color: Qt.rgba(1, 1, 1, 0.11)
    }

    // ── Center content ────────────────────────────────────────────────────────
    Column {
        id: centerContent
        anchors.centerIn: parent
        anchors.verticalCenterOffset: Math.round(root.height * 0.04)
        width: Math.min(root.width * 0.52, Math.round(520 * root.uiScale))
        spacing: Math.round(14 * root.uiScale)

        // Avatar circle
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.round(100 * root.uiScale)
            height: width
            radius: width / 2
            color: Qt.rgba(0.847, 0.910, 0.973, 0.157)
            border.color: Qt.rgba(1, 1, 1, 0.251)
            border.width: Theme.borderWidthThin
            clip: true

            Image {
                id: avatarImage
                anchors.fill: parent
                source: root.currentUserAvatar()
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                cache: true
                smooth: true
                visible: status === Image.Ready
            }

            Label {
                anchors.centerIn: parent
                text: root.userInitial(root.currentUserName())
                visible: avatarImage.status !== Image.Ready
                color: "white"
                font.pixelSize: Math.round(44 * root.uiScale)
                font.weight: Theme.fontWeight("semibold")
            }
        }

        // User display name (single user with known model)
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.hasUsers && root.usersList.length <= 1
            text: root.currentUserName()
            color: Qt.rgba(1, 1, 1, 0.949)
            font.pixelSize: Math.round(22 * root.uiScale)
            font.weight: Theme.fontWeight("medium")
        }

        // User combo (multi-user)
        ComboBox {
            id: userCombo
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.hasUsers && root.usersList.length > 1
            width: Math.min(centerContent.width * 0.7, Math.round(420 * root.uiScale))
            model: root.usersList
            textRole: "name"
            currentIndex: root.selectedUserIndex >= 0 ? root.selectedUserIndex : 0
            onActivated: root.selectUser(currentIndex)
            background: Rectangle {
                radius: height / 2
                color: Qt.rgba(0.839, 0.859, 0.890, 0.314)
                border.width: Theme.borderWidthThin
                border.color: Qt.rgba(1, 1, 1, 0.502)
            }
            contentItem: Text {
                leftPadding: Math.round(14 * root.uiScale)
                rightPadding: Math.round(30 * root.uiScale)
                verticalAlignment: Text.AlignVCenter
                text: userCombo.displayText
                color: Qt.rgba(0.059, 0.110, 0.176, 1.0)
                font.pixelSize: Math.round(16 * root.uiScale)
                elide: Text.ElideRight
            }
        }

        // Username field (no user model — single known user shows name label above instead)
        TextField {
            id: userField
            visible: !root.hasUsers
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(centerContent.width * 0.7, Math.round(420 * root.uiScale))
            height: Math.round(44 * root.uiScale)
            placeholderText: "Username"
            leftPadding: Math.round(16 * root.uiScale)
            rightPadding: Math.round(16 * root.uiScale)
            font.pixelSize: Math.round(17 * root.uiScale)
            color: Qt.rgba(0.059, 0.110, 0.176, 1.0)
            background: Rectangle {
                radius: height / 2
                color: Qt.rgba(0.851, 0.886, 0.937, 0.722)
                border.color: userField.activeFocus ? Qt.rgba(1, 1, 1, 0.910) : Qt.rgba(1, 1, 1, 0.600)
                border.width: Theme.borderWidthThin
            }
            onAccepted: passwordField.forceActiveFocus()
        }

        // Password + submit (integrated pill)
        Item {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(centerContent.width * 0.78, Math.round(380 * root.uiScale))
            height: Math.round(44 * root.uiScale)

            TextField {
                id: passwordField
                anchors.fill: parent
                echoMode: TextInput.Password
                placeholderText: "Password"
                enabled: !root.loginBusy
                leftPadding: Math.round(18 * root.uiScale)
                rightPadding: Math.round(54 * root.uiScale)
                font.pixelSize: Math.round(17 * root.uiScale)
                color: Qt.rgba(0.059, 0.110, 0.176, 1.0)
                background: Rectangle {
                    radius: parent.height / 2
                    color: Qt.rgba(0.851, 0.886, 0.937, 0.722)
                    border.color: passwordField.activeFocus ? Qt.rgba(1, 1, 1, 0.784) : Qt.rgba(1, 1, 1, 0.502)
                    border.width: Theme.borderWidthThin
                }
                onAccepted: root.submitLogin()
                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_CapsLock) {
                        root.capsLockOn = !root.capsLockOn
                        event.accepted = false
                    }
                }
            }

            Rectangle {
                id: submitBtn
                width: Math.round(32 * root.uiScale)
                height: Math.round(32 * root.uiScale)
                radius: height / 2
                anchors.right: parent.right
                anchors.rightMargin: Math.round(6 * root.uiScale)
                anchors.verticalCenter: parent.verticalCenter
                color: root.loginBusy ? Qt.rgba(0.039, 0.518, 1.0, 0.600)
                       : (!submitBtnArea.enabled ? Qt.rgba(0.431, 0.478, 0.541, 1.0)
                          : (submitBtnArea.containsMouse ? Qt.rgba(0, 0.439, 0.875, 1.0)
                                                         : Qt.rgba(0.039, 0.518, 1.0, 1.0)))
                Behavior on color { ColorAnimation { duration: Theme.durationMicro; easing.type: Theme.easingStandard } }

                // Spinner — shown during auth
                Item {
                    anchors.centerIn: parent
                    visible: root.loginBusy
                    width: Math.round(16 * root.uiScale)
                    height: Math.round(16 * root.uiScale)
                    RotationAnimator on rotation {
                        running: root.loginBusy
                        from: 0
                        to: 360
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

                // Arrow — hidden during auth
                Text {
                    anchors.centerIn: parent
                    visible: !root.loginBusy
                    text: "❯"
                    color: submitBtnArea.enabled ? "white" : Qt.rgba(0.690, 0.722, 0.769, 1.0)
                    font.pixelSize: Math.round(15 * root.uiScale)
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                MouseArea {
                    id: submitBtnArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    enabled: !root.loginBusy && (root.blockingMissingIssues || []).length === 0
                    onClicked: root.submitLogin()
                }
            }
        }

        // Caps lock warning
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.capsLockOn
            text: "⇪  Caps Lock is On"
            color: Qt.rgba(0.878, 0.784, 0.439, 1.0)
            font.pixelSize: Math.round(13 * root.uiScale)
        }

        // Error / notification
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.notificationMessage.length > 0
            text: root.notificationMessage
            color: Qt.rgba(0.867, 0.561, 0.561, 1.0)
            font.pixelSize: Math.round(13 * root.uiScale)
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            width: parent.width
        }

        // ── Contextual notices ────────────────────────────────────────────────

        // Safe mode forced notice
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(centerContent.width * 0.85, Math.round(520 * root.uiScale))
            height: safeModeLabel.implicitHeight + Math.round(14 * root.uiScale)
            radius: Math.round(8 * root.uiScale)
            color: Qt.rgba(0.165, 0.227, 0.361, 0.800)
            border.color: Qt.rgba(0.302, 0.478, 1.0, 0.533)
            border.width: Theme.borderWidthThin
            visible: GreeterApp && GreeterApp.safeModeForced

            Label {
                id: safeModeLabel
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                anchors.margins: Math.round(12 * root.uiScale)
                text: "Safe Mode diaktifkan paksa. Semua konfigurasi custom dinonaktifkan untuk sesi ini."
                color: Qt.rgba(0.667, 0.784, 1.0, 1.0)
                font.pixelSize: Math.round(13 * root.uiScale)
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }

        // Config pending notice
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(centerContent.width * 0.85, Math.round(520 * root.uiScale))
            height: pendingLabel.implicitHeight + Math.round(14 * root.uiScale)
            radius: Math.round(8 * root.uiScale)
            color: Qt.rgba(0.227, 0.188, 0.063, 0.800)
            border.color: Qt.rgba(0.831, 0.627, 0.251, 0.533)
            border.width: Theme.borderWidthThin
            visible: GreeterApp && GreeterApp.configPending
                     && !GreeterApp.hasPreviousCrash

            Label {
                id: pendingLabel
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                anchors.margins: Math.round(12 * root.uiScale)
                text: "Perubahan konfigurasi terakhir belum dikonfirmasi stabil. "
                      + "Jika sesi crash lagi, konfigurasi akan di-rollback otomatis."
                color: Qt.rgba(0.910, 0.816, 0.439, 1.0)
                font.pixelSize: Math.round(13 * root.uiScale)
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }

        // Crash / recovery notice
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(centerContent.width * 0.85, Math.round(520 * root.uiScale))
            height: crashLabel.implicitHeight + Math.round(14 * root.uiScale)
            radius: Math.round(8 * root.uiScale)
            color: Qt.rgba(0.227, 0.063, 0.063, 0.800)
            border.color: Qt.rgba(0.867, 0.314, 0.314, 0.533)
            border.width: Theme.borderWidthThin
            visible: GreeterApp && GreeterApp.hasPreviousCrash

            Label {
                id: crashLabel
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                anchors.margins: Math.round(12 * root.uiScale)
                text: {
                    let msg = "Sesi sebelumnya berakhir tidak normal"
                    if (GreeterApp.crashCount > 1)
                        msg += " (" + GreeterApp.crashCount + "x berturut-turut)"
                    if (GreeterApp.recoveryReason.length > 0)
                        msg += ": " + GreeterApp.recoveryReason
                    return msg + ". Pertimbangkan Safe Mode atau Recovery."
                }
                color: Qt.rgba(1.0, 0.667, 0.667, 1.0)
                font.pixelSize: Math.round(13 * root.uiScale)
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }

        MissingComponentsCard {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(centerContent.width * 0.90, Math.round(560 * root.uiScale))
            issues: root.missingIssues || []
            summaryText: "Komponen inti desktop hilang. Beberapa fitur login/sesi dapat gagal."
            statusText: root.installStatusText
            busy: root.installBusy
            scale: root.uiScale
            cardColor: "#cc3a3010"
            cardBorderColor: "#88f0b35a"
            titleColor: "#ffffff"
            detailColor: "#ffd9c6"
            statusColor: "#ffcc99"
            onInstallRequested: function(componentId) {
                root.installBusy = true
                var res = MissingComponents.installComponentForDomain("greeter", componentId)
                root.installBusy = false
                if (!!res && !!res.ok) {
                    root.installStatusText = "Komponen berhasil dipasang."
                    root.refreshMissingIssues()
                } else {
                    root.installStatusText = "Install gagal: " + String((res && res.error) ? res.error : "unknown")
                }
            }
        }
    }

    // ── Power + Mode buttons (bottom) ─────────────────────────────────────────
    Row {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Math.round(48 * root.uiScale)
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: Math.round(30 * root.uiScale)

        PowerRow {
            icon: "⏾"
            label: "Sleep"
            enabled: GreeterApp ? GreeterApp.canSuspend : false
            onActionTriggered: if (GreeterApp) GreeterApp.suspend()
            uiScale: root.uiScale
        }

        PowerRow {
            icon: "↻"
            label: "Restart"
            enabled: GreeterApp ? GreeterApp.canReboot : false
            onActionTriggered: if (GreeterApp) GreeterApp.reboot()
            uiScale: root.uiScale
        }

        PowerRow {
            icon: "⏻"
            label: "Shut Down"
            enabled: GreeterApp ? GreeterApp.canPowerOff : false
            onActionTriggered: if (GreeterApp) GreeterApp.powerOff()
            uiScale: root.uiScale
        }

        ModeSelector {
            id: modeSelector
            uiScale: root.uiScale
            onModeSelected: function(mode) { root.selectedMode = mode }
        }
    }

    // ── GreeterApp signals ────────────────────────────────────────────────────
    Connections {
        target: GreeterApp
        function onLoginSuccess() {
            root.loginBusy = false
            root.notificationMessage = ""
            passwordField.text = ""
        }
        function onLoginError(errorType, description) {
            root.loginBusy = false
            if (errorType === "auth_error") {
                root.notificationMessage = "Password salah. Coba lagi."
            } else {
                root.notificationMessage = description.length > 0
                    ? description
                    : "Login gagal (" + errorType + ")."
            }
            passwordField.text = ""
            passwordField.forceActiveFocus()
            shakeAnimation.originX = centerContent.x
            shakeAnimation.restart()
        }
        function onAuthMessageReceived(type, message) {
            if (type === "info" || type === "error") {
                root.notificationMessage = message
            }
        }
    }
}
