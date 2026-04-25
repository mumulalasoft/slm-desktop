import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.impl 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle

Item {
    id: root

    property var networkManager: NetworkManager
    property var popupHost: null
    property bool ipAddressVisible: false
    property bool ipSectionVisible: false
    property bool popupHint: false
    property double lastMenuCloseMs: 0
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
                opacity: (root.networkManager && root.networkManager.online) ? 1.0 : 0.55
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

    IndicatorMenu {
        id: networkMenu
        anchorItem: indicatorButton
        popupGap: root.popupGap
        popupWidth: Theme.metric("popupWidthS")
        padding: 8
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
            contentItem: IndicatorSectionLabel {
                text: root.networkManager && root.networkManager.online
                      ? "Connected: " + root.networkManager.statusText : "Disconnected"
                emphasized: true
            }
        }

        MenuItem {
            enabled: false
            text: {
                if (!root.networkManager || !root.networkManager.online) return "Type: Unavailable"
                var ct = root.networkManager.connectionType || ""
                if (ct === "wifi")     return "Type: Wi-Fi"
                if (ct === "ethernet") return "Type: Ethernet"
                if (ct === "unknown")  return "Type: Other"
                return "Type: Connected"
            }
        }

        MenuItem {
            text: "Interface: " + (
                      root.networkManager && root.networkManager.interfaceName.length > 0
                      ? root.networkManager.interfaceName
                      : "n/a"
                  )
            // color: Theme.color("textPrimary")
            font.family: Theme.fontFamilyUi
            font.pixelSize: Theme.fontSize("body")
            enabled: false
        }
        MenuItem {
            visible: root.ipSectionVisible
            height: visible ? implicitHeight : 0
            contentItem: IndicatorSectionRow {
                text: "IP Address Visible"
                rowSpacing: root.rowGap
                Switch {
                    checked: root.ipAddressVisible
                    onToggled: root.ipAddressVisible = checked
                }
            }
        }
        MenuItem {
            visible: root.ipSectionVisible
            height: visible ? implicitHeight : 0
            text: "IPv4: " + (
                      root.ipAddressVisible
                      ? (
                            root.networkManager && root.networkManager.ipv4Address.length > 0
                            ? root.networkManager.ipv4Address
                            : "n/a"
                        )
                      : "---.---.---.---"
                  )
            // color: Theme.color("textPrimary")
            font.family: Theme.fontFamilyUi
            font.pixelSize: Theme.fontSize("body")
            enabled: false
        }

        MenuItem {
            visible: root.networkManager && root.networkManager.connectionType === "wifi"
            enabled: false
            text: "Signal: " + (
                      root.networkManager && root.networkManager.signalStrength > 0
                      ? root.networkManager.signalStrength + "%"
                      : "Unknown"
                  )
            // color: Theme.color("textPrimary")
            font.family: Theme.fontFamilyUi
            font.pixelSize: Theme.fontSize("body")
        }

        MenuSeparator {}



        Instantiator {
            id: availableNetworksInstantiator
            model: root.networkManager ? root.networkManager.availableNetworks : null
            delegate: MenuItem {
                enabled: true
                contentItem: RowLayout {
                    spacing: Theme.metric("spacingXxl")

                    RadioButton {
                        checked: !!isActive
                        enabled: true
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

                    IconImage {
                        id: signalIcon
                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 20
                        Layout.alignment: Qt.AlignVCenter
                        property var candidates: root.signalIconFallbacks(signalStrength, !!isSecure)
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
                    }
                }
            }
            onObjectAdded: function(index, object) {
                networkMenu.insertItem(7 + index, object)
            }
            onObjectRemoved: function(index, object) {
                networkMenu.removeItem(object)
            }
        }



        MenuSeparator {}




        MenuItem {
            text: "Network Settings"
            // color: Theme.color("textPrimary")
            font.family: Theme.fontFamilyUi
            font.pixelSize: Theme.fontSize("body")
            onTriggered: {
                // hook: open system settings here
            }
        }
    }
}
