import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Slm_Desktop
import "file:/usr/lib/settings/components"

Flickable {
    id: root
    clip: true
    contentHeight: mainLayout.implicitHeight + 48

    property string highlightSettingId: ""
    property var lockAfterSleepBinding: SettingsApp.createBinding("settings:useraccounts/lock-after-sleep", true)
    property var displayNameBinding: SettingsApp.createBinding("settings:useraccounts/display-name", "Current User")
    property var usernameBinding: SettingsApp.createBinding("settings:useraccounts/username", "user")
    property bool autoLoginBusy: false
    property bool changePwBusy: false
    property string changePwRequestId: ""
    property string authErrorText: ""

    function initialForName(value) {
        const s = String(value || "").trim()
        return s.length > 0 ? s.charAt(0).toUpperCase() : "U"
    }

    function resolvedDisplayName() {
        if (UserAccounts && UserAccounts.displayName && UserAccounts.displayName.length > 0)
            return UserAccounts.displayName
        return String(displayNameBinding ? displayNameBinding.value : qsTr("Current User"))
    }

    function resolvedUsername() {
        if (UserAccounts && UserAccounts.username && UserAccounts.username.length > 0)
            return UserAccounts.username
        return String(usernameBinding ? usernameBinding.value : "user")
    }

    function resolvedAccountType() {
        if (UserAccounts && UserAccounts.accountType && UserAccounts.accountType.length > 0)
            return UserAccounts.accountType
        return qsTr("Standard")
    }

    ColumnLayout {
        id: mainLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 24
        anchors.topMargin: 32
        spacing: 24

        // ── Hero: Profile ──────────────────────────────────────────────────
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: heroColumn.implicitHeight + 56

            Rectangle {
                anchors.fill: parent
                radius: Theme.radiusWindow
                color: Qt.rgba(0.5, 0.5, 0.5, 0.05)
                border.color: Qt.rgba(0.5, 0.5, 0.5, 0.10)
                border.width: Theme.borderWidthThin
            }

            ColumnLayout {
                id: heroColumn
                anchors.centerIn: parent
                spacing: 10

                // Avatar with optional photo and edit chip
                Item {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 80
                    Layout.preferredHeight: 80

                    Rectangle {
                        anchors.fill: parent
                        radius: width / 2
                        color: Theme.color("accent")

                        Text {
                            anchors.centerIn: parent
                            text: root.initialForName(root.resolvedDisplayName())
                            color: Theme.color("accentText")
                            font.pixelSize: Theme.fontSize("display")
                            font.weight: Theme.fontWeight("semibold")
                            visible: heroAvatar.status !== Image.Ready
                        }
                    }

                    Rectangle {
                        anchors.fill: parent
                        radius: width / 2
                        clip: true
                        color: "transparent"
                        visible: (UserAccounts && UserAccounts.avatarPath &&
                                  UserAccounts.avatarPath.length > 0)

                        Image {
                            id: heroAvatar
                            anchors.fill: parent
                            source: (UserAccounts && UserAccounts.avatarPath) ? UserAccounts.avatarPath : ""
                            fillMode: Image.PreserveAspectCrop
                            smooth: true
                        }
                    }

                    // Edit chip (bottom-right corner)
                    Rectangle {
                        width: 28; height: 28
                        radius: width / 2
                        color: Theme.color("surface")
                        border.color: Theme.color("panelBorder")
                        border.width: Theme.borderWidthThin
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom

                        Text {
                            anchors.centerIn: parent
                            text: "✏"
                            font.pixelSize: Theme.fontSize("xs")
                            color: Theme.color("textSecondary")
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: SettingsApp.openModuleSetting("useraccounts", "profile")
                        }
                    }
                }

                // Full name
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: root.resolvedDisplayName()
                    font.pixelSize: Theme.fontSize("title")
                    font.weight: Theme.fontWeight("bold")
                    color: Theme.color("textPrimary")
                    elide: Text.ElideRight
                    Layout.maximumWidth: mainLayout.width - 96
                }

                // Username
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "@" + root.resolvedUsername()
                    font.pixelSize: Theme.fontSize("body")
                    color: Theme.color("textSecondary")
                }

                // Account type badge
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    implicitHeight: accountTypeBadge.implicitHeight + 6
                    implicitWidth: accountTypeBadge.implicitWidth + 16
                    radius: height / 2
                    color: (root.resolvedAccountType() === "Administrator")
                           ? Qt.rgba(1.0, 0.6, 0.0, 0.12)
                           : Qt.rgba(0.5, 0.5, 0.5, 0.08)
                    border.color: (root.resolvedAccountType() === "Administrator")
                                  ? Qt.rgba(1.0, 0.6, 0.0, 0.30)
                                  : Qt.rgba(0.5, 0.5, 0.5, 0.18)
                    border.width: Theme.borderWidthThin

                    Text {
                        id: accountTypeBadge
                        anchors.centerIn: parent
                        text: root.resolvedAccountType()
                        font.pixelSize: Theme.fontSize("caption")
                        font.weight: Theme.fontWeight("medium")
                        color: (root.resolvedAccountType() === "Administrator")
                               ? Qt.rgba(0.85, 0.50, 0.0, 1.0)
                               : Theme.color("textSecondary")
                    }
                }
            }
        }

        // ── Sign-In Options ────────────────────────────────────────────────
        SettingGroup {
            title: qsTr("Sign-In Options")
            Layout.fillWidth: true

            SettingCard {
                objectName: "auto-login"
                highlighted: root.highlightSettingId === "auto-login"
                label: qsTr("Log in automatically")
                description: qsTr("Skip password prompt on startup.")
                Layout.fillWidth: true

                SettingToggle {
                    enabled: !root.autoLoginBusy
                    checked: UserAccounts ? UserAccounts.automaticLoginEnabled : false
                    onToggled: {
                        if (!UserAccounts) return
                        root.autoLoginBusy = true
                        if (!UserAccounts.setAutomaticLoginEnabled(checked))
                            checked = !checked
                        root.autoLoginBusy = false
                    }
                }
            }

            SettingCard {
                objectName: "guest-session"
                highlighted: root.highlightSettingId === "guest-session"
                label: qsTr("Guest Session")
                description: (UserAccounts && UserAccounts.guestSessionSupported)
                    ? qsTr("Allow temporary guest access from the login screen.")
                    : qsTr("Not available on this login backend.")
                Layout.fillWidth: true

                SettingToggle {
                    enabled: UserAccounts && UserAccounts.guestSessionSupported
                    checked: UserAccounts ? UserAccounts.guestSessionEnabled : false
                    onToggled: {
                        if (!UserAccounts) return
                        if (!UserAccounts.setGuestSessionEnabled(checked))
                            checked = !checked
                    }
                }
            }
        }

        // ── Password & Security ────────────────────────────────────────────
        SettingGroup {
            title: qsTr("Password & Security")
            Layout.fillWidth: true

            SettingCard {
                objectName: "lock-after-sleep"
                highlighted: root.highlightSettingId === "password-policy"
                label: qsTr("Require password after sleep")
                description: qsTr("Lock screen when waking from sleep or screen saver.")
                Layout.fillWidth: true

                SettingToggle {
                    checked: !!(lockAfterSleepBinding && lockAfterSleepBinding.value)
                    onToggled: if (lockAfterSleepBinding) lockAfterSleepBinding.value = checked
                }
            }

            SettingCard {
                objectName: "change-password"
                highlighted: root.highlightSettingId === "password-policy"
                label: qsTr("Change Password")
                description: qsTr("Update your account login password.")
                Layout.fillWidth: true

                Button {
                    text: root.changePwBusy ? qsTr("Authorizing…") : qsTr("Change…")
                    enabled: !root.changePwBusy
                    onClicked: {
                        root.authErrorText = ""
                        root.changePwBusy = true
                        root.changePwRequestId = SettingsApp.requestSettingAuthorization(
                            "useraccounts", "password-policy")
                    }
                }
            }

            // Auth error inline — only shown when non-empty
            Text {
                visible: root.authErrorText.length > 0
                text: root.authErrorText
                font.pixelSize: Theme.fontSize("caption")
                color: Theme.color("error")
                wrapMode: Text.Wrap
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 4
            }
        }

        // ── Other Users on This Device ─────────────────────────────────────
        SettingGroup {
            title: qsTr("Other Users on This Device")
            Layout.fillWidth: true

            // Empty state
            Item {
                visible: !(UserAccounts && UserAccounts.users && UserAccounts.users.length > 0)
                Layout.fillWidth: true
                Layout.preferredHeight: 80
                Layout.topMargin: 4
                Layout.bottomMargin: 4

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 4

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Only you have an account on this device.")
                        font.pixelSize: Theme.fontSize("body")
                        color: Theme.color("textSecondary")
                    }
                }
            }

            Repeater {
                model: UserAccounts ? UserAccounts.users : []

                delegate: SettingCard {
                    id: userCard
                    required property var modelData
                    Layout.fillWidth: true

                    readonly property string userDisplayName: String(modelData.displayName || modelData.username || "")
                    readonly property string userUsername: String(modelData.username || "")
                    readonly property string userAccountType: String(modelData.accountType || qsTr("Standard"))
                    readonly property bool userLocked: Boolean(modelData.locked)
                    readonly property string userAvatarPath: String(modelData.avatarPath || "")
                    readonly property string userInitial: userDisplayName.length > 0
                        ? userDisplayName.charAt(0).toUpperCase() : "U"

                    label: userDisplayName
                    description: {
                        var parts = ["@" + userUsername, userAccountType]
                        if (userLocked) parts.push(qsTr("Locked"))
                        return parts.join(" · ")
                    }

                    // Avatar chip
                    Item {
                        width: 36; height: 36

                        Rectangle {
                            anchors.fill: parent
                            radius: width / 2
                            color: Theme.color("accent")

                            Text {
                                anchors.centerIn: parent
                                text: userCard.userInitial
                                color: Theme.color("accentText")
                                font.pixelSize: Theme.fontSize("subtitle")
                                font.weight: Theme.fontWeight("semibold")
                                visible: userRowPhoto.status !== Image.Ready
                            }
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: width / 2
                            clip: true
                            color: "transparent"
                            visible: userCard.userAvatarPath.length > 0

                            Image {
                                id: userRowPhoto
                                anchors.fill: parent
                                source: userCard.userAvatarPath
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                            }
                        }
                    }
                }
            }

            // Add User + Refresh footer
            SettingCard {
                label: qsTr("Add User…")
                description: qsTr("Create a new account on this device.")
                hideSeparator: true
                Layout.fillWidth: true

                RowLayout {
                    spacing: 8

                    Button {
                        text: qsTr("Add User…")
                        onClicked: SettingsApp.openModuleSetting("useraccounts", "add-user")
                    }

                    Button {
                        text: qsTr("Refresh")
                        flat: true
                        onClicked: if (UserAccounts) UserAccounts.refresh()
                    }
                }
            }
        }
    }

    // ── Authorization result handler ───────────────────────────────────────
    Connections {
        target: SettingsApp
        function onSettingAuthorizationFinished(requestId, moduleId, settingId, allowed, reason) {
            if (String(requestId) !== root.changePwRequestId) return
            root.changePwBusy = false
            root.changePwRequestId = ""
            if (!allowed) {
                root.authErrorText = String(reason || qsTr("Authorization denied."))
                return
            }
            root.authErrorText = ""
            // Password change dialog — to be implemented
        }
    }
}
