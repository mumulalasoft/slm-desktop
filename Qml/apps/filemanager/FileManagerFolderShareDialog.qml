import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle as DSStyle
import "../../components/system"

AppDialog {
    id: root
    required property var hostRoot

    property string targetPath: ""
    property string shareName: ""
    property bool sharingEnabled: false
    property string accessMode: "owner" // owner | anyone | users
    property string permissionMode: "read" // read | write
    property bool allowGuest: false
    property var selectedUsers: []
    property string errorText: ""
    property bool successState: false
    property string successAddress: ""
    property bool envReady: true
    property var envIssues: []
    property string envMessage: ""
    property bool showTechnicalDetails: false
    property string technicalDetailsText: ""
    property bool installInProgress: false
    property string installStatusText: ""

    function openForPath(pathValue) {
        targetPath = String(pathValue || "")
        successState = false
        successAddress = ""
        errorText = ""
        if (hostRoot && hostRoot.prepareFolderShareDialog) {
            hostRoot.prepareFolderShareDialog(targetPath)
        }
        refreshEnvironmentStatus()
        open()
    }

    function applyFromInfo(info) {
        var row = info || ({})
        sharingEnabled = !!row.enabled
        shareName = String(row.shareName || hostRoot.basename(targetPath))
        var rawAccess = String(row.access || "owner")
        if (rawAccess === "all") {
            accessMode = "anyone"
        } else if (rawAccess === "specific") {
            accessMode = "users"
        } else {
            accessMode = "owner"
        }
        permissionMode = String(row.permission || "read")
        allowGuest = !!row.allowGuest
        selectedUsers = row.users || []
        successAddress = String(row.address || "")
    }

    function friendlyError(res, fallbackText) {
        var code = String((res && res.error) ? res.error : "")
        var backendMsg = String((res && res.backendMessage) ? res.backendMessage : "")
        if (code === "samba-net-not-found") {
            return "Fitur berbagi belum siap di perangkat ini. Komponen berbagi jaringan belum terpasang."
        }
        if (code === "usershare-permission-denied") {
            return "Tidak punya izin untuk mengaktifkan berbagi folder. Pastikan akun Anda diizinkan berbagi folder di jaringan."
        }
        if (code === "usershare-not-enabled") {
            return "Berbagi jaringan belum diaktifkan pada sistem. Aktifkan layanan berbagi dulu, lalu coba lagi."
        }
        if (code === "usershare-invalid-config") {
            return "Pengaturan berbagi jaringan sistem belum valid. Periksa konfigurasi layanan berbagi."
        }
        if (code === "usershare-timeout") {
            return "Layanan berbagi sedang tidak merespons. Coba lagi beberapa saat."
        }
        if (code === "daemon-unavailable") {
            return "Layanan berbagi desktop belum berjalan. Coba restart sesi desktop."
        }
        if (backendMsg.length > 0) {
            return backendMsg
        }
        if (code.length > 0) {
            return "Tidak dapat menyimpan pengaturan berbagi (" + code + ")."
        }
        return String(fallbackText || "Gagal menyimpan pengaturan berbagi")
    }

    function refreshEnvironmentStatus() {
        envReady = true
        envIssues = []
        envMessage = ""
        technicalDetailsText = ""
        showTechnicalDetails = false
        installStatusText = ""
        if (!hostRoot || !hostRoot.folderSharingEnvironment) {
            return
        }
        var env = hostRoot.folderSharingEnvironment()
        envReady = !!env.ready
        envIssues = env.issues || []
        if (!envReady) {
            if (envIssues.length > 0) {
                envMessage = String(envIssues[0].message || "Berbagi jaringan belum siap.")
            } else {
                envMessage = "Berbagi jaringan belum siap."
            }
            var lines = []
            if (String(env.error || "").length > 0) {
                lines.push("error: " + String(env.error))
            }
            if (String(env.backendError || "").length > 0) {
                lines.push("backendError: " + String(env.backendError))
            }
            for (var i = 0; i < envIssues.length; ++i) {
                var issue = envIssues[i] || ({})
                lines.push("issue[" + i + "]: "
                           + String(issue.code || "unknown")
                           + " - "
                           + String(issue.message || ""))
            }
            technicalDetailsText = lines.join("\n")
        }
    }

    function issueGuidanceSteps(code) {
        var c = String(code || "")
        if (c === "samba-net-not-found") {
            return [
                        "Pasang komponen berbagi jaringan (paket Samba) di sistem.",
                        "Setelah terpasang, klik \"Periksa lagi\"."
                    ]
        }
        if (c === "usershare-permission-denied") {
            return [
                        "Pastikan akun Anda termasuk grup yang diizinkan berbagi folder (biasanya: sambashare).",
                        "Logout lalu login kembali agar izin grup aktif.",
                        "Klik \"Periksa lagi\" setelah login kembali."
                    ]
        }
        if (c === "usershare-not-enabled") {
            return [
                        "Aktifkan fitur usershare pada konfigurasi Samba sistem.",
                        "Pastikan direktori usershare tersedia dan dapat ditulis.",
                        "Klik \"Periksa lagi\"."
                    ]
        }
        if (c === "usershare-invalid-config") {
            return [
                        "Periksa konfigurasi berbagi jaringan pada sistem.",
                        "Perbaiki nilai konfigurasi usershare yang tidak valid.",
                        "Klik \"Periksa lagi\"."
                    ]
        }
        if (c === "usershare-timeout") {
            return [
                        "Pastikan layanan berbagi jaringan berjalan normal.",
                        "Tutup dialog lalu coba lagi setelah beberapa saat."
                    ]
        }
        if (c === "daemon-unavailable") {
            return [
                        "Pastikan service desktop daemon berjalan.",
                        "Restart sesi desktop jika perlu, lalu klik \"Periksa lagi\"."
                    ]
        }
        return [
                    "Periksa konfigurasi berbagi jaringan sistem.",
                    "Coba lagi setelah meninjau status layanan."
                ]
    }

    function missingComponentIssues() {
        var mapped = []
        for (var i = 0; i < (envIssues || []).length; ++i) {
            var issue = envIssues[i] || ({})
            var code = String(issue.code || "")
            var componentId = String(issue.componentId || "")
            var installable = !!issue.autoInstallable || (code === "samba-net-not-found")
            var title = String(issue.title || "")
            if (title.length === 0) {
                title = (componentId.length > 0) ? componentId : code
            }
            var guidance = String(issue.guidance || "")
            if (guidance.length === 0) {
                guidance = String(root.issueGuidanceSteps(code).join(" "))
            }
            mapped.push({
                componentId: componentId.length > 0 ? componentId : (installable ? "samba" : code),
                autoInstallable: installable,
                title: title,
                description: String(issue.message || ""),
                guidance: guidance,
                packageName: String(issue.packageName || (installable ? "samba" : ""))
            })
        }
        return mapped
    }

    title: "Share Folder"
    standardButtons: Dialog.NoButton
    dialogWidth: 540
    property real maxBodyHeight: Math.max(260, Math.min((hostRoot ? hostRoot.height : 720) - 250, 520))
    x: Math.round((hostRoot.width - width) * 0.5)
    y: Math.round((hostRoot.height - height) * 0.5)
    bodyPadding: 12
    footerPadding: 12

    bodyComponent: Component {
        Flickable {
            id: bodyFlick
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentHeight > height
            contentWidth: width
            contentHeight: contentColumn.implicitHeight
            implicitHeight: Math.min(contentColumn.implicitHeight, root.maxBodyHeight)

            ScrollBar.vertical: ScrollBar {
                policy: bodyFlick.contentHeight > bodyFlick.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
            }

            ColumnLayout {
                id: contentColumn
                width: bodyFlick.width
                spacing: 12

                DSStyle.Label {
                    Layout.fillWidth: true
                    text: root.targetPath
                          ? hostRoot.basename(root.targetPath)
                          : "Folder"
                    color: Theme.color("textPrimary")
                    font.pixelSize: Theme.fontSize("title")
                    elide: Text.ElideMiddle
                }

                DSStyle.Label {
                    Layout.fillWidth: true
                    text: root.successState
                          ? "Folder ini tersedia untuk perangkat lain di jaringan lokal."
                          : "Atur nama, akses, dan izin sebelum folder terlihat di jaringan."
                    wrapMode: Text.WordWrap
                    color: Theme.color("textSecondary")
                    font.pixelSize: Theme.fontSize("small")
                }

                Rectangle {
                    Layout.fillWidth: true
                    visible: !root.successState && !root.envReady
                    radius: Theme.radiusMd
                    color: Qt.rgba(1.0, 0.76, 0.0, 0.12)
                    border.width: Theme.borderWidthThin
                    border.color: Qt.rgba(1.0, 0.76, 0.0, 0.45)
                    implicitHeight: warningCol.implicitHeight + 14

                    ColumnLayout {
                        id: warningCol
                        anchors.fill: parent
                        anchors.margins: 7
                        spacing: 6

                        MissingComponentsCard {
                            Layout.fillWidth: true
                            issues: root.missingComponentIssues()
                            summaryText: root.envMessage.length > 0
                                         ? root.envMessage
                                         : "Berbagi jaringan belum siap."
                            showPackageName: true
                            busy: root.installInProgress
                            statusText: root.installStatusText
                            cardColor: Qt.rgba(1, 1, 1, 0.06)
                            cardBorderColor: Qt.rgba(1, 1, 1, 0.25)
                            titleColor: Theme.color("textPrimary")
                            detailColor: Theme.color("textSecondary")
                            statusColor: Theme.color("textSecondary")
                            onInstallRequested: function(componentId) {
                                root.installInProgress = true
                                root.installStatusText = ""
                                var installRes = root.hostRoot.installMissingComponent(componentId)
                                root.installInProgress = false
                                if (!!installRes && !!installRes.ok) {
                                    root.installStatusText = "Komponen berhasil dipasang. Memeriksa ulang..."
                                    root.refreshEnvironmentStatus()
                                    if (root.envReady) {
                                        root.errorText = ""
                                    }
                                } else {
                                    root.installStatusText = root.friendlyError(installRes, "Gagal memasang komponen")
                                    root.errorText = root.installStatusText
                                }
                            }
                        }

                        RowLayout {
                            spacing: 8
                            DSStyle.Button {
                                text: "Periksa lagi"
                                onClicked: root.refreshEnvironmentStatus()
                            }
                            DSStyle.Button {
                                text: "Perbaiki otomatis"
                                onClicked: {
                                    var res = root.hostRoot.repairFolderSharingEnvironment()
                                    root.envReady = !!res.ready
                                    root.envIssues = res.issues || []
                                    if (!root.envReady) {
                                        if (root.envIssues.length > 0) {
                                            root.errorText = String(root.envIssues[0].message || "Berbagi jaringan belum siap.")
                                        } else {
                                            root.errorText = root.friendlyError(res, "Berbagi jaringan belum siap.")
                                        }
                                        var lines = []
                                        if (String(res.error || "").length > 0) {
                                            lines.push("error: " + String(res.error))
                                        }
                                        var actions = res.actions || []
                                        for (var j = 0; j < actions.length; ++j) {
                                            var action = actions[j] || ({})
                                            lines.push("action[" + j + "]: "
                                                       + String(action.id || "unknown")
                                                       + " ok=" + String(!!action.ok)
                                                       + " msg=" + String(action.message || ""))
                                        }
                                        for (var k = 0; k < root.envIssues.length; ++k) {
                                            var issue = root.envIssues[k] || ({})
                                            lines.push("issue[" + k + "]: "
                                                       + String(issue.code || "unknown")
                                                       + " - "
                                                       + String(issue.message || ""))
                                        }
                                        root.technicalDetailsText = lines.join("\n")
                                    } else {
                                        root.errorText = ""
                                        root.envMessage = ""
                                        root.technicalDetailsText = ""
                                        root.showTechnicalDetails = false
                                    }
                                }
                            }
                        }

                        DSStyle.Label {
                            Layout.fillWidth: true
                            text: root.showTechnicalDetails ? "Sembunyikan detail teknis" : "Tampilkan detail teknis"
                            color: Theme.color("accent")
                            font.pixelSize: Theme.fontSize("small")
                            visible: root.technicalDetailsText.length > 0

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.showTechnicalDetails = !root.showTechnicalDetails
                            }
                        }

                        TextArea {
                            Layout.fillWidth: true
                            visible: root.showTechnicalDetails && root.technicalDetailsText.length > 0
                            readOnly: true
                            wrapMode: TextEdit.WrapAnywhere
                            text: root.technicalDetailsText
                            font.family: "monospace"
                            font.pixelSize: Theme.fontSize("small")
                            implicitHeight: 120
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    visible: !root.successState
                    radius: Theme.radiusControl
                    color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.045)
                    border.width: Theme.borderWidthThin
                    border.color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.10)
                    implicitHeight: shareSwitchRow.implicitHeight + 16

                    RowLayout {
                        id: shareSwitchRow
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        anchors.topMargin: 8
                        anchors.bottomMargin: 8
                        spacing: 10

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            DSStyle.Label {
                                Layout.fillWidth: true
                                text: "Share this folder"
                                color: Theme.color("textPrimary")
                            }
                            DSStyle.Label {
                                Layout.fillWidth: true
                                text: root.sharingEnabled
                                      ? "Visible on the local network"
                                      : "Private until sharing is enabled"
                                color: Theme.color("textSecondary")
                                font.pixelSize: Theme.fontSize("small")
                            }
                        }

                        DSStyle.Switch {
                            checked: root.sharingEnabled
                            onToggled: root.sharingEnabled = checked
                        }
                    }
                }

                ColumnLayout {
                    visible: !root.successState && root.sharingEnabled
                    spacing: 10

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 10
                        rowSpacing: 6

                        DSStyle.Label {
                            text: "Name:"
                            color: Theme.color("textPrimary")
                            Layout.preferredWidth: 94
                            horizontalAlignment: Text.AlignRight
                        }
                        DSStyle.TextField {
                            Layout.fillWidth: true
                            text: root.shareName
                            onTextChanged: root.shareName = text
                            placeholderText: hostRoot.basename(root.targetPath)
                        }

                        Item { Layout.preferredWidth: 94 }
                        DSStyle.Label {
                            Layout.fillWidth: true
                            text: "This is the name other devices will see."
                            color: Theme.color("textSecondary")
                            font.pixelSize: Theme.fontSize("small")
                        }
                    }

                    DSStyle.Label {
                        Layout.fillWidth: true
                        text: "Access"
                        color: Theme.color("textSecondary")
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        radius: Theme.radiusControl
                        color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.045) : Qt.rgba(0, 0, 0, 0.035)
                        border.width: Theme.borderWidthThin
                        border.color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(0, 0, 0, 0.09)
                        implicitHeight: accessColumn.implicitHeight + 8

                        ButtonGroup { id: accessGroup }
                        ColumnLayout {
                            id: accessColumn
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 4
                            spacing: 0

                            DSStyle.RadioButton {
                                Layout.fillWidth: true
                                text: "Only me"
                                checked: root.accessMode === "owner"
                                ButtonGroup.group: accessGroup
                                onToggled: if (checked) { root.accessMode = "owner" }
                            }
                            DSStyle.RadioButton {
                                Layout.fillWidth: true
                                text: "Anyone on this network"
                                checked: root.accessMode === "anyone"
                                ButtonGroup.group: accessGroup
                                onToggled: if (checked) { root.accessMode = "anyone" }
                            }
                            DSStyle.Label {
                                visible: root.accessMode === "anyone"
                                Layout.fillWidth: true
                                Layout.leftMargin: 34
                                Layout.rightMargin: 10
                                Layout.bottomMargin: 6
                                text: "Use this only on a network you trust."
                                color: Theme.color("warning")
                                wrapMode: Text.WordWrap
                                font.pixelSize: Theme.fontSize("small")
                            }
                            DSStyle.RadioButton {
                                Layout.fillWidth: true
                                text: "Specific users"
                                checked: root.accessMode === "users"
                                ButtonGroup.group: accessGroup
                                onToggled: if (checked) { root.accessMode = "users" }
                            }
                            DSStyle.TextField {
                                Layout.fillWidth: true
                                Layout.leftMargin: 34
                                Layout.rightMargin: 10
                                Layout.bottomMargin: 8
                                visible: root.accessMode === "users"
                                placeholderText: "Separate user names with commas"
                                text: (root.selectedUsers || []).join(", ")
                                onTextChanged: {
                                    var out = []
                                    var raw = String(text || "").split(",")
                                    for (var i = 0; i < raw.length; ++i) {
                                        var v = String(raw[i] || "").trim()
                                        if (v.length > 0) {
                                            out.push(v)
                                        }
                                    }
                                    root.selectedUsers = out
                                }
                            }
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 10
                        rowSpacing: 6

                        DSStyle.Label {
                            text: "Permission:"
                            color: Theme.color("textPrimary")
                            Layout.preferredWidth: 94
                            horizontalAlignment: Text.AlignRight
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            ButtonGroup { id: permissionGroup }
                            DSStyle.RadioButton {
                                text: "Read only"
                                checked: root.permissionMode === "read"
                                ButtonGroup.group: permissionGroup
                                onToggled: if (checked) { root.permissionMode = "read" }
                            }
                            DSStyle.RadioButton {
                                text: "Can make changes"
                                checked: root.permissionMode === "write"
                                ButtonGroup.group: permissionGroup
                                onToggled: if (checked) { root.permissionMode = "write" }
                            }
                        }

                        Item { Layout.preferredWidth: 94 }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            DSStyle.CheckBox {
                                checked: root.allowGuest
                                text: "Allow guest access"
                                onToggled: root.allowGuest = checked
                            }
                            DSStyle.Label {
                                Layout.fillWidth: true
                                text: "Best for trusted home or studio networks."
                                color: Theme.color("textSecondary")
                                font.pixelSize: Theme.fontSize("small")
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    visible: root.successState
                    radius: Theme.radiusControl
                    color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.045)
                    border.width: Theme.borderWidthThin
                    border.color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.10)
                    implicitHeight: successColumn.implicitHeight + 20

                    ColumnLayout {
                        id: successColumn
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 6

                        DSStyle.Label {
                            Layout.fillWidth: true
                            text: "Network Address"
                            color: Theme.color("textSecondary")
                        }

                        DSStyle.TextField {
                            Layout.fillWidth: true
                            readOnly: true
                            selectByMouse: true
                            text: String(root.successAddress || "-")
                        }

                        DSStyle.Label {
                            Layout.fillWidth: true
                            text: "Open this address from Finder, Windows Explorer, or another file manager on the same network."
                            color: Theme.color("textSecondary")
                            wrapMode: Text.WordWrap
                            font.pixelSize: Theme.fontSize("small")
                        }
                    }
                }

                DSStyle.Label {
                    Layout.fillWidth: true
                    visible: String(root.errorText || "").length > 0
                    text: root.errorText
                    color: Theme.color("error")
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    footerComponent: Component {
        RowLayout {
            spacing: 8
            Item { Layout.fillWidth: true }

            DSStyle.Button {
                visible: !successState
                text: "Cancel"
                onClicked: root.close()
            }
            DSStyle.Button {
                visible: successState
                text: "Done"
                onClicked: root.close()
            }
            DSStyle.Button {
                visible: successState
                text: "Copy Address"
                onClicked: root.hostRoot.copyFolderShareAddress(root.targetPath)
            }
            DSStyle.Button {
                visible: successState
                text: "Stop Sharing"
                onClicked: {
                    var res = root.hostRoot.disableFolderShare(root.targetPath)
                    if (!!res && !!res.ok) {
                        successState = false
                        root.applyFromInfo(root.hostRoot.folderShareInfoForPath(root.targetPath))
                    } else {
                        root.errorText = root.friendlyError(res, "Gagal menghentikan berbagi")
                    }
                }
            }
            DSStyle.Button {
                visible: !successState
                highlighted: true
                text: root.sharingEnabled ? "Share" : "Done"
                onClicked: {
                    root.errorText = ""
                    if (!!root.sharingEnabled) {
                        root.refreshEnvironmentStatus()
                    }
                    if (!!root.sharingEnabled && !root.envReady) {
                        root.errorText = root.envMessage.length > 0
                                ? root.envMessage
                                : "Berbagi jaringan belum siap."
                        return
                    }
                    var payload = {
                        "enabled": root.sharingEnabled,
                        "shareName": String(root.shareName || ""),
                        "access": String(root.accessMode || "owner"),
                        "permission": String(root.permissionMode || "read"),
                        "allowGuest": !!root.allowGuest,
                        "users": root.selectedUsers || []
                    }
                    var res = root.hostRoot.applyFolderShareConfig(root.targetPath, payload)
                    if (!!res && !!res.ok) {
                        root.successState = !!root.sharingEnabled
                        root.successAddress = String(res.address || "")
                        if (!root.successState) {
                            root.close()
                        }
                    } else {
                        root.successState = false
                        root.errorText = root.friendlyError(res, "Gagal menyimpan pengaturan berbagi")
                    }
                }
            }
        }
    }
}
