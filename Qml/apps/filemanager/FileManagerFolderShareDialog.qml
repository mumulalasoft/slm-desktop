import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
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

    title: "Bagikan folder di jaringan"
    standardButtons: Dialog.NoButton
    dialogWidth: 560
    property real maxBodyHeight: Math.max(260, Math.min((hostRoot ? hostRoot.height : 720) - 250, 520))
    x: Math.round((hostRoot.width - width) * 0.5)
    y: Math.round((hostRoot.height - height) * 0.5)
    bodyPadding: 14
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
                spacing: 10

                Label {
                    Layout.fillWidth: true
                    text: successState
                          ? "Folder sekarang dibagikan di jaringan"
                          : "Perangkat lain di jaringan lokal dapat membuka folder ini."
                    wrapMode: Text.WordWrap
                    color: Theme.color("textSecondary")
                }

                Rectangle {
                    Layout.fillWidth: true
                    visible: !successState && !envReady
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
                        Button {
                            text: "Periksa lagi"
                            onClicked: root.refreshEnvironmentStatus()
                        }
                        Button {
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

                    Label {
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
                    visible: !successState
                    radius: Theme.radiusMd
                    color: Theme.color("fileManagerSearchBg")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("fileManagerControlBorder")
                    implicitHeight: 44

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10

                    Label {
                        Layout.fillWidth: true
                        text: "Bagikan folder ini"
                        color: Theme.color("textPrimary")
                    }
                    Switch {
                        checked: root.sharingEnabled
                        onToggled: root.sharingEnabled = checked
                    }
                }
            }

                ColumnLayout {
                    visible: !successState && root.sharingEnabled
                    spacing: 8

                Label {
                    text: "Nama folder di jaringan"
                    color: Theme.color("textPrimary")
                }
                TextField {
                    Layout.fillWidth: true
                    text: root.shareName
                    onTextChanged: root.shareName = text
                    placeholderText: hostRoot.basename(root.targetPath)
                }
                Label {
                    Layout.fillWidth: true
                    text: "Nama ini akan terlihat dari perangkat lain"
                    color: Theme.color("textSecondary")
                    font.pixelSize: Theme.fontSize("small")
                }

                Label {
                    text: "Akses"
                    color: Theme.color("textPrimary")
                    topPadding: 4
                }
                ButtonGroup { id: accessGroup }
                RadioButton {
                    text: "Hanya saya"
                    checked: root.accessMode === "owner"
                    ButtonGroup.group: accessGroup
                    onToggled: if (checked) { root.accessMode = "owner" }
                }
                RadioButton {
                    text: "Siapa pun di jaringan ini"
                    checked: root.accessMode === "anyone"
                    ButtonGroup.group: accessGroup
                    onToggled: if (checked) { root.accessMode = "anyone" }
                }
                Label {
                    visible: root.accessMode === "anyone"
                    Layout.fillWidth: true
                    text: "Siapa pun di jaringan ini dapat membuka folder ini. Pastikan Anda mempercayai jaringan yang digunakan."
                    color: Theme.color("warning")
                    wrapMode: Text.WordWrap
                    font.pixelSize: Theme.fontSize("small")
                }
                RadioButton {
                    text: "Pengguna tertentu"
                    checked: root.accessMode === "users"
                    ButtonGroup.group: accessGroup
                    onToggled: if (checked) { root.accessMode = "users" }
                }
                TextField {
                    Layout.fillWidth: true
                    visible: root.accessMode === "users"
                    placeholderText: "Pisahkan nama pengguna dengan koma"
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

                Label {
                    text: "Izin"
                    color: Theme.color("textPrimary")
                    topPadding: 4
                }
                ButtonGroup { id: permissionGroup }
                RadioButton {
                    text: "Hanya lihat"
                    checked: root.permissionMode === "read"
                    ButtonGroup.group: permissionGroup
                    onToggled: if (checked) { root.permissionMode = "read" }
                }
                RadioButton {
                    text: "Bisa mengubah file"
                    checked: root.permissionMode === "write"
                    ButtonGroup.group: permissionGroup
                    onToggled: if (checked) { root.permissionMode = "write" }
                }

                CheckBox {
                    checked: root.allowGuest
                    text: "Izinkan akses tanpa login"
                    onToggled: root.allowGuest = checked
                }
                Label {
                    Layout.fillWidth: true
                    text: "Cocok untuk jaringan rumah yang dipercaya"
                    color: Theme.color("textSecondary")
                    font.pixelSize: Theme.fontSize("small")
                }

                Label {
                    Layout.fillWidth: true
                    text: "Pengaturan lanjutan"
                    color: Theme.color("accent")
                    font.pixelSize: Theme.fontSize("small")
                    opacity: 0.9
                }
            }

                ColumnLayout {
                    visible: successState
                    spacing: 8
                    Label {
                        Layout.fillWidth: true
                        text: "Alamat folder: " + String(root.successAddress || "-")
                        color: Theme.color("textPrimary")
                        wrapMode: Text.WrapAnywhere
                    }
                    Label {
                        Layout.fillWidth: true
                        text: "Buka dari Windows Explorer, Finder, atau file manager Linux lain di jaringan yang sama."
                        color: Theme.color("textSecondary")
                        wrapMode: Text.WordWrap
                    }
                }

                Label {
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

            Button {
                visible: !successState
                text: "Batal"
                onClicked: root.close()
            }
            Button {
                visible: successState
                text: "Tutup"
                onClicked: root.close()
            }
            Button {
                visible: successState
                text: "Salin alamat"
                onClicked: root.hostRoot.copyFolderShareAddress(root.targetPath)
            }
            Button {
                visible: successState
                text: "Hentikan berbagi"
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
            Button {
                visible: !successState
                highlighted: true
                text: "Selesai"
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
