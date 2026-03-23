import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import Style

Item {
    id: root

    property var batteryManager: BatteryManager
    property var popupHost: null
    property bool showPercentage: false
    property int textYOffset: 0
    property bool popupHint: false
    property double lastMenuCloseMs: 0
    readonly property int iconSize: 22
    readonly property int popupGap: Theme.metric("spacingXs")
    readonly property int rowGap: Theme.metric("spacingMd")
    readonly property bool popupOpen: popupHint || batteryMenu.opened

    Timer {
        id: popupHintTimer
        interval: 320
        repeat: false
        onTriggered: root.popupHint = false
    }

    implicitWidth: indicatorButton.implicitWidth
    implicitHeight: indicatorButton.implicitHeight

    function openMenuSafely() {
        if ((Date.now() - lastMenuCloseMs) < 180) {
            return
        }
        root.popupHint = true
        popupHintTimer.restart()
        Qt.callLater(function() { batteryMenu.open() })
    }

    function iconSourceByName(name) {
        var iconName = (name || "").toString().trim()
        if (iconName.length === 0) {
            iconName = "battery-missing-symbolic"
        }
        return "image://themeicon/" + iconName + "?v=" +
                ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                 ? ThemeIconController.revision : 0)
    }

    function loadPreference() {
        if (typeof UIPreferences === "undefined" || !UIPreferences || !UIPreferences.getPreference) {
            return
        }
        var value = UIPreferences.getPreference("battery.showPercentage", false)
        root.showPercentage = !!value
    }

    function savePreference() {
        if (typeof UIPreferences === "undefined" || !UIPreferences || !UIPreferences.setPreference) {
            return
        }
        UIPreferences.setPreference("battery.showPercentage", root.showPercentage)
    }

    Component.onCompleted: loadPreference()

    ToolButton {
        id: indicatorButton
        anchors.fill: parent
        padding: 0
        onClicked: {
            if ((Date.now() - root.lastMenuCloseMs) < 220) {
                return
            }
            if (batteryMenu.opened || batteryMenu.visible) {
                root.lastMenuCloseMs = Date.now()
                batteryMenu.close()
                return
            }
            root.openMenuSafely()
        }

        contentItem: Item {
            implicitHeight: Theme.metric("controlHeightRegular")
            implicitWidth: batteryIcon.width + (batteryText.visible ? (Theme.metric("spacingXs") + batteryText.implicitWidth) : 0)

            Image {
                id: batteryIcon
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                width: root.iconSize
                height: root.iconSize
                fillMode: Image.PreserveAspectFit
                source: root.iconSourceByName(root.batteryManager ? root.batteryManager.iconName : "battery-missing-symbolic")
            }

            Text {
                id: batteryText
                visible: root.showPercentage
                anchors.left: batteryIcon.right
                anchors.leftMargin: Theme.metric("spacingXs")
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: root.textYOffset
                text: root.batteryManager ? (root.batteryManager.percentage + "%") : "--%"
                color: Theme.color("textOnGlass")
                font.family: Theme.fontFamilyUi
                font.pixelSize: Theme.fontSize("body")
                font.weight: Theme.fontWeight("semibold")
                renderType: Text.NativeRendering
            }
        }

        background: Rectangle {
            radius: Theme.radiusControl
            color: indicatorButton.hovered ? Theme.color("accentSoft") : "transparent"
            border.width: Theme.borderWidthThin
            border.color: indicatorButton.hovered ? Theme.color("panelBorder") : "transparent"
        }
    }

    IndicatorMenu {
        id: batteryMenu
        anchorItem: indicatorButton
        popupGap: root.popupGap
        popupWidth: Theme.metric("popupWidthS")

        onAboutToShow: {
            root.popupHint = false
            if (root.batteryManager) {
                root.batteryManager.refresh()
            }
        }
        onAboutToHide: {
            root.popupHint = false
            root.lastMenuCloseMs = Date.now()
        }

        MenuItem {
            contentItem: IndicatorSectionRow {
                text: "Show Percentage"
                rowSpacing: root.rowGap
                Switch {
                    checked: root.showPercentage
                    onToggled: {
                        root.showPercentage = checked
                        root.savePreference()
                    }
                }
            }
        }

        MenuItem {
            enabled: false
            contentItem: IndicatorSectionLabel {
                text: root.batteryManager ? root.batteryManager.levelText : "Battery --%"
                emphasized: true
            }
        }

        MenuItem {
            enabled: false
            contentItem: IndicatorSectionLabel {
                text: root.batteryManager ? root.batteryManager.durationText : "Battery status unknown"
            }
        }

        MenuSeparator {}

        MenuItem {
            text: "Power Saver"
            checkable: true
            enabled: root.batteryManager && root.batteryManager.canSetPowerProfile
            checked: root.batteryManager && root.batteryManager.powerProfile === "power-saver"
            onTriggered: {
                if (root.batteryManager) {
                    root.batteryManager.setPowerProfile("power-saver")
                }
            }
        }

        MenuItem {
            text: "Balanced"
            checkable: true
            enabled: root.batteryManager && root.batteryManager.canSetPowerProfile
            checked: root.batteryManager && root.batteryManager.powerProfile === "balanced"
            onTriggered: {
                if (root.batteryManager) {
                    root.batteryManager.setPowerProfile("balanced")
                }
            }
        }

        MenuItem {
            text: "Performance"
            checkable: true
            enabled: root.batteryManager && root.batteryManager.canSetPowerProfile
            checked: root.batteryManager && root.batteryManager.powerProfile === "performance"
            onTriggered: {
                if (root.batteryManager) {
                    root.batteryManager.setPowerProfile("performance")
                }
            }
        }

        MenuSeparator {}

        MenuItem {
            text: "Open Power Settings"
            onTriggered: {
                if (root.batteryManager) {
                    root.batteryManager.openPowerSettings()
                }
            }
        }
    }
}
