import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import "file:/usr/lib/settings/components"

Flickable {
    id: root
    anchors.fill: parent
    contentHeight: mainLayout.implicitHeight + 48
    clip: true
    property string highlightSettingId: ""
    property var outputVolumeBinding: SettingsApp.createBindingFor("sound", "output-volume", 75)
    property var outputDeviceBinding: SettingsApp.createBindingFor("sound", "output-device", "Internal Speakers")
    property var inputVolumeBinding: SettingsApp.createBindingFor("sound", "input-volume", 50)

    ColumnLayout {
        id: mainLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 24
        anchors.top: parent.top
        anchors.topMargin: 24
        spacing: 24

        SettingGroup {
            title: qsTr("Output")
            Layout.fillWidth: true

            SettingCard {
                objectName: "output-volume"
                highlighted: root.highlightSettingId === "output-volume"
                label: qsTr("Main Volume")
                description: qsTr("Adjust the overall system volume.")
                Layout.fillWidth: true

                RowLayout {
                    spacing: 10
                    Image {
                        source: "image://icon/audio-volume-low-symbolic"
                        width: 16; height: 16
                        smooth: true
                    }
                    Slider {
                        Layout.preferredWidth: 180
                        from: 0; to: 100
                        value: Number(root.outputVolumeBinding.value)
                        onMoved: root.outputVolumeBinding.value = Math.round(value)
                    }
                    Image {
                        source: "image://icon/audio-volume-high-symbolic"
                        width: 16; height: 16
                        smooth: true
                    }
                }
            }

            SettingCard {
                objectName: "output-device"
                highlighted: root.highlightSettingId === "output-device"
                label: qsTr("Output Device")
                description: qsTr("Select where the audio is played.")
                Layout.fillWidth: true

                ComboBox {
                    id: outputDeviceCombo
                    model: ["Internal Speakers", "Headphones (Bluetooth)", "External Monitor (HDMI)"]
                    implicitWidth: 220
                    currentIndex: Math.max(0, model.indexOf(String(root.outputDeviceBinding.value)))
                    onActivated: root.outputDeviceBinding.value = currentText
                }
            }
        }

        SettingGroup {
            title: qsTr("Input")
            Layout.fillWidth: true

            SettingCard {
                objectName: "input-volume"
                highlighted: root.highlightSettingId === "input-volume"
                label: qsTr("Input Volume")
                description: qsTr("Adjust the microphone sensitivity.")
                Layout.fillWidth: true

                RowLayout {
                    spacing: 10
                    Image {
                        source: "image://icon/audio-input-microphone-symbolic"
                        width: 16; height: 16
                        smooth: true
                    }
                    Slider {
                        Layout.preferredWidth: 180
                        from: 0; to: 100
                        value: Number(root.inputVolumeBinding.value)
                        onMoved: root.inputVolumeBinding.value = Math.round(value)
                    }
                }
            }
        }
    }

    Connections {
        target: root.outputDeviceBinding
        function onValueChanged() {
            outputDeviceCombo.currentIndex = Math.max(0, outputDeviceCombo.model.indexOf(String(root.outputDeviceBinding.value)))
        }
    }
}
