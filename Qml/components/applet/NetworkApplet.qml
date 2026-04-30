import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.impl 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle
import "../overlay" as Overlay

Item {
    id: root

    property var networkManager: NetworkManager
    property var popupHost: null
    property bool ipAddressVisible: false
    property bool ipSectionVisible: false
    property bool popupHint: false
    property double lastMenuCloseMs: 0
    property string pendingSsid: ""
    property int pendingSignalStrength: 0
    property bool pendingSecure: false
    readonly property int iconSize: 22
    readonly property int popupGap: Theme.metric("spacingSm")
    readonly property int rowGap: Theme.metric("spacingMd")
    readonly property bool popupOpen: popupHint || networkMenu.opened

    Timer {
        id: popupHintTimer
        interval: 320
        repeat: false
        onTriggered: root.popupHint = false
    }

    function syncIpSection() {
        if (typeof DesktopSettings === "undefined" || !DesktopSettings) return
        root.ipSectionVisible = DesktopSettings.settingValue("shellTheme.networkShowIp", false) === true
    }

    Component.onCompleted: syncIpSection()

    Connections {
        target: (typeof DesktopSettings !== "undefined") ? DesktopSettings : null
        function onAvailableChanged() { root.syncIpSection() }
        function onSettingChanged(path) {
            if (path === "shellTheme.networkShowIp") root.syncIpSection()
        }
    }
    // property var string networkName: networkManager.statusText
    property bool showText: true

    implicitWidth: indicatorButton.implicitWidth
    implicitHeight: indicatorButton.implicitHeight

    function microAnimationAllowed() {
        if (!Theme.animationsEnabled) {
            return false
        }
        if (typeof MotionController === "undefined" || !MotionController || !MotionController.allowMotionPriority) {
            return true
        }
        return MotionController.allowMotionPriority(MotionController.LowPriority)
    }

    function openMenuSafely() {
        if ((Date.now() - lastMenuCloseMs) < 180) {
            return
        }
        root.popupHint = true
        popupHintTimer.restart()
        Qt.callLater(function() { networkMenu.togglePopup() })
    }

    function isConcreteSsid(name) {
        var text = (name || "").toString().trim()
        return text.length > 0 &&
               text !== "N/A" &&
               text !== "Wi-Fi" &&
               text !== "Ethernet" &&
               text !== "Connected" &&
               text !== "Offline"
    }

    function iconSourceByName(name) {
        var iconName = (name || "").toString().trim()
        if (iconName.length === 0) {
            iconName = "network-offline-symbolic"
        }
        return "image://themeicon/" + iconName + "?v=" +
                ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                 ? ThemeIconController.revision : 0)
    }

    function wifiLevelName(strength) {
        var value = Number(strength)
        if (value >= 75) {
            return "excellent"
        }
        if (value >= 50) {
            return "good"
        }
        if (value >= 25) {
            return "ok"
        }
        if (value > 0) {
            return "weak"
        }
        return "none"
    }

    function panelIconCandidates() {
        var nm = root.networkManager
        if (!nm) return ["network-offline-symbolic"]
        var connected = !!nm.online
        var connType = nm.connectionType || ""

        if (!connected || connType === "none") {
            return [
                "network-offline-symbolic",
                "network-wireless-offline-symbolic",
                "network-wireless-signal-none-symbolic"
            ]
        }

        if (connType === "ethernet") {
            return [
                "network-wired-symbolic",
                "network-wired",
                "network-transmit-receive-symbolic"
            ]
        }

        if (connType === "wifi") {
            var strength = nm.signalStrength || 0
            var secure = !!nm.activeConnectionSecure
            if (strength === 0) {
                // Connected but signal not yet read — don't show "none" (looks like offline)
                var base = secure ? "network-wireless-connected-secure-symbolic"
                                  : "network-wireless-connected-symbolic"
                return [base, "network-wireless-connected-symbolic", "network-wireless-symbolic"]
            }
            var level = root.wifiLevelName(strength)
            if (secure) {
                return [
                    "network-wireless-signal-" + level + "-secure-symbolic",
                    "network-wireless-signal-" + level + "-symbolic",
                    "network-wireless-signal-good-symbolic",
                    "network-wireless-symbolic"
                ]
            }
            return [
                "network-wireless-signal-" + level + "-symbolic",
                "network-wireless-signal-good-symbolic",
                "network-wireless-symbolic"
            ]
        }

        // VPN, tunnel, or other unrecognised active connection
        return [
            "network-vpn-symbolic",
            "network-wired-symbolic",
            "network-transmit-receive-symbolic"
        ]
    }

    function signalIconName(strength, secure) {
        var value = Number(strength)
        var level = "none"
        if (value >= 75) {
            level = "excellent"
        } else if (value >= 50) {
            level = "good"
        } else if (value >= 25) {
            level = "ok"
        } else if (value > 0) {
            level = "weak"
        }

        if (secure) {
            return "network-wireless-signal-" + level + "-secure-symbolic"
        }
        return "network-wireless-signal-" + level + "-symbolic"
    }

    function signalIconFallbacks(strength, secure) {
        var primary = signalIconName(strength, secure)
        var list = [primary]
        if (secure) {
            list.push(signalIconName(strength, false))
        }
        list.push("network-wireless-signal-good-symbolic")
        list.push("network-wireless-signal-none-symbolic")
        return list
    }

    function iconCandidate(candidates, index) {
        if (!candidates || index < 0 || index >= candidates.length) {
            return ""
        }
        return candidates[index]
    }

    function friendlyConnectError(result) {
        var message = String(result && result.message ? result.message : "")
        if (message.length > 0) {
            return message
        }
        var error = String(result && result.error ? result.error : "")
        if (error === "nmcli-unavailable") {
            return "Network connector is unavailable."
        }
        if (error === "connect-timeout") {
            return "Connection timed out."
        }
        if (error === "invalid-ssid") {
            return "Network name is invalid."
        }
        return "Could not join the selected network."
    }

    function connectNetwork(ssid, password, remember) {
        if (!root.networkManager || !root.networkManager.connectToNetwork) {
            return { "ok": false, "error": "network-manager-unavailable" }
        }
        return root.networkManager.connectToNetwork(String(ssid || ""),
                                                    String(password || ""),
                                                    !!remember)
    }

    function requestJoinNetwork(ssid, secure, strength) {
        var name = String(ssid || "").trim()
        if (name.length <= 0 || name === "<Hidden Network>") {
            return
        }
        root.pendingSsid = name
        root.pendingSignalStrength = Number(strength || 0)
        root.pendingSecure = !!secure
        networkMenu.close()

        if (!secure) {
            var openResult = root.connectNetwork(name, "", false)
            if (!openResult || !openResult.ok) {
                wifiPasswordDialog.errorMessage = root.friendlyConnectError(openResult)
                wifiPasswordDialog.ssid = name
                wifiPasswordDialog.isSecure = false
                wifiPasswordDialog.securityType = ""
                wifiPasswordDialog.signalStrength = root.pendingSignalStrength
                wifiPasswordDialog.showDialog()
            }
            return
        }

        wifiPasswordDialog.ssid = name
        wifiPasswordDialog.isSecure = true
        wifiPasswordDialog.securityType = "WPA/WPA2"
        wifiPasswordDialog.signalStrength = root.pendingSignalStrength
        wifiPasswordDialog.errorMessage = ""
        wifiPasswordDialog.busy = false
        wifiPasswordDialog.rememberNetwork = true
        wifiPasswordDialog.showDialog()
    }

    function openNetworkSettings() {
        networkMenu.close()

        var opened = false
        if (typeof AppExecutionGate !== "undefined" && AppExecutionGate && AppExecutionGate.launchCommand) {
            if (typeof AppBinaryDir !== "undefined" && String(AppBinaryDir || "").length > 0) {
                opened = AppExecutionGate.launchCommand(String(AppBinaryDir) + "/slm-settings --module network",
                                                        "",
                                                        "network-applet")
            }
            if (!opened) {
                opened = AppExecutionGate.launchCommand("slm-settings --module network",
                                                        "",
                                                        "network-applet")
            }
        }

        if (!opened && typeof AppCommandRouter !== "undefined" && AppCommandRouter && AppCommandRouter.route) {
            AppCommandRouter.route("app.desktopid",
                                   { desktopId: "slm-settings.desktop" },
                                   "network-applet")
        } else if (!opened && typeof AppExecutionGate !== "undefined" && AppExecutionGate
                   && AppExecutionGate.launchDesktopId) {
            AppExecutionGate.launchDesktopId("slm-settings.desktop", "network-applet")
        }
    }

    ToolButton {
        id: indicatorButton
        anchors.fill: parent
        padding: 0

        contentItem: Row {
            spacing: Theme.metric("spacingXs")
            anchors.verticalCenter: parent.verticalCenter

            IconImage {
                id: panelIcon
                width: root.iconSize
                height: root.iconSize
                property var candidates: root.panelIconCandidates()
                property int candidateIndex: 0
                source: root.iconSourceByName(root.iconCandidate(candidates, candidateIndex))
                fillMode: Image.PreserveAspectFit
                opacity: (root.networkManager && root.networkManager.online) ? Theme.opacitySurfaceStrong : Theme.opacityMuted
                color: Theme.color("textOnGlass")
                onStatusChanged: {
                    if (status === Image.Error && candidateIndex + 1 < candidates.length) {
                        candidateIndex += 1
                    }
                }
                onCandidatesChanged: candidateIndex = 0
            }


        }

        background: Rectangle {
            radius: Theme.radiusControl
            color: indicatorButton.hovered ? Theme.color("accentSoft") : "transparent"
            border.width: Theme.borderWidthThin
            border.color: indicatorButton.hovered ? Theme.color("panelBorder") : "transparent"
            Behavior on color {
                enabled: root.microAnimationAllowed()
                ColorAnimation { duration: Theme.durationSm; easing.type: Theme.easingDefault }
            }
            Behavior on border.color {
                enabled: root.microAnimationAllowed()
                ColorAnimation { duration: Theme.durationSm; easing.type: Theme.easingDefault }
            }
        }

        onClicked: {
            if ((Date.now() - root.lastMenuCloseMs) < 220) {
                return
            }
            if (networkMenu.opened || networkMenu.visible) {
                root.lastMenuCloseMs = Date.now()
                networkMenu.close()
                return
            }
            root.openMenuSafely()
        }
    }

    Overlay.WifiPasswordDialog {
        id: wifiPasswordDialog

        onAccepted: function(password, remember) {
            wifiPasswordDialog.busy = true
            wifiPasswordDialog.errorMessage = ""
            var result = root.connectNetwork(root.pendingSsid, password, remember)
            wifiPasswordDialog.busy = false
            if (result && result.ok) {
                wifiPasswordDialog.dismissDialog()
            } else {
                wifiPasswordDialog.errorMessage = root.friendlyConnectError(result)
            }
        }

        onRejected: {
            root.pendingSsid = ""
            root.pendingSignalStrength = 0
            root.pendingSecure = false
        }
    }

    IndicatorMenu {
        id: networkMenu
        anchorItem: indicatorButton
        popupGap: root.popupGap
        popupWidth: Theme.metric("popupWidthL")
        padding: Theme.spacingSm
        onAboutToShow: {
            root.popupHint = false
            if (root.networkManager) {
                root.networkManager.refresh()
                root.networkManager.refreshAvailableNetworks()
            }
        }
        onAboutToHide: {
            root.popupHint = false
            root.lastMenuCloseMs = Date.now()
        }

        MenuItem {
            enabled: false
            contentItem: ColumnLayout {
                spacing: Theme.spacingMd

                Rectangle {
                    Layout.fillWidth: true
                    radius: Theme.radiusControl
                    color: Theme.color("fileManagerSearchBg")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("panelBorder")
                    implicitHeight: statusCard.implicitHeight + Theme.spacingLg

                    RowLayout {
                        id: statusCard
                        anchors.fill: parent
                        anchors.margins: Theme.spacingMd
                        spacing: Theme.spacingMd

                        Rectangle {
                            Layout.preferredWidth: 42
                            Layout.preferredHeight: 42
                            radius: Theme.radiusControl
                            color: Theme.color("accentSoft")

                            IconImage {
                                anchors.centerIn: parent
                                width: 24
                                height: 24
                                property var candidates: root.panelIconCandidates()
                                property int candidateIndex: 0
                                source: root.iconSourceByName(root.iconCandidate(candidates, candidateIndex))
                                fillMode: Image.PreserveAspectFit
                                color: Theme.color("textPrimary")
                                onStatusChanged: {
                                    if (status === Image.Error && candidateIndex + 1 < candidates.length) {
                                        candidateIndex += 1
                                    }
                                }
                                onCandidatesChanged: candidateIndex = 0
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingXs

                            Text {
                                Layout.fillWidth: true
                                text: root.networkManager && root.networkManager.online
                                      ? root.networkManager.statusText
                                      : "Not Connected"
                                color: Theme.color("textPrimary")
                                font.family: Theme.fontFamilyUi
                                font.pixelSize: Theme.fontSize("title")
                                font.weight: Theme.fontWeight("semibold")
                                elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true
                                text: {
                                    if (!root.networkManager || !root.networkManager.online) {
                                        return "Network is unavailable"
                                    }
                                    var ct = String(root.networkManager.connectionType || "")
                                    if (ct === "wifi") {
                                        return "Wi-Fi" + (root.networkManager.signalStrength > 0
                                                         ? " - " + root.networkManager.signalStrength + "% signal"
                                                         : "")
                                    }
                                    if (ct === "ethernet") {
                                        return "Ethernet"
                                    }
                                    if (ct === "unknown") {
                                        return "Connected"
                                    }
                                    return "Connected via " + ct
                                }
                                color: Theme.color("textSecondary")
                                font.family: Theme.fontFamilyUi
                                font.pixelSize: Theme.fontSize("small")
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: Theme.spacingLg
                    rowSpacing: Theme.spacingSm

                    Text {
                        text: "Interface"
                        color: Theme.color("textSecondary")
                        font.family: Theme.fontFamilyUi
                        font.pixelSize: Theme.fontSize("small")
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.networkManager && root.networkManager.interfaceName.length > 0
                              ? root.networkManager.interfaceName
                              : "n/a"
                        color: Theme.color("textPrimary")
                        font.family: Theme.fontFamilyUi
                        font.pixelSize: Theme.fontSize("small")
                        horizontalAlignment: Text.AlignRight
                        elide: Text.ElideMiddle
                    }

                    Text {
                        visible: root.ipSectionVisible
                        text: "IPv4"
                        color: Theme.color("textSecondary")
                        font.family: Theme.fontFamilyUi
                        font.pixelSize: Theme.fontSize("small")
                    }

                    Text {
                        visible: root.ipSectionVisible
                        Layout.fillWidth: true
                        text: root.ipAddressVisible
                              ? (root.networkManager && root.networkManager.ipv4Address.length > 0
                                 ? root.networkManager.ipv4Address
                                 : "n/a")
                              : "---.---.---.---"
                        color: Theme.color("textPrimary")
                        font.family: Theme.fontFamilyUi
                        font.pixelSize: Theme.fontSize("small")
                        horizontalAlignment: Text.AlignRight
                        elide: Text.ElideMiddle
                    }
                }
            }
        }

        MenuItem {
            visible: root.ipSectionVisible
            height: visible ? implicitHeight : 0
            contentItem: IndicatorSectionRow {
                text: "Show IP Address"
                rowSpacing: root.rowGap
                Switch {
                    checked: root.ipAddressVisible
                    onToggled: root.ipAddressVisible = checked
                }
            }
        }

        MenuSeparator {}

        MenuItem {
            enabled: false
            contentItem: RowLayout {
                spacing: Theme.spacingSm

                Text {
                    Layout.fillWidth: true
                    text: "Available Networks"
                    color: Theme.color("textSecondary")
                    font.family: Theme.fontFamilyUi
                    font.pixelSize: Theme.fontSize("small")
                    font.weight: Theme.fontWeight("semibold")
                }

                ToolButton {
                    icon.source: root.iconSourceByName("view-refresh-symbolic")
                    onClicked: if (root.networkManager) root.networkManager.refreshAvailableNetworks()
                }
            }
        }

        MenuItem {
            enabled: false
            visible: !root.networkManager || !root.networkManager.hasAvailableNetworks
            text: "No Wi-Fi networks found"
            font.family: Theme.fontFamilyUi
            font.pixelSize: Theme.fontSize("body")
        }

        MenuItem {
            enabled: true
            visible: root.networkManager && root.networkManager.hasAvailableNetworks
            contentItem: ColumnLayout {
                spacing: Theme.spacingXs

                Repeater {
                    model: root.networkManager ? root.networkManager.availableNetworks : null

                    delegate: Rectangle {
                        required property string ssid
                        required property int signalStrength
                        required property bool isSecure
                        required property bool isActive

                        Layout.fillWidth: true
                        radius: Theme.radiusControl
                        color: isActive ? Theme.color("accentSoft") : "transparent"
                        implicitHeight: networkRow.implicitHeight + Theme.spacingSm

                        MouseArea {
                            anchors.fill: parent
                            enabled: !isActive
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.requestJoinNetwork(ssid, isSecure, signalStrength)
                        }

                        RowLayout {
                            id: networkRow
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: Theme.spacingSm
                            anchors.rightMargin: Theme.spacingSm
                            spacing: Theme.spacingSm

                            RadioButton {
                                checked: isActive
                                enabled: false
                            }

                            Text {
                                Layout.fillWidth: true
                                text: ssid
                                color: Theme.color("textPrimary")
                                font.family: Theme.fontFamilyUi
                                font.pixelSize: Theme.fontSize("body")
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                            }

                            Text {
                                visible: isSecure
                                text: "Secured"
                                color: Theme.color("textSecondary")
                                font.family: Theme.fontFamilyUi
                                font.pixelSize: Theme.fontSize("xs")
                            }

                            IconImage {
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 20
                                Layout.alignment: Qt.AlignVCenter
                                property var candidates: root.signalIconFallbacks(signalStrength, isSecure)
                                property int candidateIndex: 0
                                source: "image://themeicon/" + root.iconCandidate(candidates, candidateIndex) + "?v=" +
                                        ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                                         ? ThemeIconController.revision : 0)
                                fillMode: Image.PreserveAspectFit
                                color: Theme.color("textPrimary")
                                onStatusChanged: {
                                    if (status === Image.Error && candidateIndex + 1 < candidates.length) {
                                        candidateIndex += 1
                                    }
                                }
                                onCandidatesChanged: candidateIndex = 0
                            }
                        }
                    }
                }
            }
        }

        MenuSeparator {}

        MenuItem {
            text: "Network Settings"
            font.family: Theme.fontFamilyUi
            font.pixelSize: Theme.fontSize("body")
            onTriggered: root.openNetworkSettings()
        }
    }
}
