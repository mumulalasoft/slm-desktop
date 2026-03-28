import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Slm_Desktop
import "../../../../../Qml/apps/settings/components"

Flickable {
    id: root
    clip: true
    contentHeight: contentColumn.implicitHeight + 28

    property string highlightSettingId: ""
    property var lockAfterSleepBinding: SettingsApp.createBinding("settings:useraccounts/lock-after-sleep", true)
    property var displayNameBinding: SettingsApp.createBinding("settings:useraccounts/display-name", "Current User")
    property var usernameBinding: SettingsApp.createBinding("settings:useraccounts/username", "user")
    property bool autoLoginBusy: false

    function initialForName(value) {
        const s = String(value || "").trim()
        return s.length > 0 ? s.charAt(0).toUpperCase() : "U"
    }

    ColumnLayout {
        id: contentColumn
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 20
        spacing: 20

        Text {
            text: qsTr("User Accounts")
            font.pixelSize: Theme.fontSize("xl")
            font.weight: Font.Bold
            color: Theme.color("textPrimary")
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 98
            radius: 14
            color: Theme.color("surface")
            border.width: 1
            border.color: Theme.color("panelBorder")

            RowLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Rectangle {
                    Layout.preferredWidth: 52
                    Layout.preferredHeight: 52
                    radius: 26
                    color: Theme.color("accent")

                    Text {
                        anchors.centerIn: parent
                        text: root.initialForName((UserAccounts && UserAccounts.displayName && UserAccounts.displayName.length > 0)
                                                  ? UserAccounts.displayName
                                                  : (displayNameBinding ? displayNameBinding.value : ""))
                        color: Theme.color("accentText")
                        font.pixelSize: 22
                        font.weight: Font.DemiBold
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Text {
                        text: (UserAccounts && UserAccounts.displayName && UserAccounts.displayName.length > 0)
                            ? UserAccounts.displayName
                            : String(displayNameBinding ? displayNameBinding.value : qsTr("Current User"))
                        color: Theme.color("textPrimary")
                        font.pixelSize: Theme.fontSize("lg")
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    Text {
                        text: "@" + ((UserAccounts && UserAccounts.username && UserAccounts.username.length > 0)
                                     ? UserAccounts.username
                                     : String(usernameBinding ? usernameBinding.value : "user"))
                        color: Theme.color("textSecondary")
                        font.pixelSize: Theme.fontSize("sm")
                        elide: Text.ElideRight
                    }
                    Text {
                        text: (UserAccounts && UserAccounts.accountType && UserAccounts.accountType.length > 0)
                            ? UserAccounts.accountType
                            : qsTr("Standard")
                        color: Theme.color("textDisabled")
                        font.pixelSize: Theme.fontSize("xs")
                    }
                }

                Rectangle {
                    visible: UserAccounts && UserAccounts.avatarPath && UserAccounts.avatarPath.length > 0
                    Layout.preferredWidth: 42
                    Layout.preferredHeight: 42
                    radius: 21
                    color: "transparent"
                    clip: true
                    Image {
                        anchors.fill: parent
                        source: UserAccounts ? UserAccounts.avatarPath : ""
                        fillMode: Image.PreserveAspectCrop
                        smooth: true
                    }
                }

                Button {
                    text: qsTr("Edit")
                    onClicked: SettingsApp.openModuleSetting("useraccounts", "profile")
                }
            }
        }

        SettingGroup {
            title: qsTr("Sign-In Options")
            Layout.fillWidth: true

            SettingCard {
                highlighted: root.highlightSettingId === "auto-login"
                label: qsTr("Log in automatically")
                description: qsTr("Sign in without entering password on startup.")

                SettingToggle {
                    enabled: !root.autoLoginBusy
                    checked: UserAccounts ? UserAccounts.automaticLoginEnabled : false
                    onToggled: {
                        if (!UserAccounts) {
                            return
                        }
                        root.autoLoginBusy = true
                        if (!UserAccounts.setAutomaticLoginEnabled(checked)) {
                            checked = !checked
                        }
                        root.autoLoginBusy = false
                    }
                }
            }

            SettingCard {
                highlighted: root.highlightSettingId === "guest-session"
                label: qsTr("Guest Session")
                description: (UserAccounts && UserAccounts.guestSessionSupported)
                    ? qsTr("Allow temporary guest access from the login screen.")
                    : qsTr("Guest session is not available on this login backend.")

                SettingToggle {
                    enabled: UserAccounts && UserAccounts.guestSessionSupported
                    checked: UserAccounts ? UserAccounts.guestSessionEnabled : false
                    onToggled: {
                        if (!UserAccounts) {
                            return
                        }
                        if (!UserAccounts.setGuestSessionEnabled(checked)) {
                            checked = !checked
                        }
                    }
                }
            }

            SettingCard {
                highlighted: root.highlightSettingId === "password-policy"
                label: qsTr("Require password after sleep")
                description: qsTr("Ask password when waking from sleep.")

                SettingToggle {
                    checked: !!(lockAfterSleepBinding && lockAfterSleepBinding.value)
                    onToggled: if (lockAfterSleepBinding) lockAfterSleepBinding.value = checked
                }
            }
        }

        SettingGroup {
            title: qsTr("Password & Security")
            Layout.fillWidth: true

            SettingCard {
                highlighted: root.highlightSettingId === "password-policy"
                label: qsTr("Change Password")
                description: qsTr("Update your account password.")

                Button {
                    text: qsTr("Change Password…")
                    onClicked: SettingsApp.requestSettingAuthorization("useraccounts", "password-policy")
                }
            }

            SettingCard {
                visible: UserAccounts && UserAccounts.lastError && UserAccounts.lastError.length > 0
                label: qsTr("System status")
                description: UserAccounts ? UserAccounts.lastError : ""
            }
        }

        SettingGroup {
            title: qsTr("Other users on this device")
            Layout.fillWidth: true

            Repeater {
                model: UserAccounts ? UserAccounts.users : []

                delegate: SettingCard {
                    required property var modelData
                    label: String(modelData.displayName || modelData.username || "")
                    description: "@" + String(modelData.username || "") + " • "
                                 + String(modelData.accountType || qsTr("Standard"))

                    RowLayout {
                        spacing: 10
                        visible: !!(modelData.avatarPath && String(modelData.avatarPath).length > 0)
                        Rectangle {
                            width: 26
                            height: 26
                            radius: 13
                            clip: true
                            color: "transparent"
                            Image {
                                anchors.fill: parent
                                source: modelData.avatarPath || ""
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                            }
                        }
                    }
                }
            }

            SettingCard {
                label: qsTr("Refresh user list")
                description: qsTr("Reload users from system account service.")

                Button {
                    text: qsTr("Refresh")
                    onClicked: if (UserAccounts) UserAccounts.refresh()
                }
            }
        }
    }
}
