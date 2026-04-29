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
    readonly property int iconRevision: ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                                         ? ThemeIconController.revision : 0)

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

    function iconSource(name) {
        return "image://themeicon/" + String(name || "folder-symbolic") + "?v=" + root.iconRevision
    }

    function accessTitle() {
        if (accessMode === "anyone") {
            return "Anyone on this network"
        }
        if (accessMode === "users") {
            return "Specific users"
        }
        return "Only me"
    }

    function permissionTitle() {
        return permissionMode === "write" ? "Can make changes" : "Read only"
    }

    title: "Share Folder"
    standardButtons: Dialog.NoButton
    dialogWidth: 560
    property real maxBodyHeight: Math.max(300, Math.min((hostRoot ? hostRoot.height : 720) - 250, 600))
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

                Rectangle {
                    Layout.fillWidth: true
                    radius: Theme.radiusControlLarge
                    color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.055) : Qt.rgba(0, 0, 0, 0.035)
                    border.width: Theme.borderWidthThin
                    border.color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.085)
                    implicitHeight: folderHeader.implicitHeight + 22

                    RowLayout {
                        id: folderHeader
                        anchors.fill: parent
                        anchors.margins: 11
                        spacing: 12

                        Rectangle {
                            Layout.preferredWidth: 44
                            Layout.preferredHeight: 44
                            radius: Theme.radiusControl
                            color: Theme.color("accentSoft")
                            border.width: Theme.borderWidthThin
                            border.color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.08)

                            Image {
                                anchors.centerIn: parent
                                width: 26
                                height: 26
                                sourceSize.width: 26
                                sourceSize.height: 26
                                source: root.iconSource("folder-symbolic")
                                opacity: Theme.opacityGhost
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            DSStyle.Label {
                                Layout.fillWidth: true
                                text: root.targetPath ? hostRoot.basename(root.targetPath) : "Folder"
                                color: Theme.color("textPrimary")
                                font.pixelSize: Theme.fontSize("title")
                                font.weight: Theme.fontWeight("semibold")
                                elide: Text.ElideMiddle
                            }

                            DSStyle.Label {
                                Layout.fillWidth: true
                                text: root.targetPath
                                visible: root.targetPath.length > 0
                                color: Theme.color("textSecondary")
                                font.pixelSize: Theme.fontSize("small")
                                elide: Text.ElideMiddle
                            }
                        }

                        Rectangle {
                            Layout.preferredWidth: statusLabel.implicitWidth + 18
                            Layout.preferredHeight: 24
                            radius: height / 2
                            color: root.successState || root.sharingEnabled
                                   ? Qt.rgba(0.16, 0.78, 0.25, Theme.darkMode ? 0.18 : 0.14)
                                   : Qt.rgba(0, 0, 0, Theme.darkMode ? 0.18 : 0.06)
                            border.width: Theme.borderWidthThin
                            border.color: root.successState || root.sharingEnabled
                                          ? Qt.rgba(0.16, 0.78, 0.25, 0.42)
                                          : Theme.color("panelBorder")

                            DSStyle.Label {
                                id: statusLabel
                                anchors.centerIn: parent
                                text: root.successState || root.sharingEnabled ? "Shared" : "Private"
                                color: root.successState || root.sharingEnabled
                                       ? Theme.color("success")
                                       : Theme.color("textSecondary")
                                font.pixelSize: Theme.fontSize("xs")
                                font.weight: Theme.fontWeight("semibold")
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    visible: !root.successState && !root.envReady
                    radius: Theme.radiusControl
                    color: Qt.rgba(1.0, 0.76, 0.0, Theme.darkMode ? 0.11 : 0.13)
                    border.width: Theme.borderWidthThin
                    border.color: Qt.rgba(1.0, 0.76, 0.0, 0.44)
                    implicitHeight: warningCol.implicitHeight + 16

                    ColumnLayout {
                        id: warningCol
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8

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
                            font.family: Theme.fontFamilyMonospace
                            font.pixelSize: Theme.fontSize("small")
                            implicitHeight: 120
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    visible: root.successState
                    radius: Theme.radiusControlLarge
                    color: Qt.rgba(0.16, 0.78, 0.25, Theme.darkMode ? 0.12 : 0.09)
                    border.width: Theme.borderWidthThin
                    border.color: Qt.rgba(0.16, 0.78, 0.25, 0.36)
                    implicitHeight: successColumn.implicitHeight + 22

                    ColumnLayout {
                        id: successColumn
                        anchors.fill: parent
                        anchors.margins: 11
                        spacing: 8

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Rectangle {
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 28
                                radius: width / 2
                                color: Theme.color("success")

                                DSStyle.Label {
                                    anchors.centerIn: parent
                                    text: "OK"
                                    color: Theme.color("accentText")
                                    font.pixelSize: Theme.fontSize("tiny")
                                    font.weight: Theme.fontWeight("semibold")
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                DSStyle.Label {
                                    Layout.fillWidth: true
                                    text: "Folder is available on the local network"
                                    color: Theme.color("textPrimary")
                                    font.weight: Theme.fontWeight("semibold")
                                }
                                DSStyle.Label {
                                    Layout.fillWidth: true
                                    text: root.accessTitle() + " - " + root.permissionTitle()
                                    color: Theme.color("textSecondary")
                                    font.pixelSize: Theme.fontSize("small")
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        DSStyle.TextField {
                            Layout.fillWidth: true
                            readOnly: true
                            selectByMouse: true
                            text: String(root.successAddress || "-")
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    visible: !root.successState
                    radius: Theme.radiusControlLarge
                    color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.04)
                    border.width: Theme.borderWidthThin
                    border.color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.13) : Qt.rgba(0, 0, 0, 0.09)
                    implicitHeight: shareSwitchRow.implicitHeight + 22

                    RowLayout {
                        id: shareSwitchRow
                        anchors.fill: parent
                        anchors.margins: 11
                        spacing: 12

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            DSStyle.Label {
                                Layout.fillWidth: true
                                text: "Share this folder"
                                color: Theme.color("textPrimary")
                                font.weight: Theme.fontWeight("semibold")
                            }
                            DSStyle.Label {
                                Layout.fillWidth: true
                                text: root.sharingEnabled
                                      ? "Devices on the same network can find it using the settings below."
                                      : "Keep this folder private until sharing is turned on."
                                color: Theme.color("textSecondary")
                                font.pixelSize: Theme.fontSize("small")
                                wrapMode: Text.WordWrap
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
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        DSStyle.Label {
                            Layout.fillWidth: true
                            text: "Share name"
                            color: Theme.color("textPrimary")
                            font.weight: Theme.fontWeight("semibold")
                        }
                        DSStyle.TextField {
                            Layout.fillWidth: true
                            text: root.shareName
                            onTextChanged: root.shareName = text
                            placeholderText: hostRoot.basename(root.targetPath)
                        }
                        DSStyle.Label {
                            Layout.fillWidth: true
                            text: "This is the name other devices will see."
                            color: Theme.color("textSecondary")
                            font.pixelSize: Theme.fontSize("small")
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        DSStyle.Label {
                            Layout.fillWidth: true
                            text: "Who can access"
                            color: Theme.color("textPrimary")
                            font.weight: Theme.fontWeight("semibold")
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            radius: Theme.radiusControl
                            color: root.accessMode === "owner"
                                   ? Theme.color("accentSoft")
                                   : (accessOwnerArea.containsMouse
                                      ? Theme.color("controlBgHover")
                                      : Theme.color("controlBg"))
                            border.width: Theme.borderWidthThin
                            border.color: root.accessMode === "owner" ? Theme.color("accent") : Theme.color("panelBorder")
                            implicitHeight: accessOwnerRow.implicitHeight + 18

                            RowLayout {
                                id: accessOwnerRow
                                anchors.fill: parent
                                anchors.margins: 9
                                spacing: 10

                                Rectangle {
                                    Layout.preferredWidth: 18
                                    Layout.preferredHeight: 18
                                    radius: width / 2
                                    color: root.accessMode === "owner" ? Theme.color("accent") : "transparent"
                                    border.width: Theme.borderWidthThin
                                    border.color: root.accessMode === "owner" ? Theme.color("accent") : Theme.color("panelBorderStrong")
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1
                                    DSStyle.Label {
                                        Layout.fillWidth: true
                                        text: "Only me"
                                        color: Theme.color("textPrimary")
                                    }
                                    DSStyle.Label {
                                        Layout.fillWidth: true
                                        text: "Sharing stays limited to your account."
                                        color: Theme.color("textSecondary")
                                        font.pixelSize: Theme.fontSize("small")
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }

                            MouseArea {
                                id: accessOwnerArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.accessMode = "owner"
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            radius: Theme.radiusControl
                            color: root.accessMode === "anyone"
                                   ? Theme.color("accentSoft")
                                   : (accessAnyoneArea.containsMouse
                                      ? Theme.color("controlBgHover")
                                      : Theme.color("controlBg"))
                            border.width: Theme.borderWidthThin
                            border.color: root.accessMode === "anyone" ? Theme.color("accent") : Theme.color("panelBorder")
                            implicitHeight: accessAnyoneColumn.implicitHeight + 18

                            ColumnLayout {
                                id: accessAnyoneColumn
                                anchors.fill: parent
                                anchors.margins: 9
                                spacing: 6

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    Rectangle {
                                        Layout.preferredWidth: 18
                                        Layout.preferredHeight: 18
                                        radius: width / 2
                                        color: root.accessMode === "anyone" ? Theme.color("accent") : "transparent"
                                        border.width: Theme.borderWidthThin
                                        border.color: root.accessMode === "anyone" ? Theme.color("accent") : Theme.color("panelBorderStrong")
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 1
                                        DSStyle.Label {
                                            Layout.fillWidth: true
                                            text: "Anyone on this network"
                                            color: Theme.color("textPrimary")
                                        }
                                        DSStyle.Label {
                                            Layout.fillWidth: true
                                            text: "Simple for trusted home or studio networks."
                                            color: Theme.color("textSecondary")
                                            font.pixelSize: Theme.fontSize("small")
                                            wrapMode: Text.WordWrap
                                        }
                                    }
                                }

                                DSStyle.Label {
                                    Layout.fillWidth: true
                                    visible: root.accessMode === "anyone"
                                    Layout.leftMargin: 28
                                    text: "Use only on networks you trust."
                                    color: Theme.color("warning")
                                    font.pixelSize: Theme.fontSize("small")
                                    wrapMode: Text.WordWrap
                                }
                            }

                            MouseArea {
                                id: accessAnyoneArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.accessMode = "anyone"
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            radius: Theme.radiusControl
                            color: root.accessMode === "users"
                                   ? Theme.color("accentSoft")
                                   : (accessUsersArea.containsMouse
                                      ? Theme.color("controlBgHover")
                                      : Theme.color("controlBg"))
                            border.width: Theme.borderWidthThin
                            border.color: root.accessMode === "users" ? Theme.color("accent") : Theme.color("panelBorder")
                            implicitHeight: accessUsersColumn.implicitHeight + 18

                            ColumnLayout {
                                id: accessUsersColumn
                                anchors.fill: parent
                                anchors.margins: 9
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    Rectangle {
                                        Layout.preferredWidth: 18
                                        Layout.preferredHeight: 18
                                        radius: width / 2
                                        color: root.accessMode === "users" ? Theme.color("accent") : "transparent"
                                        border.width: Theme.borderWidthThin
                                        border.color: root.accessMode === "users" ? Theme.color("accent") : Theme.color("panelBorderStrong")
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 1
                                        DSStyle.Label {
                                            Layout.fillWidth: true
                                            text: "Specific users"
                                            color: Theme.color("textPrimary")
                                        }
                                        DSStyle.Label {
                                            Layout.fillWidth: true
                                            text: "Limit access to selected user names."
                                            color: Theme.color("textSecondary")
                                            font.pixelSize: Theme.fontSize("small")
                                            wrapMode: Text.WordWrap
                                        }
                                    }
                                }

                                DSStyle.TextField {
                                    Layout.fillWidth: true
                                    Layout.leftMargin: 28
                                    visible: root.accessMode === "users"
                                    placeholderText: "User names, separated by commas"
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

                            MouseArea {
                                id: accessUsersArea
                                anchors.fill: parent
                                visible: root.accessMode !== "users"
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.accessMode = "users"
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        DSStyle.Label {
                            Layout.fillWidth: true
                            text: "Permission"
                            color: Theme.color("textPrimary")
                            font.weight: Theme.fontWeight("semibold")
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            radius: Theme.radiusControl
                            color: Theme.color("controlBg")
                            border.width: Theme.borderWidthThin
                            border.color: Theme.color("panelBorder")
                            implicitHeight: 38

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 4
                                spacing: 4

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    radius: Theme.radiusMd
                                    color: root.permissionMode === "read" ? Theme.color("accent") : "transparent"

                                    DSStyle.Label {
                                        anchors.centerIn: parent
                                        text: "Read only"
                                        color: root.permissionMode === "read" ? Theme.color("accentText") : Theme.color("textPrimary")
                                        font.pixelSize: Theme.fontSize("small")
                                        font.weight: Theme.fontWeight("semibold")
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: root.permissionMode = "read"
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    radius: Theme.radiusMd
                                    color: root.permissionMode === "write" ? Theme.color("accent") : "transparent"

                                    DSStyle.Label {
                                        anchors.centerIn: parent
                                        text: "Can make changes"
                                        color: root.permissionMode === "write" ? Theme.color("accentText") : Theme.color("textPrimary")
                                        font.pixelSize: Theme.fontSize("small")
                                        font.weight: Theme.fontWeight("semibold")
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: root.permissionMode = "write"
                                    }
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            radius: Theme.radiusControl
                            color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.045) : Qt.rgba(0, 0, 0, 0.03)
                            border.width: Theme.borderWidthThin
                            border.color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(0, 0, 0, 0.08)
                            implicitHeight: guestRow.implicitHeight + 18

                            RowLayout {
                                id: guestRow
                                anchors.fill: parent
                                anchors.margins: 9
                                spacing: 10

                                DSStyle.CheckBox {
                                    checked: root.allowGuest
                                    onToggled: root.allowGuest = checked
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    DSStyle.Label {
                                        Layout.fillWidth: true
                                        text: "Allow guest access"
                                        color: Theme.color("textPrimary")
                                    }
                                    DSStyle.Label {
                                        Layout.fillWidth: true
                                        text: "Guests can connect without a user account."
                                        color: Theme.color("textSecondary")
                                        font.pixelSize: Theme.fontSize("small")
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    visible: String(root.errorText || "").length > 0
                    radius: Theme.radiusControl
                    color: Qt.rgba(0.82, 0.29, 0.29, Theme.darkMode ? 0.13 : 0.09)
                    border.width: Theme.borderWidthThin
                    border.color: Qt.rgba(0.82, 0.29, 0.29, 0.34)
                    implicitHeight: errorLabel.implicitHeight + 18

                    DSStyle.Label {
                        id: errorLabel
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        text: root.errorText
                        color: Theme.color("error")
                        wrapMode: Text.WordWrap
                        font.pixelSize: Theme.fontSize("small")
                    }
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
                visible: successState
                text: "Done"
                onClicked: root.close()
            }
            DSStyle.Button {
                visible: successState
                text: "Copy Address"
                defaultAction: true
                onClicked: root.hostRoot.copyFolderShareAddress(root.targetPath)
            }
            DSStyle.Button {
                visible: !successState
                defaultAction: true
                text: root.sharingEnabled ? "Start Sharing" : "Save"
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
