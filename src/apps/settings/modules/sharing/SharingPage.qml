import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Slm_Desktop
import "file:/usr/lib/settings/components"

Flickable {
    id: root
    contentHeight: contentColumn.height
    clip: true

    // ── Helpers ───────────────────────────────────────────────────────────────

    function trustLevelLabel(level) {
        switch (String(level)) {
        case "full":      return qsTr("Full Trust")
        case "trusted":   return qsTr("Trusted")
        case "blocked":   return qsTr("Blocked")
        default:          return qsTr("Not Trusted")
        }
    }

    // ── Layout ────────────────────────────────────────────────────────────────

    ColumnLayout {
        id: contentColumn
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 20
        spacing: 28

        // Title
        Text {
            text: qsTr("Sharing")
            font.pixelSize: Theme.fontSize("display")
            font.weight: Theme.fontWeight("bold")
            color: Theme.color("textPrimary")
            topPadding: 8
        }

        Text {
            text: qsTr("Control how this device shares files, printers, and screens with nearby trusted devices.")
            font.pixelSize: Theme.fontSize("body")
            color: Theme.color("textSecondary")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // Service unavailable notice
        SettingCard {
            visible: !SharingServiceClient.available
            Layout.fillWidth: true
            label: qsTr("Sharing service unavailable")
            description: qsTr("The sharing daemon is not running. Some features may be unavailable.")
        }

        // ── 1. File Sharing ──────────────────────────────────────────────────
        SettingGroup {
            title: qsTr("File Sharing")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("Share files with nearby devices")
                description: qsTr("Other devices on this network can browse your shared folders")

                SettingToggle {
                    checked: SharingServiceClient.fileSharingEnabled
                    onToggled: SharingServiceClient.setFeatureEnabled("file-sharing", checked)
                }
            }

            Repeater {
                model: SharingServiceClient.sharedFolders
                delegate: SettingCard {
                    required property var modelData
                    label: modelData.path ? modelData.path.split("/").pop() || modelData.path : ""
                    description: modelData.path || ""

                    Row {
                        spacing: 8

                        Text {
                            text: modelData.permission === "write" ? qsTr("Read & Write") : qsTr("Read Only")
                            color: Theme.color("textSecondary")
                            font.pixelSize: Theme.fontSize("small")
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Button {
                            text: qsTr("Remove")
                            flat: true
                            onClicked: {
                                SharingServiceClient.removeSharedFolder(modelData.path)
                            }
                        }
                    }
                }
            }

            SettingCard {
                label: qsTr("Add Shared Folder…")
                description: qsTr("Choose a folder to make available on the network")

                Button {
                    text: qsTr("Add")
                    flat: true
                    enabled: SharingServiceClient.fileSharingEnabled
                    onClicked: addFolderDialog.open()
                }
            }
        }

        // ── 2. Nearby Devices ────────────────────────────────────────────────
        SettingGroup {
            title: qsTr("Nearby Devices")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("Nearby Sharing")
                description: qsTr("Discover and send files to devices around you")

                SettingToggle {
                    checked: SharingServiceClient.nearbySharingEnabled
                    onToggled: SharingServiceClient.setFeatureEnabled("nearby-sharing", checked)
                }
            }

            SettingCard {
                label: qsTr("Visible devices")
                description: SharingServiceClient.nearbyDevices.length === 0
                    ? qsTr("No devices found nearby")
                    : qsTr("%1 device(s) visible").arg(SharingServiceClient.nearbyDevices.length)

                Button {
                    text: SharingServiceClient.nearbyDiscovering ? qsTr("Stop") : qsTr("Scan")
                    flat: true
                    enabled: SharingServiceClient.nearbySharingEnabled
                    onClicked: {
                        if (SharingServiceClient.nearbyDiscovering)
                            SharingServiceClient.stopDiscovery()
                        else
                            SharingServiceClient.startDiscovery()
                    }
                }
            }

            Repeater {
                model: SharingServiceClient.nearbyDevices
                delegate: SettingCard {
                    required property var modelData
                    label: modelData.name || modelData.deviceId || ""
                    description: root.trustLevelLabel(modelData.trustLevel)

                    Row {
                        spacing: 8

                        Button {
                            text: qsTr("Send File")
                            flat: true
                            enabled: String(modelData.trustLevel) !== "blocked"
                            onClicked: {
                                // Opens file picker portal; result passed to sendFileTo
                                pendingSendDeviceId = modelData.deviceId
                            }
                        }

                        Button {
                            text: String(modelData.trustLevel) === "blocked"
                                  ? qsTr("Unblock")
                                  : (String(modelData.trustLevel) === "untrusted"
                                     ? qsTr("Trust")
                                     : qsTr("Manage"))
                            flat: true
                            onClicked: {
                                const id = modelData.deviceId
                                if (String(modelData.trustLevel) === "blocked")
                                    SharingServiceClient.unblockDevice(id)
                                else if (String(modelData.trustLevel) === "untrusted")
                                    SharingServiceClient.pairDevice(id)
                            }
                        }
                    }
                }
            }
        }

        // ── 3. Screen Sharing ────────────────────────────────────────────────
        SettingGroup {
            title: qsTr("Screen Sharing")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("Allow screen sharing")
                description: qsTr("Trusted devices can request to view or control this screen")

                SettingToggle {
                    checked: SharingServiceClient.screenSharingEnabled
                    onToggled: SharingServiceClient.setFeatureEnabled("screen-sharing", checked)
                }
            }

            SettingCard {
                label: qsTr("Require approval for each session")
                description: qsTr("A confirmation dialog appears every time a device requests access")

                SettingToggle {
                    checked: true
                    enabled: SharingServiceClient.screenSharingEnabled
                }
            }

            SettingCard {
                label: qsTr("Allow remote interaction")
                description: qsTr("Connected devices can control mouse and keyboard, not just view")

                SettingToggle {
                    checked: false
                    enabled: SharingServiceClient.screenSharingEnabled
                }
            }
        }

        // ── 4. Printer Sharing ───────────────────────────────────────────────
        SettingGroup {
            title: qsTr("Printer Sharing")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("Share connected printers")
                description: qsTr("Printers on this device become available to other devices on the network")

                SettingToggle {
                    checked: SharingServiceClient.printerSharingEnabled
                    onToggled: SharingServiceClient.setFeatureEnabled("printer-sharing", checked)
                }
            }

            SettingCard {
                label: qsTr("Network-discoverable printers")
                description: SharingServiceClient.printerSharingEnabled
                    ? qsTr("Printers are visible on this network")
                    : qsTr("Enable printer sharing to share printers")
                Layout.fillWidth: true
            }
        }

        // ── 5. Remote Access ─────────────────────────────────────────────────
        SettingGroup {
            title: qsTr("Remote Access")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("Allow remote terminal")
                description: qsTr("Trusted devices can open a secure terminal session on this device")

                SettingToggle {
                    checked: SharingServiceClient.remoteAccessEnabled
                    onToggled: SharingServiceClient.setFeatureEnabled("remote-access", checked)
                }
            }

            SettingCard {
                label: qsTr("Authorized devices")
                description: qsTr("Only devices you have explicitly trusted can connect")
                Layout.fillWidth: true
            }
        }

        // ── 6. Media Sharing ─────────────────────────────────────────────────
        SettingGroup {
            title: qsTr("Media Sharing")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("Share media library")
                description: qsTr("Allow nearby devices to browse and play your media")

                SettingToggle {
                    checked: SharingServiceClient.mediaSharingEnabled
                    onToggled: SharingServiceClient.setFeatureEnabled("media-sharing", checked)
                }
            }

            SettingCard {
                label: qsTr("Cast target")
                description: qsTr("Appear as a cast destination for other devices")

                SettingToggle {
                    checked: false
                    enabled: SharingServiceClient.mediaSharingEnabled
                }
            }
        }

        // ── 7. Clipboard Sharing ─────────────────────────────────────────────
        SettingGroup {
            title: qsTr("Clipboard")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("Sync clipboard with trusted devices")
                description: qsTr("Copy on one device, paste on another")

                SettingToggle {
                    checked: SharingServiceClient.clipboardSharingEnabled
                    onToggled: SharingServiceClient.setFeatureEnabled("clipboard-sharing", checked)
                }
            }

            SettingCard {
                label: qsTr("Trusted devices only")
                description: qsTr("Clipboard content is never synced with untrusted devices")
                Layout.fillWidth: true

                Text {
                    text: qsTr("Always on")
                    color: Theme.color("textSecondary")
                    font.pixelSize: Theme.fontSize("small")
                }
            }
        }

        // ── 8. Device Trust & Permissions ────────────────────────────────────
        SettingGroup {
            title: qsTr("Device Trust & Permissions")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("Trusted devices")
                description: SharingServiceClient.trustedDevices.length === 0
                    ? qsTr("No trusted devices")
                    : qsTr("%1 trusted device(s)").arg(SharingServiceClient.trustedDevices.length)
                Layout.fillWidth: true
            }

            Repeater {
                model: SharingServiceClient.trustedDevices
                delegate: SettingCard {
                    required property var modelData
                    label: modelData.name || modelData.deviceId || ""
                    description: root.trustLevelLabel(modelData.trustLevel)
                        + " · " + qsTr("Last seen %1").arg(modelData.lastSeenAt || qsTr("never"))

                    Row {
                        spacing: 8

                        Button {
                            text: qsTr("Revoke")
                            flat: true
                            onClicked: SharingServiceClient.unpairDevice(modelData.deviceId)
                        }

                        Button {
                            text: qsTr("Block")
                            flat: true
                            onClicked: SharingServiceClient.blockDevice(modelData.deviceId)
                        }
                    }
                }
            }
        }

        // ── 9. Advanced ──────────────────────────────────────────────────────
        SettingGroup {
            title: qsTr("Advanced")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("Device name on the network")
                description: qsTr("How this device appears to others")
                Layout.fillWidth: true

                Text {
                    text: Qt.application.name || "SLM Desktop"
                    color: Theme.color("textSecondary")
                }
            }

            SettingCard {
                label: qsTr("Restrict sharing to local network")
                description: qsTr("Prevent sharing services from being reachable over the internet")

                SettingToggle {
                    checked: true
                }
            }
        }

        // ── 10. Privacy & Security ───────────────────────────────────────────
        SettingGroup {
            title: qsTr("Privacy & Security")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("Require authentication for each share session")
                description: qsTr("Devices must confirm they are trusted before accessing shared resources")

                SettingToggle {
                    checked: true
                }
            }

            SettingCard {
                label: qsTr("Auto-reject untrusted transfer requests")
                description: qsTr("Incoming files from unknown devices are automatically rejected")

                SettingToggle {
                    checked: true
                }
            }

            SettingCard {
                label: qsTr("Audit log")
                description: qsTr("All sharing activity is logged for review")

                Button {
                    text: qsTr("View Log")
                    flat: true
                }
            }
        }

        Item { height: 20 }
    }

    // ── Dialogs ───────────────────────────────────────────────────────────────

    Dialog {
        id: addFolderDialog
        title: qsTr("Add Shared Folder")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: 480

        ColumnLayout {
            spacing: 12
            anchors.fill: parent

            Text {
                text: qsTr("Enter the path of the folder you want to share:")
                color: Theme.color("textPrimary")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            TextField {
                id: folderPathField
                placeholderText: qsTr("/home/user/Documents/Shared")
                Layout.fillWidth: true
            }

            RowLayout {
                Text { text: qsTr("Access:"); color: Theme.color("textPrimary") }
                ComboBox {
                    id: accessMode
                    model: [qsTr("Read Only"), qsTr("Read & Write")]
                }
            }

            CheckBox {
                id: guestAccessCheck
                text: qsTr("Allow anyone on this network (no password)")
            }
        }

        onAccepted: {
            const path = folderPathField.text.trim()
            if (!path)
                return
            SharingServiceClient.addSharedFolder(path, {
                "access": accessMode.currentIndex === 0 ? "ro" : "rw",
                "guestAccess": guestAccessCheck.checked
            })
            folderPathField.text = ""
        }
    }

    // ── Pairing confirmation dialog ───────────────────────────────────────────

    Dialog {
        id: pairingDialog
        title: qsTr("Pair Device")
        modal: true
        standardButtons: Dialog.Yes | Dialog.No

        property string pairingId: ""
        property var deviceInfo: ({})

        Text {
            text: pairingDialog.deviceInfo.name
                  ? qsTr("Allow \"%1\" to connect to this device?").arg(pairingDialog.deviceInfo.name)
                  : qsTr("Allow a new device to connect?")
            color: Theme.color("textPrimary")
            wrapMode: Text.WordWrap
            width: parent.width
        }

        onAccepted: SharingServiceClient.acceptPairingRequest(pairingId)
        onRejected: SharingServiceClient.rejectPairingRequest(pairingId)
    }

    // ── State ─────────────────────────────────────────────────────────────────

    property string pendingSendDeviceId: ""
    onPendingSendDeviceIdChanged: {
        if (pendingSendDeviceId) {
            // Real implementation opens a file-chooser portal here
            pendingSendDeviceId = ""
        }
    }

    Connections {
        target: SharingServiceClient
        function onPairingRequested(pairingId, deviceInfo) {
            pairingDialog.pairingId = pairingId
            pairingDialog.deviceInfo = deviceInfo
            pairingDialog.open()
        }
    }

    Component.onCompleted: {
        SharingServiceClient.refresh()
    }
}
