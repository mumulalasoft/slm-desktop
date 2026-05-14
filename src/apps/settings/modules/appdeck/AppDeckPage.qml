import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "file:/usr/lib/settings/components"

Flickable {
    id: root
    anchors.fill: parent
    contentHeight: mainLayout.implicitHeight + 40
    clip: true

    property string highlightSettingId: ""

    function _setExportSetting(path, value) {
        if (typeof DesktopSettings !== "undefined" && DesktopSettings && DesktopSettings.setSettingValue) {
            DesktopSettings.setSettingValue(path, value)
        }
    }

    function applyDesktopExportPreset(key) {
        var preset = String(key || "").toLowerCase()
        if (preset === "strict") {
            _setExportSetting("appdeck.desktopExportMinUpwardPx", 40)
            _setExportSetting("appdeck.desktopExportVerticalRatioPercent", 170)
            _setExportSetting("appdeck.desktopExportMaxHorizontalDriftPx", 28)
            return
        }
        if (preset === "easy") {
            _setExportSetting("appdeck.desktopExportMinUpwardPx", 16)
            _setExportSetting("appdeck.desktopExportVerticalRatioPercent", 115)
            _setExportSetting("appdeck.desktopExportMaxHorizontalDriftPx", 64)
            return
        }
        _setExportSetting("appdeck.desktopExportMinUpwardPx", 28)
        _setExportSetting("appdeck.desktopExportVerticalRatioPercent", 135)
        _setExportSetting("appdeck.desktopExportMaxHorizontalDriftPx", 42)
    }

    ColumnLayout {
        id: mainLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 24
        anchors.top: parent.top
        anchors.topMargin: 24
        spacing: 20

        SettingGroup {
            title: qsTr("AppDeck")
            Layout.fillWidth: true

            SettingCard {
                objectName: "icon-size"
                highlighted: root.highlightSettingId === "icon-size"
                label: qsTr("Icon Size")
                description: qsTr("Set the size of AppDeck icons.")
                Layout.fillWidth: true

                ComboBox {
                    id: iconSizeCombo
                    model: [qsTr("Small"), qsTr("Medium"), qsTr("Large")]
                    readonly property var sizeKeys: ["small", "medium", "large"]
                    currentIndex: {
                        if (typeof DesktopSettings === "undefined" || !DesktopSettings) return 1
                        var sz = DesktopSettings.dockIconSize || "medium"
                        var idx = sizeKeys.indexOf(sz)
                        return idx >= 0 ? idx : 1
                    }
                    onActivated: function(index) {
                        if (typeof DesktopSettings !== "undefined" && DesktopSettings
                                && DesktopSettings.setDockIconSize) {
                            DesktopSettings.setDockIconSize(sizeKeys[index])
                        }
                    }
                    Layout.preferredWidth: 160
                }
            }

            SettingCard {
                objectName: "magnification"
                highlighted: root.highlightSettingId === "magnification"
                label: qsTr("Magnification")
                description: qsTr("Enlarge icons when hovering over the AppDeck.")
                Layout.fillWidth: true

                SettingToggle {
                    checked: {
                        if (typeof DesktopSettings === "undefined" || !DesktopSettings) return true
                        return DesktopSettings.dockMagnificationEnabled !== false
                    }
                    onToggled: {
                        if (typeof DesktopSettings !== "undefined" && DesktopSettings
                                && DesktopSettings.setDockMagnificationEnabled) {
                            DesktopSettings.setDockMagnificationEnabled(checked)
                        }
                    }
                }
            }

            SettingCard {
                objectName: "desktop-export-preset"
                highlighted: root.highlightSettingId === "desktop-export-preset"
                label: qsTr("Desktop Export Preset")
                description: qsTr("Quick profile for drag sensitivity when exporting AppDeck items to desktop.")
                Layout.fillWidth: true

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    ComboBox {
                        id: exportPresetCombo
                        model: [qsTr("Strict"), qsTr("Balanced"), qsTr("Easy"), qsTr("Custom")]
                        readonly property var presetKeys: ["strict", "balanced", "easy", "custom"]
                        currentIndex: {
                            if (typeof DesktopSettings === "undefined" || !DesktopSettings) {
                                return 1
                            }
                            var up = Number(DesktopSettings.dockDesktopExportMinUpwardPx || 28)
                            var ratio = Number(DesktopSettings.dockDesktopExportVerticalRatioPercent || 135)
                            var drift = Number(DesktopSettings.dockDesktopExportMaxHorizontalDriftPx || 42)
                            if (up === 40 && ratio === 170 && drift === 28) {
                                return 0
                            }
                            if (up === 28 && ratio === 135 && drift === 42) {
                                return 1
                            }
                            if (up === 16 && ratio === 115 && drift === 64) {
                                return 2
                            }
                            return 3
                        }
                        onActivated: function(index) {
                            var key = presetKeys[index]
                            if (key === "custom") {
                                return
                            }
                            root.applyDesktopExportPreset(key)
                        }
                        Layout.preferredWidth: 180
                    }

                    Button {
                        text: qsTr("Reset to Balanced")
                        onClicked: root.applyDesktopExportPreset("balanced")
                        enabled: exportPresetCombo.currentIndex !== 1
                        Layout.preferredWidth: 160
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                }
            }

            SettingCard {
                objectName: "desktop-export-upward"
                highlighted: root.highlightSettingId === "desktop-export-upward"
                label: qsTr("Desktop Export Upward Distance")
                description: qsTr("Minimum upward drag distance (px) from AppDeck to start Add-to-Desktop export.")
                Layout.fillWidth: true

                SpinBox {
                    id: exportMinUpwardSpin
                    from: 8
                    to: 96
                    value: (typeof DesktopSettings !== "undefined" && DesktopSettings
                            && DesktopSettings.dockDesktopExportMinUpwardPx !== undefined)
                           ? DesktopSettings.dockDesktopExportMinUpwardPx : 28
                    onValueModified: {
                        if (typeof DesktopSettings !== "undefined" && DesktopSettings
                                && DesktopSettings.setDockDesktopExportMinUpwardPx) {
                            DesktopSettings.setDockDesktopExportMinUpwardPx(value)
                        }
                    }
                    Layout.preferredWidth: 160
                }
            }

            SettingCard {
                objectName: "desktop-export-ratio"
                highlighted: root.highlightSettingId === "desktop-export-ratio"
                label: qsTr("Desktop Export Vertical Ratio")
                description: qsTr("Require vertical drag dominance over horizontal movement (percent).")
                Layout.fillWidth: true

                SpinBox {
                    id: exportRatioSpin
                    from: 100
                    to: 260
                    stepSize: 5
                    value: (typeof DesktopSettings !== "undefined" && DesktopSettings
                            && DesktopSettings.dockDesktopExportVerticalRatioPercent !== undefined)
                           ? DesktopSettings.dockDesktopExportVerticalRatioPercent : 135
                    textFromValue: function(v) { return String(v) + "%" }
                    valueFromText: function(t) {
                        var n = parseInt(String(t).replace("%", ""))
                        return isNaN(n) ? value : n
                    }
                    onValueModified: {
                        if (typeof DesktopSettings !== "undefined" && DesktopSettings
                                && DesktopSettings.setDockDesktopExportVerticalRatioPercent) {
                            DesktopSettings.setDockDesktopExportVerticalRatioPercent(value)
                        }
                    }
                    Layout.preferredWidth: 160
                }
            }

            SettingCard {
                objectName: "desktop-export-drift"
                highlighted: root.highlightSettingId === "desktop-export-drift"
                label: qsTr("Desktop Export Max Horizontal Drift")
                description: qsTr("Maximum sideways drift (px) allowed while exporting AppDeck item to desktop.")
                Layout.fillWidth: true

                SpinBox {
                    id: exportDriftSpin
                    from: 8
                    to: 140
                    value: (typeof DesktopSettings !== "undefined" && DesktopSettings
                            && DesktopSettings.dockDesktopExportMaxHorizontalDriftPx !== undefined)
                           ? DesktopSettings.dockDesktopExportMaxHorizontalDriftPx : 42
                    onValueModified: {
                        if (typeof DesktopSettings !== "undefined" && DesktopSettings
                                && DesktopSettings.setDockDesktopExportMaxHorizontalDriftPx) {
                            DesktopSettings.setDockDesktopExportMaxHorizontalDriftPx(value)
                        }
                    }
                    Layout.preferredWidth: 160
                }
            }
        }
    }
}
