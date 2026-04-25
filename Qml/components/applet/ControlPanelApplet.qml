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
    readonly property int iconSize: 22
    readonly property int popupGap: Theme.metric("spacingSm")
    readonly property int popupDropExtra: 0
    readonly property int popupWidth: 336
    readonly property bool popupOpen: popupHint || menu.opened

    // ── Inline components ────────────────────────────────────────────────────

    component ControlTile: Rectangle {
        id: tile

        property bool isActive: false
        property string tileIconName: ""
        property string tileLabel: ""
        property string tileStatus: ""
        property bool longPressEnabled: false

        signal tileTapped()
        signal tileLongPressed()

        function iconSrc(name) {
            return "image://themeicon/" + name + "?v=" +
                   ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                    ? ThemeIconController.revision : 0)
        }

        Layout.fillWidth: true
        Layout.preferredHeight: 84

        radius: Theme.radiusCard
        color: {
            if (tile.isActive)
                return tileHover.containsMouse ? Theme.color("accentHover") : Theme.color("accent")
            return tileHover.containsMouse ? Theme.color("controlBgHover") : Theme.color("surface")
        }
        border.width: Theme.borderWidthThin
        border.color: tile.isActive ? Theme.color("accent") : Theme.color("panelBorder")

        Behavior on color {
            enabled: Theme.animationsEnabled
            ColorAnimation { duration: Theme.durationSm; easing.type: Theme.easingDefault }
        }
        Behavior on border.color {
            enabled: Theme.animationsEnabled
            ColorAnimation { duration: Theme.durationSm; easing.type: Theme.easingDefault }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 0

            IconImage {
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
                source: tile.iconSrc(tile.tileIconName)
                color: tile.isActive ? Theme.color("accentText") : Theme.color("textPrimary")
            }

            Item { Layout.fillHeight: true }

            Label {
                Layout.fillWidth: true
                text: tile.tileLabel
                color: tile.isActive ? Theme.color("accentText") : Theme.color("textPrimary")
                font.family: Theme.fontFamilyUi
                font.pixelSize: Theme.fontSize("small")
                font.weight: Theme.fontWeight("medium")
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                text: tile.tileStatus
                color: tile.isActive ? Qt.alpha(Theme.color("accentText"), 0.65) : Theme.color("textSecondary")
                font.family: Theme.fontFamilyUi
                font.pixelSize: Theme.fontSize("tiny")
                elide: Text.ElideRight
            }
        }

        HoverHandler { id: tileHover }

        TapHandler {
            acceptedButtons: Qt.LeftButton
            onLongPressed: {
                if (tile.longPressEnabled) {
                    root.bluetoothLongPressConsumed = true
                    tile.tileLongPressed()
                }
            }
            onTapped: {
                if (root.bluetoothLongPressConsumed) {
                    root.bluetoothLongPressConsumed = false
                    return
                }
                tile.tileTapped()
            }
            onCanceled: root.bluetoothLongPressConsumed = false
        }

        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: tile.tileLongPressed()
        }
    }

    component SliderCard: Rectangle {
        id: sliderCard

        property string cardIconName: ""
        property string cardLabel: ""
        property real cardValue: 0
        property real cardFrom: 0
        property real cardTo: 100

        signal valueMoved(real v)
        signal labelTapped()

        function iconSrc(name) {
            return "image://themeicon/" + name + "?v=" +
                   ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                    ? ThemeIconController.revision : 0)
        }

        Layout.fillWidth: true
        radius: Theme.radiusCard
        color: Theme.color("surface")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("panelBorder")
        implicitHeight: 54

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.topMargin: 9
            anchors.bottomMargin: 5
            spacing: 2

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.metric("spacingXs")

                IconImage {
                    Layout.preferredWidth: 14
                    Layout.preferredHeight: 14
                    source: sliderCard.iconSrc(sliderCard.cardIconName)
                    color: Theme.color("textPrimary")
                }

                Label {
                    Layout.fillWidth: true
                    text: sliderCard.cardLabel
                    color: Theme.color("textPrimary")
                    font.family: Theme.fontFamilyUi
                    font.pixelSize: Theme.fontSize("small")
                    font.weight: Theme.fontWeight("medium")
                    HoverHandler { id: labelHover }
                    TapHandler { onTapped: sliderCard.labelTapped() }
                }

                Label {
                    text: Math.round(sliderCard.cardValue) + "%"
                    color: Theme.color("textSecondary")
                    font.family: Theme.fontFamilyUi
                    font.pixelSize: Theme.fontSize("small")
                }
            }

            Slider {
                Layout.fillWidth: true
                from: sliderCard.cardFrom
                to: sliderCard.cardTo
                value: sliderCard.cardValue
                onMoved: sliderCard.valueMoved(value)
            }
        }
    }

    // ── Helpers ──────────────────────────────────────────────────────────────

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
        if (iconName.length === 0) iconName = "preferences-system-symbolic"
        return "image://themeicon/" + iconName + "?v=" +
                ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                 ? ThemeIconController.revision : 0)
    }

    function microAnimationAllowed() {
        if (!Theme.animationsEnabled) return false
        if (typeof MotionController === "undefined" || !MotionController || !MotionController.allowMotionPriority)
            return true
        return MotionController.allowMotionPriority(MotionController.LowPriority)
    }

    function openMenuSafely() {
        if ((Date.now() - lastMenuCloseMs) < 180) return
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
                                   "control-panel")
        } else if (typeof AppExecutionGate !== "undefined" && AppExecutionGate
                   && AppExecutionGate.launchDesktopId) {
            AppExecutionGate.launchDesktopId("slm-settings.desktop", "control-panel")
        }
    }

    function loadBluetoothPreference() {
        if (typeof DesktopSettings === "undefined" || !DesktopSettings || !DesktopSettings.settingValue)
            return
        root.bluetoothEnabledPreference = !!DesktopSettings.settingValue("bluetooth.enabled", false)
        root.bluetoothPreferenceKnown = true
    }

    function effectiveBluetoothPowered() {
        if (root.bluetoothPreferenceKnown) return root.bluetoothEnabledPreference
        if (root.bluetoothManager && root.bluetoothManager.powered !== undefined
                && root.bluetoothManager.powered !== null)
            return !!root.bluetoothManager.powered
        return false
    }

    function setBluetoothEnabled(enabled) {
        var on = !!enabled
        root.bluetoothEnabledPreference = on
        root.bluetoothPreferenceKnown = true
        if (typeof DesktopSettings !== "undefined" && DesktopSettings && DesktopSettings.setSettingValue)
            DesktopSettings.setSettingValue("bluetooth.enabled", on)
        if (root.bluetoothManager && root.bluetoothManager.setPowered)
            root.bluetoothManager.setPowered(on)
    }

    function toggleBluetooth() {
        root.setBluetoothEnabled(!root.effectiveBluetoothPowered())
    }

    // ── Topbar indicator ─────────────────────────────────────────────────────

    ToolButton {
        id: indicatorButton
        anchors.fill: parent
        padding: 0

        onClicked: {
            if ((Date.now() - root.lastMenuCloseMs) < 220) return
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
                source: root.iconSourceByName("preferences-system-symbolic")
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

    // ── Control Panel popup ──────────────────────────────────────────────────

    IndicatorMenu {
        id: menu
        appletId: "crown.controlpanel"
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
                spacing: Theme.metric("spacingSm")

                // ── Header ────────────────────────────────────────────────────
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Control Panel")
                        color: Theme.color("textPrimary")
                        font.family: Theme.fontFamilyUi
                        font.pixelSize: Theme.fontSize("bodyLarge")
                        font.weight: Theme.fontWeight("semiBold")
                    }

                    ToolButton {
                        implicitWidth: 28
                        implicitHeight: 28
                        padding: 0

                        contentItem: Item {
                            IconImage {
                                anchors.centerIn: parent
                                width: 14
                                height: 14
                                source: root.iconSourceByName("go-next-symbolic")
                                color: Theme.color("textSecondary")
                            }
                        }

                        background: Rectangle {
                            radius: Theme.radiusControl
                            color: parent.hovered ? Theme.color("controlBgHover") : "transparent"
                            Behavior on color {
                                enabled: root.microAnimationAllowed()
                                ColorAnimation { duration: Theme.durationSm; easing.type: Theme.easingDefault }
                            }
                        }

                        onClicked: root.openSettings("")
                    }
                }

                // ── Quick-toggle tiles (3 × 2 grid) ───────────────────────────
                GridLayout {
                    Layout.fillWidth: true
                    columns: 3
                    rowSpacing: Theme.metric("spacingXs")
                    columnSpacing: Theme.metric("spacingXs")

                    ControlTile {
                        isActive: root.networkManager ? !!root.networkManager.online : false
                        tileIconName: "network-wireless-signal-good-symbolic"
                        tileLabel: qsTr("Wi-Fi")
                        tileStatus: (root.networkManager && root.networkManager.online)
                                    ? qsTr("Connected") : qsTr("Off")
                        onTileTapped: root.openSettings("network")
                        onTileLongPressed: root.openSettings("network")
                    }

                    ControlTile {
                        isActive: root.effectiveBluetoothPowered()
                        tileIconName: root.effectiveBluetoothPowered()
                                      ? "bluetooth-active-symbolic"
                                      : "bluetooth-disabled-symbolic"
                        tileLabel: qsTr("Bluetooth")
                        tileStatus: root.effectiveBluetoothPowered() ? qsTr("On") : qsTr("Off")
                        longPressEnabled: true
                        onTileTapped: root.toggleBluetooth()
                        onTileLongPressed: root.openSettings("bluetooth")
                    }

                    ControlTile {
                        isActive: root.notificationManager ? !!root.notificationManager.doNotDisturb : false
                        tileIconName: "notifications-disabled-symbolic"
                        tileLabel: qsTr("Focus")
                        tileStatus: (root.notificationManager && root.notificationManager.doNotDisturb)
                                    ? qsTr("On") : qsTr("Off")
                        onTileTapped: {
                            if (root.notificationManager && root.notificationManager.setDoNotDisturb)
                                root.notificationManager.setDoNotDisturb(!root.notificationManager.doNotDisturb)
                        }
                        onTileLongPressed: root.openSettings("notifications")
                    }

                    ControlTile {
                        isActive: false
                        tileIconName: "folder-remote-symbolic"
                        tileLabel: qsTr("Sharing")
                        tileStatus: qsTr("Off")
                        onTileTapped: root.openSettings("sharing")
                        onTileLongPressed: root.openSettings("sharing")
                    }

                    ControlTile {
                        isActive: false
                        tileIconName: "video-display-symbolic"
                        tileLabel: qsTr("Mirroring")
                        tileStatus: qsTr("Off")
                        onTileTapped: root.openSettings("display")
                        onTileLongPressed: root.openSettings("display")
                    }

                    ControlTile {
                        isActive: false
                        tileIconName: "night-light-symbolic"
                        tileLabel: qsTr("Night Light")
                        tileStatus: qsTr("Off")
                        onTileTapped: root.openSettings("display")
                        onTileLongPressed: root.openSettings("display")
                    }
                }

                // ── Brightness ────────────────────────────────────────────────
                SliderCard {
                    cardIconName: "display-brightness-symbolic"
                    cardLabel: qsTr("Display")
                    cardValue: root.brightnessValue
                    cardFrom: 0
                    cardTo: 100
                    onValueMoved: function(v) { root.brightnessValue = v }
                    onLabelTapped: root.openSettings("display")
                }

                // ── Sound ─────────────────────────────────────────────────────
                SliderCard {
                    cardIconName: {
                        var v = root.soundManager ? root.soundManager.volume : 0
                        if (v > 66) return "audio-volume-high-symbolic"
                        if (v > 33) return "audio-volume-medium-symbolic"
                        if (v > 0)  return "audio-volume-low-symbolic"
                        return "audio-volume-muted-symbolic"
                    }
                    cardLabel: qsTr("Sound")
                    cardValue: root.soundManager ? root.soundManager.volume : 0
                    cardFrom: 0
                    cardTo: 150
                    onValueMoved: function(v) {
                        if (root.soundManager && root.soundManager.setVolume)
                            root.soundManager.setVolume(Math.round(v))
                    }
                    onLabelTapped: root.openSettings("sound")
                }

                // ── Now Playing ───────────────────────────────────────────────
                Rectangle {
                    Layout.fillWidth: true
                    radius: Theme.radiusCard
                    color: Theme.color("surface")
                    border.width: Theme.borderWidthThin
                    border.color: Theme.color("panelBorder")
                    implicitHeight: 56

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 8
                        anchors.topMargin: 10
                        anchors.bottomMargin: 10
                        spacing: Theme.metric("spacingSm")

                        Rectangle {
                            width: 36
                            height: 36
                            radius: Theme.radiusMdPlus
                            color: Theme.color("controlBg")
                            border.width: Theme.borderWidthThin
                            border.color: Theme.color("panelBorder")
                            IconImage {
                                anchors.centerIn: parent
                                width: 18
                                height: 18
                                source: root.iconSourceByName("audio-x-generic-symbolic")
                                color: Theme.color("textSecondary")
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Not Playing")
                                color: Theme.color("textPrimary")
                                font.family: Theme.fontFamilyUi
                                font.pixelSize: Theme.fontSize("body")
                                font.weight: Theme.fontWeight("medium")
                                elide: Text.ElideRight
                            }

                            Label {
                                text: qsTr("Music")
                                color: Theme.color("textSecondary")
                                font.family: Theme.fontFamilyUi
                                font.pixelSize: Theme.fontSize("small")
                            }
                        }

                        ToolButton {
                            implicitWidth: 30
                            implicitHeight: 30
                            padding: 0

                            contentItem: Item {
                                IconImage {
                                    anchors.centerIn: parent
                                    width: 16
                                    height: 16
                                    source: root.iconSourceByName("media-playback-start-symbolic")
                                    color: Theme.color("textPrimary")
                                }
                            }

                            background: Rectangle {
                                radius: Theme.radiusControl
                                color: parent.hovered ? Theme.color("controlBgHover") : "transparent"
                                Behavior on color {
                                    enabled: root.microAnimationAllowed()
                                    ColorAnimation { duration: Theme.durationSm; easing.type: Theme.easingDefault }
                                }
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
            if (String(path || "") === "bluetooth.enabled") root.loadBluetoothPreference()
        }
        function onAvailableChanged() {
            root.loadBluetoothPreference()
        }
    }

    Component.onCompleted: loadBluetoothPreference()
}
