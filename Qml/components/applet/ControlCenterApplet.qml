import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.impl 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle

Item {
    id: root

    property var popupHost: null
    property var networkManager: NetworkManager
    property var bluetoothManager: BluetoothManager
    property var soundManager: SoundManager
    property var notificationManager: NotificationManager
    property bool popupHint: false
    property double lastMenuCloseMs: 0
    property real brightnessValue: 72
    readonly property int iconSize: 22
    readonly property int popupGap: Theme.metric("spacingXs")
    readonly property int sectionGap: Theme.metric("spacingSm")
    readonly property int itemGap: Theme.metric("spacingXs")
    readonly property int popupWidth: 300
    readonly property bool popupOpen: popupHint || menu.opened

    Timer {
        id: popupHintTimer
        interval: 320
        repeat: false
        onTriggered: root.popupHint = false
    }

    implicitWidth: indicatorButton.implicitWidth
    implicitHeight: indicatorButton.implicitHeight

    function iconSourceByName(name) {
        var iconName = String(name || "").trim()
        if (iconName.length === 0) {
            iconName = "preferences-system-symbolic"
        }
        return "image://themeicon/" + iconName + "?v=" +
                ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                 ? ThemeIconController.revision : 0)
    }

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
        Qt.callLater(function() { menu.togglePopup() })
    }

    function openSettings(moduleId) {
        menu.close()
        var hint = String(moduleId || "")
        if (typeof AppCommandRouter !== "undefined" && AppCommandRouter && AppCommandRouter.route) {
            AppCommandRouter.route("app.desktopid",
                                   { desktopId: "slm-settings.desktop", moduleHint: hint },
                                   "control-center")
        } else if (typeof AppExecutionGate !== "undefined" && AppExecutionGate
                   && AppExecutionGate.launchDesktopId) {
            AppExecutionGate.launchDesktopId("slm-settings.desktop", "control-center")
        }
    }

    ToolButton {
        id: indicatorButton
        anchors.fill: parent
        padding: 0
        onClicked: {
            if ((Date.now() - root.lastMenuCloseMs) < 220) {
                return
            }
            if (menu.opened || menu.visible) {
                root.lastMenuCloseMs = Date.now()
                menu.close()
                return
            }
            root.openMenuSafely()
        }

        contentItem: Item {
            implicitWidth: root.iconSize
            implicitHeight: root.iconSize

            IconImage {
                anchors.centerIn: parent
                width: root.iconSize
                height: root.iconSize
                fillMode: Image.PreserveAspectFit
                source: root.iconSourceByName("tweaks-app-symbolic")
                color: Theme.color("textOnGlass")
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
    }

    IndicatorMenu {
        id: menu
        appletId: "topbar.controlcenter"
        anchorItem: indicatorButton
        popupHost: root.popupHost
        popupGap: root.popupGap
        popupWidth: root.popupWidth
        padding: Theme.metric("spacingXs")
        onAboutToShow: {
            root.popupHint = false
            if (root.networkManager && root.networkManager.refresh) root.networkManager.refresh()
            if (root.bluetoothManager && root.bluetoothManager.refresh) root.bluetoothManager.refresh()
            if (root.soundManager && root.soundManager.refresh) root.soundManager.refresh()
        }
        onAboutToHide: {
            root.popupHint = false
            root.lastMenuCloseMs = Date.now()
        }

        MenuItem {
            enabled: false
            contentItem: ColumnLayout {
                spacing: root.sectionGap

                RowLayout {
                    Layout.fillWidth: true
                    spacing: root.itemGap

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Theme.metric("controlHeightLarge") * 3 + Theme.metric("spacingXs")
                        radius: Theme.radiusCard
                        color: Theme.color("surfaceRaised")
                        border.width: Theme.borderWidthThin
                        border.color: Theme.color("panelBorder")

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.metric("spacingSm")
                            spacing: Theme.metric("spacingXs")

                            Button {
                                Layout.fillWidth: true
                                text: "Wi-Fi\n" + (root.networkManager && root.networkManager.online ? "Connected" : "Not Connected")
                                icon.source: root.iconSourceByName("network-wireless-signal-good-symbolic")
                                horizontalPadding: Theme.metric("spacingSm")
                                onClicked: root.openSettings("network")
                            }

                            Button {
                                Layout.fillWidth: true
                                text: "Bluetooth\n" + (root.bluetoothManager && root.bluetoothManager.powered ? "On" : "Off")
                                icon.source: root.iconSourceByName("bluetooth-active-symbolic")
                                horizontalPadding: Theme.metric("spacingSm")
                                onClicked: root.openSettings("bluetooth")
                            }

                            Button {
                                Layout.fillWidth: true
                                text: "Slare\nOff"
                                icon.source: root.iconSourceByName("folder-remote-symbolic")
                                horizontalPadding: Theme.metric("spacingSm")
                                onClicked: root.openSettings("sharing")
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: root.itemGap

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Theme.metric("controlHeightLarge") * 2
                            radius: Theme.radiusCard
                            color: Theme.color("surfaceRaised")
                            border.width: Theme.borderWidthThin
                            border.color: Theme.color("panelBorder")

                            Button {
                                anchors.fill: parent
                                anchors.margins: Theme.metric("spacingSm")
                                text: "Do Not\nDisturb"
                                icon.source: root.iconSourceByName("weather-clear-night-symbolic")
                                horizontalPadding: Theme.metric("spacingSm")
                                onClicked: {
                                    if (root.notificationManager && root.notificationManager.setDoNotDisturb) {
                                        root.notificationManager.setDoNotDisturb(!root.notificationManager.doNotDisturb)
                                    }
                                    root.openSettings("notifications")
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Theme.metric("controlHeightLarge") * 2
                            radius: Theme.radiusCard
                            color: Theme.color("surfaceRaised")
                            border.width: Theme.borderWidthThin
                            border.color: Theme.color("panelBorder")

                            Button {
                                anchors.fill: parent
                                anchors.margins: Theme.metric("spacingSm")
                                text: "Screen\nMirroring"
                                icon.source: root.iconSourceByName("video-display-symbolic")
                                horizontalPadding: Theme.metric("spacingSm")
                                onClicked: root.openSettings("display")
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    radius: Theme.radiusCard
                    color: Theme.color("surfaceRaised")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("panelBorder")
                    implicitHeight: Theme.metric("controlHeightLarge") + Theme.metric("spacingLg")

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.metric("spacingSm")
                        spacing: Theme.metric("spacingXs")

                        Label {
                            text: "Display"
                            color: Theme.color("textPrimary")
                            font.pixelSize: Theme.fontSize("body")
                            font.family: Theme.fontFamilyUi
                        }

                        Slider {
                            Layout.fillWidth: true
                            from: 0
                            to: 100
                            value: root.brightnessValue
                            onMoved: root.brightnessValue = value
                            onPressedChanged: {
                                if (!pressed) {
                                    root.openSettings("display")
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    radius: Theme.radiusCard
                    color: Theme.color("surfaceRaised")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("panelBorder")
                    implicitHeight: Theme.metric("controlHeightLarge") + Theme.metric("spacingLg")

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.metric("spacingSm")
                        spacing: Theme.metric("spacingXs")

                        Label {
                            text: "Sound"
                            color: Theme.color("textPrimary")
                            font.pixelSize: Theme.fontSize("body")
                            font.family: Theme.fontFamilyUi
                        }

                        Slider {
                            Layout.fillWidth: true
                            from: 0
                            to: 150
                            value: root.soundManager ? root.soundManager.volume : 0
                            onMoved: {
                                if (root.soundManager && root.soundManager.setVolume) {
                                    root.soundManager.setVolume(Math.round(value))
                                }
                            }
                            onPressedChanged: {
                                if (!pressed) {
                                    root.openSettings("sound")
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    radius: Theme.radiusCard
                    color: Theme.color("surfaceRaised")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("panelBorder")
                    implicitHeight: Theme.metric("controlHeightLarge") + Theme.metric("spacingLg")

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.metric("spacingSm")
                        spacing: Theme.metric("spacingSm")

                        Rectangle {
                            Layout.preferredWidth: Theme.metric("controlHeightLarge") + Theme.metric("spacingSm")
                            Layout.preferredHeight: Theme.metric("controlHeightLarge") + Theme.metric("spacingSm")
                            radius: Theme.radiusControl
                            color: Theme.color("controlBg")
                            border.width: Theme.borderWidthThin
                            border.color: Theme.color("panelBorder")

                            IconImage {
                                anchors.centerIn: parent
                                width: Theme.metric("controlHeightRegular")
                                height: Theme.metric("controlHeightRegular")
                                source: root.iconSourceByName("audio-x-generic-symbolic")
                                color: Theme.color("textPrimary")
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: "Music"
                            color: Theme.color("textPrimary")
                            font.pixelSize: Theme.fontSize("body")
                            font.family: Theme.fontFamilyUi
                            font.weight: Theme.fontWeight("semiBold")
                        }

                        ToolButton {
                            icon.source: root.iconSourceByName("media-playback-start-symbolic")
                            onClicked: root.openSettings("sound")
                        }
                    }
                }
            }
        }
    }
}
