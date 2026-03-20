import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    width: 1920
    height: 1080
    color: "#0b1118"
    focus: true

    property string notificationMessage: ""
    property date now: new Date()
    property bool capsLockOn: false
    property int selectedUserIndex: -1
    property bool lastUserResolved: false
    property var users: []
    property real uiScale: Math.max(0.90, Math.min(1.20, Math.min(width / 1920, height / 1080)))
    readonly property bool hasUserModel: !!userModel && userModel.count > 0

    function pickAvatar(icon, face, avatar, iconSource, home) {
        if (icon && icon.length > 0)
            return icon
        if (face && face.length > 0)
            return face
        if (avatar && avatar.length > 0)
            return avatar
        if (iconSource && iconSource.length > 0)
            return iconSource
        if (home && home.length > 0)
            return "file://" + home + "/.face.icon"
        return ""
    }

    function userInitial(value) {
        if (!value || value.length === 0)
            return "?"
        return value.charAt(0).toUpperCase()
    }

    function currentUserName() {
        if (selectedUserIndex >= 0 && selectedUserIndex < users.length && users[selectedUserIndex] && users[selectedUserIndex].name)
            return users[selectedUserIndex].name
        if (userField.text && userField.text.length > 0)
            return userField.text
        return "User"
    }

    function currentUserAvatar() {
        if (selectedUserIndex >= 0 && selectedUserIndex < users.length && users[selectedUserIndex] && users[selectedUserIndex].avatar)
            return users[selectedUserIndex].avatar
        return ""
    }

    function cacheUser(index, name, avatarPath) {
        if (index < 0)
            return
        const next = users.slice(0)
        next[index] = {
            name: name || "",
            avatar: avatarPath || ""
        }
        users = next

        if (!lastUserResolved && userModel && userModel.lastUser && userModel.lastUser.length > 0 && name === userModel.lastUser) {
            selectedUserIndex = index
            userField.text = name
            lastUserResolved = true
        }

        if (selectedUserIndex < 0 && users.length > 0 && users[0]) {
            selectedUserIndex = 0
            if ((!userField.text || userField.text.length === 0) && users[0].name)
                userField.text = users[0].name
        }
    }

    function submitLogin() {
        notificationMessage = ""
        sddm.login(userField.text, passwordField.text, sessionCombo.currentIndex)
    }

    function selectUser(index) {
        if (index < 0 || index >= users.length || !users[index])
            return
        selectedUserIndex = index
        if (users[index].name && users[index].name.length > 0)
            userField.text = users[index].name
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

    Image {
        anchors.fill: parent
        source: config.background
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
    }

    Rectangle {
        anchors.fill: parent
        color: "#3a000000"
    }

 

    Item {
        visible: false
        Repeater {
            model: root.hasUserModel ? userModel : null
            delegate: Item {
                Component.onCompleted: {
                    const n = (typeof name === "string") ? name : ""
                    const ic = (typeof icon === "string") ? icon : ""
                    const fc = (typeof face === "string") ? face : ""
                    const av = (typeof avatar === "string") ? avatar : ""
                    const is = (typeof iconSource === "string") ? iconSource : ""
                    const hm = (typeof home === "string") ? home : ""
                    root.cacheUser(index, n, root.pickAvatar(ic, fc, av, is, hm))
                }
            }
        }
    }

    Column {
        id: centerContent
        anchors.centerIn: parent
        width: Math.min(root.width * 0.64, 760 * root.uiScale)
        spacing: Math.round(16 * root.uiScale)

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

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.currentUserName()
            visible: false
            color: "#f0ffffff"
            font.pixelSize: Math.round(22 * root.uiScale)
            font.weight: Font.Medium
        }

        ComboBox {
            id: userCombo
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.hasUserModel && root.users.length > 1
            width: Math.min(centerContent.width * 0.7, Math.round(420 * root.uiScale))
            model: root.users
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

        TextField {
            id: userField
            visible: !root.hasUserModel
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(centerContent.width * 0.7, Math.round(420 * root.uiScale))
            height: Math.round(44 * root.uiScale)
            placeholderText: "Username"
            text: (userModel && userModel.lastUser) ? userModel.lastUser : ""
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

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Math.round(10 * root.uiScale)

            TextField {
                id: passwordField
                width: Math.min(centerContent.width * 0.56, Math.round(360 * root.uiScale))
                height: Math.round(44 * root.uiScale)
         
                echoMode: TextInput.Password
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

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.capsLockOn
            text: "Caps Lock is ON"
            color: "#ffe0b16e"
            font.pixelSize: Math.round(14 * root.uiScale)
        }

        Item {
            width: 1
            height: Math.round(14 * root.uiScale)
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Math.round(30 * root.uiScale)

            Column {
                spacing: Math.round(8 * root.uiScale)
                Button {
                    width: Math.round(48 * root.uiScale)
                    height: width
                    enabled: sddm.canSuspend
                    onClicked: sddm.suspend()
                    background: Rectangle {
                        radius: width / 2
                        color: parent.down ? "#44ffffff" : "transparent"
                        border.width: 1
                        border.color: "#ccffffff"
                    }
                    contentItem: Text {
                        text: "⏾"
                        color: "#f5ffffff"
                        font.pixelSize: Math.round(24 * root.uiScale)
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                Label {
                    text: "Sleep"
                    color: "#d8ffffff"
                    anchors.horizontalCenter: parent.horizontalCenter
                    font.pixelSize: Math.round(14 * root.uiScale)
                }
            }

            Column {
                spacing: Math.round(8 * root.uiScale)
                Button {
                    width: Math.round(48 * root.uiScale)
                    height: width
                    enabled: sddm.canReboot
                    onClicked: sddm.reboot()
                    background: Rectangle {
                        radius: width / 2
                        color: parent.down ? "#44ffffff" : "transparent"
                        border.width: 1
                        border.color: "#ccffffff"
                    }
                    contentItem: Text {
                        text: "↻"
                        color: "#f5ffffff"
                        font.pixelSize: Math.round(24 * root.uiScale)
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                Label {
                    text: "Restart"
                    color: "#d8ffffff"
                    anchors.horizontalCenter: parent.horizontalCenter
                    font.pixelSize: Math.round(14 * root.uiScale)
                }
            }

            Column {
                spacing: Math.round(8 * root.uiScale)
                Button {
                    width: Math.round(48 * root.uiScale)
                    height: width
                    enabled: sddm.canPowerOff
                    onClicked: sddm.powerOff()
                    background: Rectangle {
                        radius: width / 2
                        color: parent.down ? "#44ffffff" : "transparent"
                        border.width: 1
                        border.color: "#ccffffff"
                    }
                    contentItem: Text {
                        text: "⏻"
                        color: "#f5ffffff"
                        font.pixelSize: Math.round(24 * root.uiScale)
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                Label {
                    text: "Shut Down"
                    color: "#d8ffffff"
                    anchors.horizontalCenter: parent.horizontalCenter
                    font.pixelSize: Math.round(14 * root.uiScale)
                }
            }

            Column {
                spacing: Math.round(8 * root.uiScale)
                Button {
                    id: sessionButton
                    width: Math.round(48 * root.uiScale)
                    height: width
                    onClicked: sessionPopup.open()
                    background: Rectangle {
                        radius: width / 2
                        color: parent.down ? "#44ffffff" : "transparent"
                        border.width: 1
                        border.color: "#ccffffff"
                    }
                    contentItem: Text {
                        text: "⇄"
                        color: "#f5ffffff"
                        font.pixelSize: Math.round(24 * root.uiScale)
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                Label {
                    text: "Other..."
                    color: "#d8ffffff"
                    anchors.horizontalCenter: parent.horizontalCenter
                    font.pixelSize: Math.round(14 * root.uiScale)
                }

                Popup {
                    id: sessionPopup
                    y: sessionButton.height + Math.round(8 * root.uiScale)
                    x: -Math.round(95 * root.uiScale)
                    width: Math.round(240 * root.uiScale)
                    modal: false
                    focus: true
                    padding: Math.round(8 * root.uiScale)
                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                    background: Rectangle {
                        radius: Math.round(10 * root.uiScale)
                        color: "#e31a2433"
                        border.width: 1
                        border.color: "#70ffffff"
                    }

                    ListView {
                        implicitHeight: contentHeight
                        model: sessionModel
                        boundsBehavior: Flickable.StopAtBounds
                        clip: true
                        delegate: ItemDelegate {
                            width: ListView.view.width
                            text: (typeof name === "string" && name.length > 0) ? name : "Session"
                            highlighted: index === sessionCombo.currentIndex
                            onClicked: {
                                sessionCombo.currentIndex = index
                                sessionPopup.close()
                            }
                            background: Rectangle {
                                radius: Math.round(8 * root.uiScale)
                                color: parent.highlighted ? "#0a84ff" : "transparent"
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "#ffffff"
                                font.pixelSize: Math.round(15 * root.uiScale)
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: Math.round(10 * root.uiScale)
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.notificationMessage.length > 0
            text: root.notificationMessage
            color: "#ffdd8f8f"
            font.pixelSize: Math.round(14 * root.uiScale)
        }
    }

    Label {
        id: topClock
        anchors.horizontalCenter: parent.horizontalCenter
        y: Math.round(root.height * 0.18)
        text: Qt.formatDateTime(root.now, "hh:mm")
        color: "#f2ffffff"
        font.pixelSize: Math.round(74 * root.uiScale)
        font.weight: Font.Light
    }

    ComboBox {
        id: sessionCombo
        visible: false
        model: sessionModel
        textRole: "name"
        valueRole: "name"
    }

    Connections {
        target: sddm
        function onLoginFailed() {
            root.notificationMessage = "Login failed"
            passwordField.selectAll()
            passwordField.forceActiveFocus()
        }
    }

    Component.onCompleted: {
        if (!userField.text || userField.text.length === 0)
            passwordField.forceActiveFocus()
        else
            passwordField.forceActiveFocus()
    }
}
