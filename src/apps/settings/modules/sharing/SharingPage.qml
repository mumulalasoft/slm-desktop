import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Slm_Desktop
import "file:/usr/lib/settings/components"

Flickable {
    id: root
    contentHeight: contentColumn.height
    clip: true

    // ── DBus connections ──────────────────────────────────────────────────────

    property var sharingIface: null
    property var nearbyIface: null
    property var trustIface: null

    // Feature states – populated from GetStatus()
    property bool fileSharingEnabled: false
    property bool nearbySharingEnabled: false
    property bool screenSharingEnabled: false
    property bool printerSharingEnabled: false
    property bool remoteAccessEnabled: false
    property bool mediaSharingEnabled: false
    property bool clipboardSharingEnabled: false

    // Nearby devices list
    property var nearbyDevices: []
    property bool nearbyDiscovering: false

    // Trusted devices list
    property var trustedDevices: []

    // Shared folders list
    property var sharedFolders: []

    // Environment state
    property var envStatus: ({})
    property bool serviceAvailable: false

    // ── Helpers ───────────────────────────────────────────────────────────────

    function refreshStatus() {
        if (!root.sharingIface) return
        const r = root.sharingIface.GetStatus()
        if (r && r.ok) {
            const f = r.features || {}
            root.fileSharingEnabled      = !!f["file-sharing"]
            root.nearbySharingEnabled    = !!f["nearby-sharing"]
            root.screenSharingEnabled    = !!f["screen-sharing"]
            root.printerSharingEnabled   = !!f["printer-sharing"]
            root.remoteAccessEnabled     = !!f["remote-access"]
            root.mediaSharingEnabled     = !!f["media-sharing"]
            root.clipboardSharingEnabled = !!f["clipboard-sharing"]
        }
    }

    function refreshFolders() {
        if (!root.sharingIface) return
        const r = root.sharingIface.ListSharedFolders()
        if (r && r.ok) {
            const folders = r.folders || {}
            root.sharedFolders = Object.keys(folders).map(function(p) {
                return Object.assign({ path: p }, folders[p])
            })
        }
    }

    function refreshNearby() {
        if (!root.nearbyIface) return
        const r = root.nearbyIface.GetDevices()
        if (r && r.ok) root.nearbyDevices = r.devices || []
    }

    function refreshTrusted() {
        if (!root.trustIface) return
        const r = root.trustIface.GetTrustedDevices()
        if (r && r.ok) root.trustedDevices = r.devices || []
    }

    function setFeature(feature, enabled) {
        if (!root.sharingIface) return
        root.sharingIface.SetFeatureEnabled(feature, enabled)
        refreshStatus()
    }

    function deviceTypeIcon(type) {
        switch (String(type)) {
        case "phone":   return "phone"
        case "tablet":  return "tablet"
        case "tv":      return "tv"
        case "printer": return "printer"
        case "laptop":  return "computer-laptop"
        default:        return "computer"
        }
    }

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

        // ── 1. File Sharing ──────────────────────────────────────────────────
        SettingGroup {
            title: qsTr("File Sharing")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("Share files with nearby devices")
                description: qsTr("Other devices on this network can browse your shared folders")

                SettingToggle {
                    checked: root.fileSharingEnabled
                    onToggled: root.setFeature("file-sharing", checked)
                }
            }

            // Shared folders list
            Repeater {
                model: root.sharedFolders
                delegate: SettingCard {
                    required property var modelData
                    label: modelData.path.split("/").pop() || modelData.path
                    description: modelData.path

                    Row {
                        spacing: 8

                        Text {
                            text: modelData.access === "rw" ? qsTr("Read & Write") : qsTr("Read Only")
                            color: Theme.color("textSecondary")
                            font.pixelSize: Theme.fontSize("small")
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Button {
                            text: qsTr("Remove")
                            flat: true
                            onClicked: {
                                if (root.sharingIface) {
                                    root.sharingIface.RemoveSharedFolder(modelData.path)
                                    root.refreshFolders()
                                }
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
                    enabled: root.fileSharingEnabled
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
                    checked: root.nearbySharingEnabled
                    onToggled: root.setFeature("nearby-sharing", checked)
                }
            }

            SettingCard {
                label: qsTr("Visible devices")
                description: root.nearbyDevices.length === 0
                    ? qsTr("No devices found nearby")
                    : qsTr("%1 device(s) visible").arg(root.nearbyDevices.length)

                Button {
                    text: root.nearbyDiscovering ? qsTr("Stop") : qsTr("Scan")
                    flat: true
                    enabled: root.nearbySharingEnabled
                    onClicked: {
                        if (!root.nearbyIface) return
                        if (root.nearbyDiscovering) {
                            root.nearbyIface.StopDiscovery()
                            root.nearbyDiscovering = false
                        } else {
                            root.nearbyIface.StartDiscovery()
                            root.nearbyDiscovering = true
                            Qt.callLater(root.refreshNearby)
                        }
                    }
                }
            }

            Repeater {
                model: root.nearbyDevices
                delegate: SettingCard {
                    required property var modelData
                    label: modelData.name || modelData.deviceId
                    description: root.trustLevelLabel(modelData.trustLevel)

                    Row {
                        spacing: 8

                        Button {
                            text: qsTr("Send File")
                            flat: true
                            enabled: String(modelData.trustLevel) !== "blocked"
                            onClicked: sendFileDialog.targetDeviceId = modelData.deviceId
                        }

                        Button {
                            text: String(modelData.trustLevel) === "blocked"
                                  ? qsTr("Unblock")
                                  : (String(modelData.trustLevel) === "untrusted" ? qsTr("Trust") : qsTr("Manage"))
                            flat: true
                            onClicked: {
                                if (!root.trustIface) return
                                if (String(modelData.trustLevel) === "blocked") {
                                    root.trustIface.UnblockDevice(modelData.deviceId)
                                } else if (String(modelData.trustLevel) === "untrusted") {
                                    root.trustIface.PairDevice(modelData.deviceId)
                                }
                                root.refreshNearby()
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
                    checked: root.screenSharingEnabled
                    onToggled: root.setFeature("screen-sharing", checked)
                }
            }

            SettingCard {
                label: qsTr("Require approval for each session")
                description: qsTr("A confirmation dialog appears every time a device requests access")

                SettingToggle {
                    checked: true
                    enabled: root.screenSharingEnabled
                }
            }

            SettingCard {
                label: qsTr("Allow remote interaction")
                description: qsTr("Connected devices can control mouse and keyboard, not just view")

                SettingToggle {
                    checked: false
                    enabled: root.screenSharingEnabled
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
                    checked: root.printerSharingEnabled
                    onToggled: root.setFeature("printer-sharing", checked)
                }
            }

            SettingCard {
                label: qsTr("Network-discoverable printers")
                description: root.printerSharingEnabled
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
                    checked: root.remoteAccessEnabled
                    onToggled: root.setFeature("remote-access", checked)
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
                    checked: root.mediaSharingEnabled
                    onToggled: root.setFeature("media-sharing", checked)
                }
            }

            SettingCard {
                label: qsTr("Cast target")
                description: qsTr("Appear as a cast destination for other devices")

                SettingToggle {
                    checked: false
                    enabled: root.mediaSharingEnabled
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
                    checked: root.clipboardSharingEnabled
                    onToggled: root.setFeature("clipboard-sharing", checked)
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
                description: root.trustedDevices.length === 0
                    ? qsTr("No trusted devices")
                    : qsTr("%1 trusted device(s)").arg(root.trustedDevices.length)
                Layout.fillWidth: true
            }

            Repeater {
                model: root.trustedDevices
                delegate: SettingCard {
                    required property var modelData
                    label: modelData.name || modelData.deviceId
                    description: root.trustLevelLabel(modelData.trustLevel)
                        + " · " + qsTr("Last seen %1").arg(modelData.lastSeenAt || qsTr("never"))

                    Row {
                        spacing: 8

                        Button {
                            text: qsTr("Revoke")
                            flat: true
                            onClicked: {
                                if (root.trustIface) {
                                    root.trustIface.UnpairDevice(modelData.deviceId)
                                    root.refreshTrusted()
                                }
                            }
                        }

                        Button {
                            text: qsTr("Block")
                            flat: true
                            onClicked: {
                                if (root.trustIface) {
                                    root.trustIface.BlockDevice(modelData.deviceId)
                                    root.refreshTrusted()
                                }
                            }
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
            if (!folderPathField.text || !root.sharingIface) return
            root.sharingIface.AddSharedFolder(folderPathField.text, {
                "access": accessMode.currentIndex === 0 ? "ro" : "rw",
                "guestAccess": guestAccessCheck.checked
            })
            root.refreshFolders()
            folderPathField.text = ""
        }
    }

    QtObject {
        id: sendFileDialog
        property string targetDeviceId: ""
        onTargetDeviceIdChanged: {
            if (targetDeviceId) {
                // In a full implementation this opens a file picker portal,
                // then calls root.nearbyIface.SendFileTo(targetDeviceId, pickedPath)
                targetDeviceId = ""
            }
        }
    }

    // ── Initialisation ────────────────────────────────────────────────────────

    Component.onCompleted: {
        root.serviceAvailable = true
        refreshStatus()
        refreshFolders()
        refreshTrusted()
    }
}
