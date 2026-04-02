import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../../../../Qml/apps/settings/components"

Flickable {
    id: root
    contentHeight: contentColumn.height
    clip: true
    property string highlightSettingId: ""
    property var resolutionBinding: SettingsApp.createBindingFor("display", "resolution", "2560 x 1600 (16:10)")
    property var scalingBinding: SettingsApp.createBindingFor("display", "scaling", "125%")
    property var nightLightBinding: SettingsApp.createBindingFor("display", "night-light", false)

    ColumnLayout {
        id: contentColumn
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 20
        spacing: 24

        Text {
            text: "Display"
            font.pixelSize: 28
            font.weight: Font.Bold
            color: "#ffffff"
        }

        // Resolution Section
        SettingGroup {
            title: "Resolution & Scaling"
            Layout.fillWidth: true

            SettingCard {
                objectName: "resolution"
                highlighted: root.highlightSettingId === "resolution"
                label: "Resolution"
                description: "Built-in Display (2560 x 1600)"
                
                ComboBox {
                    id: resolutionCombo
                    model: ["2560 x 1600 (16:10)", "2048 x 1280", "1920 x 1200", "1680 x 1050"]
                    currentIndex: Math.max(0, model.indexOf(String(root.resolutionBinding.value)))
                    Layout.preferredWidth: 200
                    onActivated: root.resolutionBinding.value = currentText
                }
            }

            SettingCard {
                objectName: "scaling"
                highlighted: root.highlightSettingId === "scaling"
                label: "Scaling"
                description: "Adjust the size of text and icons"
                
                ComboBox {
                    id: scalingCombo
                    model: ["100%", "125%", "150%", "200%"]
                    currentIndex: Math.max(0, model.indexOf(String(root.scalingBinding.value)))
                    Layout.preferredWidth: 100
                    onActivated: root.scalingBinding.value = currentText
                }
            }
        }

        // Night Light Section
        SettingGroup {
            title: "Night Light"
            Layout.fillWidth: true

            SettingCard {
                objectName: "night-light"
                highlighted: root.highlightSettingId === "night-light"
                label: "Night Light"
                description: "Make screen colors warmer to help you sleep"
                
                SettingToggle {
                    id: nightLightToggle
                    checked: Boolean(root.nightLightBinding.value)
                    onToggled: root.nightLightBinding.value = checked
                }
            }

            SettingCard {
                label: "Schedule"
                visible: nightLightToggle.checked
                
                RowLayout {
                    spacing: 10
                    Text { text: "From"; color: "#aaaaaa" }
                    Button { text: "21:00"; flat: true }
                    Text { text: "to"; color: "#aaaaaa" }
                    Button { text: "07:00"; flat: true }
                }
            }
        }

        // Multiple Displays
        SettingGroup {
            title: "Multiple Displays"
            Layout.fillWidth: true

            SettingCard {
                label: "Arrangement"
                description: "Mirror or extend desktop"
                
                Button {
                    text: "Configure..."
                }
            }
        }
    }

    Connections {
        target: root.resolutionBinding
        function onValueChanged() {
            resolutionCombo.currentIndex = Math.max(0, resolutionCombo.model.indexOf(String(root.resolutionBinding.value)))
        }
    }

    Connections {
        target: root.scalingBinding
        function onValueChanged() {
            scalingCombo.currentIndex = Math.max(0, scalingCombo.model.indexOf(String(root.scalingBinding.value)))
        }
    }
}
