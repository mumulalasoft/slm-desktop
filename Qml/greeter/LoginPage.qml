import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    color: "#0b1118"
    focus: true

    // ── State ────────────────────────────────────────────────────────────────
    property string notificationMessage: ""
    property date   now: new Date()
    property bool   capsLockOn: false
    property string selectedMode: "normal"
    property bool   installBusy: false
    property string installStatusText: ""

    property int    selectedUserIndex: -1
    property bool   lastUserResolved: false
    property var    users: []

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
            if (u && u.avatar) return String(u.avatar)
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
        notificationMessage = ""
        if (GreeterApp) {
            GreeterApp.login(userField.text.trim(),
                             passwordField.text,
                             root.selectedMode)
        }
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
        color: "#3a000000"
    }

    // ── Clock ─────────────────────────────────────────────────────────────────
    Label {
        anchors.horizontalCenter: parent.horizontalCenter
        y: Math.round(root.height * 0.18)
        text: Qt.formatDateTime(root.now, "hh:mm")
        color: "#f2ffffff"
        font.pixelSize: Math.round(74 * root.uiScale)
        font.weight: Font.Light
    }

    // ── Shake animation (on login error) ─────────────────────────────────────
    SequentialAnimation {
        id: shakeAnimation
        property real originX: 0
        NumberAnimation { target: centerContent; property: "x"; from: shakeAnimation.originX;     to: shakeAnimation.originX - 14; duration: 50; easing.type: Easing.InOutSine }
        NumberAnimation { target: centerContent; property: "x"; from: shakeAnimation.originX - 14; to: shakeAnimation.originX + 14; duration: 80; easing.type: Easing.InOutSine }
        NumberAnimation { target: centerContent; property: "x"; from: shakeAnimation.originX + 14; to: shakeAnimation.originX - 8;  duration: 70; easing.type: Easing.InOutSine }
        NumberAnimation { target: centerContent; property: "x"; from: shakeAnimation.originX - 8;  to: shakeAnimation.originX + 8;  duration: 60; easing.type: Easing.InOutSine }
        NumberAnimation { target: centerContent; property: "x"; from: shakeAnimation.originX + 8;  to: shakeAnimation.originX;      duration: 50; easing.type: Easing.OutCubic }
    }

    // ── Center content ────────────────────────────────────────────────────────
    Column {
        id: centerContent
        anchors.centerIn: parent
        width: Math.min(root.width * 0.64, Math.round(760 * root.uiScale))
        spacing: Math.round(16 * root.uiScale)

        // Avatar circle
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.round(122 * root.uiScale)
            height: width
            radius: width / 2
            color: "#40ffffff"
            border.color: "#66ffffff"
            border.width: 1
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
                color: "#ffffff"
                font.pixelSize: Math.round(44 * root.uiScale)
                font.weight: Font.DemiBold
            }
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
                color: "#50d6dbe3"
                border.width: 1
                border.color: "#80ffffff"
            }
            contentItem: Text {
                leftPadding: Math.round(14 * root.uiScale)
                rightPadding: Math.round(30 * root.uiScale)
                verticalAlignment: Text.AlignVCenter
                text: userCombo.displayText
                color: "#0f1c2d"
                font.pixelSize: Math.round(16 * root.uiScale)
                elide: Text.ElideRight
            }
        }

        // Username field (no user model or single user)
        TextField {
            id: userField
            visible: !root.hasUsers || root.usersList.length === 1
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(centerContent.width * 0.7, Math.round(420 * root.uiScale))
            height: Math.round(44 * root.uiScale)
            placeholderText: "Username"
            leftPadding: Math.round(16 * root.uiScale)
            rightPadding: Math.round(16 * root.uiScale)
            font.pixelSize: Math.round(17 * root.uiScale)
            color: "#0f1c2d"
            background: Rectangle {
                radius: height / 2
                color: "#b8d9e2ef"
                border.color: userField.activeFocus ? "#e8ffffff" : "#99ffffff"
                border.width: 1
            }
            onAccepted: passwordField.forceActiveFocus()
        }

        // Password + submit
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Math.round(10 * root.uiScale)

            TextField {
                id: passwordField
                width: Math.min(centerContent.width * 0.56, Math.round(360 * root.uiScale))
                height: Math.round(44 * root.uiScale)
                echoMode: TextInput.Password
                placeholderText: "Password"
                leftPadding: Math.round(16 * root.uiScale)
                rightPadding: Math.round(16 * root.uiScale)
                font.pixelSize: Math.round(17 * root.uiScale)
                color: "#0f1c2d"
                background: Rectangle {
                    radius: height / 2
                    color: "#b8d9e2ef"
                    border.color: passwordField.activeFocus ? "#e8ffffff" : "#99ffffff"
                    border.width: 1
                }
                onAccepted: root.submitLogin()
                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_CapsLock) {
                        root.capsLockOn = !root.capsLockOn
                        event.accepted = false
                    }
                }
            }

            Button {
                width: Math.round(44 * root.uiScale)
                height: Math.round(44 * root.uiScale)
                text: "❯"
                onClicked: root.submitLogin()
                background: Rectangle {
                    radius: width / 2
                    color: parent.down ? "#0a5fb5" : "#0a84ff"
                }
                contentItem: Text {
                    text: parent.text
                    color: "#ffffff"
                    font.pixelSize: Math.round(20 * root.uiScale)
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        // Caps lock warning
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.capsLockOn
            text: "Caps Lock is ON"
            color: "#ffe0b16e"
            font.pixelSize: Math.round(14 * root.uiScale)
        }

        // Spacer
        Item {
            width: 1
            height: Math.round(14 * root.uiScale)
        }

        // Power + mode buttons
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Math.round(30 * root.uiScale)

            // Sleep
            PowerRow {
                icon: "⏾"
                label: "Sleep"
                enabled: GreeterApp ? GreeterApp.canSuspend : false
                onActionTriggered: if (GreeterApp) GreeterApp.suspend()
                uiScale: root.uiScale
            }

            // Restart
            PowerRow {
                icon: "↻"
                label: "Restart"
                enabled: GreeterApp ? GreeterApp.canReboot : false
                onActionTriggered: if (GreeterApp) GreeterApp.reboot()
                uiScale: root.uiScale
            }

            // Shut down
            PowerRow {
                icon: "⏻"
                label: "Shut Down"
                enabled: GreeterApp ? GreeterApp.canPowerOff : false
                onActionTriggered: if (GreeterApp) GreeterApp.powerOff()
                uiScale: root.uiScale
            }

            // Mode selector
            ModeSelector {
                id: modeSelector
                uiScale: root.uiScale
                onModeSelected: function(mode) { root.selectedMode = mode }
            }
        }

        // Error / notification
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.notificationMessage.length > 0
            text: root.notificationMessage
            color: "#ffdd8f8f"
            font.pixelSize: Math.round(14 * root.uiScale)
        }

        // ── Contextual notices ────────────────────────────────────────────────

        // Safe mode forced notice
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(centerContent.width * 0.85, Math.round(520 * root.uiScale))
            height: safeModeLabel.implicitHeight + Math.round(14 * root.uiScale)
            radius: Math.round(8 * root.uiScale)
            color: "#cc2a3a5c"
            border.color: "#884d7aff"
            border.width: 1
            visible: GreeterApp && GreeterApp.safeModeForced

            Label {
                id: safeModeLabel
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                anchors.margins: Math.round(12 * root.uiScale)
                text: "Safe Mode diaktifkan paksa. Semua konfigurasi custom dinonaktifkan untuk sesi ini."
                color: "#aac8ff"
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
            color: "#cc3a3010"
            border.color: "#88d4a040"
            border.width: 1
            visible: GreeterApp && GreeterApp.configPending
                     && !GreeterApp.hasPreviousCrash

            Label {
                id: pendingLabel
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                anchors.margins: Math.round(12 * root.uiScale)
                text: "Perubahan konfigurasi terakhir belum dikonfirmasi stabil. "
                      + "Jika sesi crash lagi, konfigurasi akan di-rollback otomatis."
                color: "#e8d070"
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
            color: "#cc3a1010"
            border.color: "#88dd5050"
            border.width: 1
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
                color: "#ffaaaa"
                font.pixelSize: Math.round(13 * root.uiScale)
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(centerContent.width * 0.90, Math.round(560 * root.uiScale))
            radius: Math.round(8 * root.uiScale)
            color: "#cc3a3010"
            border.color: "#88f0b35a"
            border.width: 1
            visible: GreeterApp && (GreeterApp.missingComponents || []).length > 0
            implicitHeight: missingColumn.implicitHeight + Math.round(16 * root.uiScale)

            Column {
                id: missingColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Math.round(10 * root.uiScale)
                spacing: Math.round(8 * root.uiScale)

                Label {
                    width: parent.width
                    text: "Komponen inti desktop hilang. Beberapa fitur login/sesi dapat gagal."
                    color: "#ffe4b0"
                    font.pixelSize: Math.round(13 * root.uiScale)
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }

                Repeater {
                    model: GreeterApp ? (GreeterApp.missingComponents || []) : []
                    delegate: Rectangle {
                        width: parent ? parent.width : 400
                        color: "#44201208"
                        border.width: 1
                        border.color: "#66ffffff"
                        radius: Math.round(6 * root.uiScale)
                        implicitHeight: row.implicitHeight + Math.round(10 * root.uiScale)

                        RowLayout {
                            id: row
                            anchors.fill: parent
                            anchors.margins: Math.round(6 * root.uiScale)
                            spacing: Math.round(8 * root.uiScale)

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Label {
                                    Layout.fillWidth: true
                                    text: String((modelData || {}).title || (modelData || {}).componentId || "Unknown")
                                    color: "#ffffff"
                                    font.pixelSize: Math.round(12 * root.uiScale)
                                    font.bold: true
                                    wrapMode: Text.WordWrap
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: String((modelData || {}).guidance || (modelData || {}).description || "")
                                    color: "#ffd9c6"
                                    font.pixelSize: Math.round(11 * root.uiScale)
                                    wrapMode: Text.WordWrap
                                }
                            }

                            Button {
                                visible: !!(modelData || {}).autoInstallable
                                enabled: !root.installBusy
                                text: root.installBusy ? "Installing..." : "Install"
                                onClicked: {
                                    root.installBusy = true
                                    var res = GreeterApp.installMissingComponent(String((modelData || {}).componentId || ""))
                                    root.installBusy = false
                                    if (!!res && !!res.ok) {
                                        root.installStatusText = "Komponen berhasil dipasang."
                                        GreeterApp.refreshMissingComponents()
                                    } else {
                                        root.installStatusText = "Install gagal: " + String((res && res.error) ? res.error : "unknown")
                                    }
                                }
                            }
                        }
                    }
                }

                Label {
                    width: parent.width
                    visible: root.installStatusText.length > 0
                    text: root.installStatusText
                    color: "#ffcc99"
                    font.pixelSize: Math.round(11 * root.uiScale)
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    // ── GreeterApp signals ────────────────────────────────────────────────────
    Connections {
        target: GreeterApp
        function onLoginSuccess() {
            root.notificationMessage = ""
        }
        function onLoginError(errorType, description) {
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
