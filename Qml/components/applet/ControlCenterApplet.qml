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
    property bool bluetoothEnabledPreference: false
    property bool bluetoothPreferenceKnown: false
    property bool bluetoothLongPressConsumed: false
    property bool popupHint: false
    property double lastMenuCloseMs: 0
    property real brightnessValue: 72
    readonly property int iconSize: 20
    readonly property int popupGap: Theme.metric("spacingSm")
    readonly property int popupDropExtra: 0
    readonly property int sectionGap: Theme.metric("spacingSm")
    readonly property int itemGap: Theme.metric("spacingXs")
    readonly property int popupWidth: 336
    readonly property int cardPadding: Theme.metric("spacingSm")
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

    function loadBluetoothPreference() {
        if (typeof DesktopSettings === "undefined" || !DesktopSettings || !DesktopSettings.settingValue) {
            return
        }
        root.bluetoothEnabledPreference = !!DesktopSettings.settingValue("bluetooth.enabled", false)
        root.bluetoothPreferenceKnown = true
    }

    function effectiveBluetoothPowered() {
        if (root.bluetoothPreferenceKnown) {
            return root.bluetoothEnabledPreference
        }
        if (root.bluetoothManager && root.bluetoothManager.powered !== undefined && root.bluetoothManager.powered !== null) {
            return !!root.bluetoothManager.powered
        }
        return false
    }

    function setBluetoothEnabled(enabled) {
        var on = !!enabled
        root.bluetoothEnabledPreference = on
        root.bluetoothPreferenceKnown = true
        if (typeof DesktopSettings !== "undefined" && DesktopSettings && DesktopSettings.setSettingValue) {
            DesktopSettings.setSettingValue("bluetooth.enabled", on)
        }
        if (root.bluetoothManager && root.bluetoothManager.setPowered) {
            root.bluetoothManager.setPowered(on)
        }
    }

    function toggleBluetooth() {
        root.setBluetoothEnabled(!root.effectiveBluetoothPowered())
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
        appletId: "crown.controlcenter"
        anchorItem: indicatorButton
        popupHost: root.popupHost
        popupGap: root.popupGap
        popupDropExtra: root.popupDropExtra
        popupWidth: root.popupWidth
        padding: Theme.metric("spacingXs")
        onAboutToShow: {
            root.popupHint = false
            root.loadBluetoothPreference()
            if (root.networkManager && root.networkManager.refresh) root.networkManager.refresh()
            if (root.bluetoothManager && root.bluetoothManager.refresh) root.bluetoothManager.refresh()
            if (root.soundManager && root.soundManager.refresh) root.soundManager.refresh()
        }
        onAboutToHide: {
            root.popupHint = false
            root.lastMenuCloseMs = Date.now()
        }

        MenuItem {
            enabled: true
            hoverEnabled: false
            onTriggered: {}
            background: Rectangle {
                color: "transparent"
                border.width: Theme.borderWidthThin
                border.color: "transparent"
            }
            contentItem: ColumnLayout {
                spacing: root.sectionGap

                RowLayout {
                    Layout.fillWidth: true
                    spacing: root.itemGap

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Theme.metric("controlHeightLarge") * 3
                                                + (root.cardPadding * 2)
                                                + (Theme.metric("spacingXs") * 2)
                        radius: Theme.radiusCard
                        color: Theme.color("surface")
                        border.width: Theme.borderWidthThin
                        border.color: Theme.color("panelBorder")

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: root.cardPadding
                            spacing: Theme.metric("spacingXs")

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: Theme.metric("controlHeightLarge")
                                radius: Theme.radiusControl
                                color: wifiArea.containsMouse ? Theme.color("controlBgHover") : Theme.color("controlBg")
                                border.width: Theme.borderWidthThin
                                border.color: Theme.color("panelBorder")
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: Theme.metric("spacingSm")
                                    anchors.rightMargin: Theme.metric("spacingSm")
                                    spacing: Theme.metric("spacingXs")
                                    IconImage {
                                        Layout.preferredWidth: 16
                                        Layout.preferredHeight: 16
                                        source: root.iconSourceByName("network-wireless-signal-good-symbolic")
                                        color: Theme.color("textPrimary")
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 0
                                        Label { text: "Wi-Fi"; color: Theme.color("textPrimary"); font.family: Theme.fontFamilyUi; font.pixelSize: Theme.fontSize("body") }
                                        Label { text: root.networkManager && root.networkManager.online ? "Connected" : "Not Connected"; color: Theme.color("textSecondary"); font.family: Theme.fontFamilyUi; font.pixelSize: Theme.fontSize("small") }
                                    }
                                }
                                HoverHandler { id: wifiArea }
                                TapHandler { onTapped: root.openSettings("network") }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: Theme.metric("controlHeightLarge")
                                radius: Theme.radiusControl
                                color: root.effectiveBluetoothPowered()
                                       ? (btArea.containsMouse ? Theme.color("accentHover") : Theme.color("accent"))
                                       : (btArea.containsMouse ? Theme.color("controlBgHover") : Theme.color("controlBg"))
                                border.width: Theme.borderWidthThin
                                border.color: root.effectiveBluetoothPowered()
                                              ? Theme.color("accent")
                                              : Theme.color("panelBorder")
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: Theme.metric("spacingSm")
                                    anchors.rightMargin: Theme.metric("spacingSm")
                                    spacing: Theme.metric("spacingXs")
                                    IconImage {
                                        Layout.preferredWidth: 16
                                        Layout.preferredHeight: 16
                                        source: root.iconSourceByName(root.effectiveBluetoothPowered()
                                                                      ? "bluetooth-active-symbolic"
                                                                      : "bluetooth-disabled-symbolic")
                                        color: root.effectiveBluetoothPowered()
                                               ? Theme.color("accentText")
                                               : Theme.color("textPrimary")
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 0
                                        Label {
                                            text: "Bluetooth"
                                            color: root.effectiveBluetoothPowered()
                                                   ? Theme.color("accentText")
                                                   : Theme.color("textPrimary")
                                            font.family: Theme.fontFamilyUi
                                            font.pixelSize: Theme.fontSize("body")
                                        }
                                        Label {
                                            text: root.effectiveBluetoothPowered() ? "On" : "Off"
                                            color: root.effectiveBluetoothPowered()
                                                   ? Theme.color("accentText")
                                                   : Theme.color("textSecondary")
                                            font.family: Theme.fontFamilyUi
                                            font.pixelSize: Theme.fontSize("small")
                                        }
                                    }
                                }
                                HoverHandler { id: btArea }
                                TapHandler {
                                    acceptedButtons: Qt.LeftButton
                                    onLongPressed: {
                                        root.bluetoothLongPressConsumed = true
                                        root.openSettings("bluetooth")
                                    }
                                    onTapped: {
                                        if (root.bluetoothLongPressConsumed) {
                                            root.bluetoothLongPressConsumed = false
                                            return
                                        }
                                        root.toggleBluetooth()
                                    }
                                    onCanceled: root.bluetoothLongPressConsumed = false
                                }
                                TapHandler {
                                    acceptedButtons: Qt.RightButton
                                    onTapped: root.openSettings("bluetooth")
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: Theme.metric("controlHeightLarge")
                                radius: Theme.radiusControl
                                color: slareArea.containsMouse ? Theme.color("controlBgHover") : Theme.color("controlBg")
                                border.width: Theme.borderWidthThin
                                border.color: Theme.color("panelBorder")
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: Theme.metric("spacingSm")
                                    anchors.rightMargin: Theme.metric("spacingSm")
                                    spacing: Theme.metric("spacingXs")
                                    IconImage {
                                        Layout.preferredWidth: 16
                                        Layout.preferredHeight: 16
                                        source: root.iconSourceByName("folder-remote-symbolic")
                                        color: Theme.color("textPrimary")
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 0
                                        Label { text: "Slare"; color: Theme.color("textPrimary"); font.family: Theme.fontFamilyUi; font.pixelSize: Theme.fontSize("body") }
                                        Label { text: "Off"; color: Theme.color("textSecondary"); font.family: Theme.fontFamilyUi; font.pixelSize: Theme.fontSize("small") }
                                    }
                                }
                                HoverHandler { id: slareArea }
                                TapHandler { onTapped: root.openSettings("sharing") }
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
                            color: dndArea.containsMouse ? Theme.color("controlBgHover") : Theme.color("surface")
                            border.width: Theme.borderWidthThin
                            border.color: Theme.color("panelBorder")
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: root.cardPadding
                                spacing: Theme.metric("spacingXs")
                                IconImage {
                                    Layout.preferredWidth: 18
                                    Layout.preferredHeight: 18
                                    source: root.iconSourceByName("weather-clear-night-symbolic")
                                    color: Theme.color("textPrimary")
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 0
                                    Label {
                                        Layout.fillWidth: true
                                        text: "Do Not Disturb"
                                        color: Theme.color("textPrimary")
                                        font.family: Theme.fontFamilyUi
                                        font.pixelSize: Theme.fontSize("body")
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: root.notificationManager && root.notificationManager.doNotDisturb ? "On" : "Off"
                                        color: Theme.color("textSecondary")
                                        font.family: Theme.fontFamilyUi
                                        font.pixelSize: Theme.fontSize("small")
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                            HoverHandler { id: dndArea }
                            TapHandler {
                                onTapped: {
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
                            color: mirrorArea.containsMouse ? Theme.color("controlBgHover") : Theme.color("surface")
                            border.width: Theme.borderWidthThin
                            border.color: Theme.color("panelBorder")
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: root.cardPadding
                                spacing: Theme.metric("spacingXs")
                                IconImage {
                                    Layout.preferredWidth: 18
                                    Layout.preferredHeight: 18
                                    source: root.iconSourceByName("video-display-symbolic")
                                    color: Theme.color("textPrimary")
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 0
                                    Label {
                                        Layout.fillWidth: true
                                        text: "Screen Mirroring"
                                        color: Theme.color("textPrimary")
                                        font.family: Theme.fontFamilyUi
                                        font.pixelSize: Theme.fontSize("body")
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: "Off"
                                        color: Theme.color("textSecondary")
                                        font.family: Theme.fontFamilyUi
                                        font.pixelSize: Theme.fontSize("small")
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                            HoverHandler { id: mirrorArea }
                            TapHandler { onTapped: root.openSettings("display") }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    radius: Theme.radiusCard
                    color: Theme.color("surface")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("panelBorder")
                    implicitHeight: Theme.metric("controlHeightLarge") + Theme.metric("spacingLg")

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: root.cardPadding
                        spacing: Theme.metric("spacingXs")
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.metric("spacingXs")
                            IconImage {
                                Layout.preferredWidth: 16
                                Layout.preferredHeight: 16
                                source: root.iconSourceByName("video-display-symbolic")
                                color: Theme.color("textPrimary")
                            }
                            Label {
                                Layout.fillWidth: true
                                text: "Display"
                                color: Theme.color("textPrimary")
                                font.pixelSize: Theme.fontSize("body")
                                font.family: Theme.fontFamilyUi
                            }
                            Label {
                                text: Math.round(root.brightnessValue) + "%"
                                color: Theme.color("textSecondary")
                                font.pixelSize: Theme.fontSize("small")
                                font.family: Theme.fontFamilyUi
                            }
                        }
                        Slider {
                            Layout.fillWidth: true
                            from: 0
                            to: 100
                            value: root.brightnessValue
                            onMoved: root.brightnessValue = value
                            onPressedChanged: if (!pressed) root.openSettings("display")
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    radius: Theme.radiusCard
                    color: Theme.color("surface")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("panelBorder")
                    implicitHeight: Theme.metric("controlHeightLarge") + Theme.metric("spacingLg")

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: root.cardPadding
                        spacing: Theme.metric("spacingXs")
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.metric("spacingXs")
                            IconImage {
                                Layout.preferredWidth: 16
                                Layout.preferredHeight: 16
                                source: root.iconSourceByName("audio-volume-medium-symbolic")
                                color: Theme.color("textPrimary")
                            }
                            Label {
                                Layout.fillWidth: true
                                text: "Sound"
                                color: Theme.color("textPrimary")
                                font.pixelSize: Theme.fontSize("body")
                                font.family: Theme.fontFamilyUi
                            }
                            Label {
                                text: root.soundManager ? (Math.round(root.soundManager.volume) + "%") : "--%"
                                color: Theme.color("textSecondary")
                                font.pixelSize: Theme.fontSize("small")
                                font.family: Theme.fontFamilyUi
                            }
                        }
                        Slider {
                            Layout.fillWidth: true
                            from: 0
                            to: 150
                            value: root.soundManager ? root.soundManager.volume : 0
                            onMoved: if (root.soundManager && root.soundManager.setVolume) root.soundManager.setVolume(Math.round(value))
                            onPressedChanged: if (!pressed) root.openSettings("sound")
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    radius: Theme.radiusCard
                    color: Theme.color("surface")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("panelBorder")
                    implicitHeight: Theme.metric("controlHeightLarge") + Theme.metric("spacingLg")

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: root.cardPadding
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
                            Layout.preferredWidth: Theme.metric("controlHeightLarge")
                            Layout.preferredHeight: Theme.metric("controlHeightLarge")
                            icon.source: root.iconSourceByName("media-playback-start-symbolic")
                            background: Rectangle {
                                radius: Theme.radiusControl
                                color: parent.hovered ? Theme.color("controlBgHover") : Theme.color("controlBg")
                                border.width: Theme.borderWidthThin
                                border.color: Theme.color("panelBorder")
                            }
                            onClicked: root.openSettings("sound")
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: (typeof DesktopSettings !== "undefined") ? DesktopSettings : null
        function onSettingChanged(path) {
            if (String(path || "") === "bluetooth.enabled") {
                root.loadBluetoothPreference()
            }
        }
        function onAvailableChanged() {
            root.loadBluetoothPreference()
        }
    }

    Component.onCompleted: loadBluetoothPreference()
}
