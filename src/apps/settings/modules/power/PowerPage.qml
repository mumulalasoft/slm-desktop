import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "file:/usr/lib/settings/components"

Flickable {
    id: root
    contentHeight: contentColumn.height
    clip: true
    property var powerModeBinding: SettingsApp.createBindingFor("power", "power-mode", "Balanced")
    property var suspendBinding: SettingsApp.createBinding("settings:power/suspend", true)

    ColumnLayout {
        id: contentColumn
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 20
        spacing: 24

        Text {
            text: "Power"
            font.pixelSize: 28
            font.weight: Font.Bold
            color: "#ffffff"
        }

        // Battery Section
        SettingGroup {
            title: "Battery"
            Layout.fillWidth: true

            SettingCard {
                label: "Battery Level"
                description: "Built-in Battery"
                
                RowLayout {
                    spacing: 8
                    ProgressBar {
                        value: 0.85
                        Layout.preferredWidth: 150
                    }
                    Text {
                        text: "85%"
                        color: "#ffffff"
                        font.weight: Font.Medium
                    }
                }
            }

            SettingCard {
                label: "State"
                description: "Discharging"
                
                Text {
                    text: "2h 45m remaining"
                    color: "#aaaaaa"
                    font.pixelSize: 12
                }
            }
        }

        // Performance Section
        SettingGroup {
            title: "Performance Mode"
            Layout.fillWidth: true

            SettingCard {
                label: "Power Mode"
                description: "Balance performance and battery life"
                
                ComboBox {
                    id: powerModeCombo
                    model: ["Power Saver", "Balanced", "Performance"]
                    currentIndex: Math.max(0, model.indexOf(String(root.powerModeBinding.value)))
                    Layout.preferredWidth: 150
                    onActivated: root.powerModeBinding.value = currentText
                }
            }
        }

        // Sleep Section
        SettingGroup {
            title: "Sleep & Suspend"
            Layout.fillWidth: true

            SettingCard {
                label: "Screen Off"
                description: "Turn off screen when inactive"
                
                ComboBox {
                    model: ["1 minute", "2 minutes", "5 minutes", "10 minutes", "Never"]
                    currentIndex: 2
                    Layout.preferredWidth: 150
                }
            }

            SettingCard {
                label: "Automatic Suspend"
                description: "Suspend when inactive"
                
                SettingToggle {
                    id: suspendToggle
                    checked: Boolean(root.suspendBinding.value)
                    onToggled: root.suspendBinding.value = checked
                }
            }
        }
    }
}
