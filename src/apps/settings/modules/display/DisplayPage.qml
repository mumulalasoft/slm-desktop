import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Slm_Desktop
import "file:/usr/lib/settings/components"

Flickable {
    id: root
    contentHeight: contentColumn.implicitHeight + 48
    clip: true
    property string highlightSettingId: ""
    property var resolutionBinding: SettingsApp.createBindingFor("display", "resolution", "2560 x 1600 (16:10)")
    property var scalingBinding: SettingsApp.createBindingFor("display", "scaling", 1.0)
    property var nightLightBinding: SettingsApp.createBinding("desktop:display/night-light", false)

    Component.onCompleted: {
        resolutionCombo.model = DisplayController.availableResolutions()
        scalingCombo.model = DisplayController.availableScales()
    }

    ColumnLayout {
        id: contentColumn
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 24
        anchors.topMargin: 24
        spacing: 24

        SettingGroup {
            title: qsTr("Resolution & Scaling")
            Layout.fillWidth: true

            SettingCard {
                objectName: "resolution"
                highlighted: root.highlightSettingId === "resolution"
                label: qsTr("Resolution")
                description: qsTr("Built-in Display")
                Layout.fillWidth: true

                ComboBox {
                    id: resolutionCombo
                    textRole: "label"
                    implicitWidth: 200
                    onActivated: DisplayController.applyResolution(model[index].label)
                }
            }

            SettingCard {
                objectName: "scaling"
                highlighted: root.highlightSettingId === "scaling"
                label: qsTr("Scaling")
                description: qsTr("Adjust the size of text and icons.")
                Layout.fillWidth: true

                ComboBox {
                    id: scalingCombo
                    textRole: "label"
                    implicitWidth: 150
                    onActivated: DisplayController.applyScaling(model[index].scale)
                }
            }
        }

        SettingGroup {
            title: qsTr("Night Light")
            Layout.fillWidth: true

            SettingCard {
                objectName: "night-light"
                highlighted: root.highlightSettingId === "night-light"
                label: qsTr("Night Light")
                description: qsTr("Warm screen colors help reduce eye strain in the evening.")
                Layout.fillWidth: true

                SettingToggle {
                    id: nightLightToggle
                    checked: Boolean(root.nightLightBinding.value)
                    onToggled: {
                        root.nightLightBinding.value = checked
                    }
                }
            }

            SettingCard {
                label: qsTr("Schedule")
                visible: nightLightToggle.checked
                Layout.fillWidth: true

                RowLayout {
                    spacing: Theme.metric("spacingSm")
                    Text {
                        text: qsTr("From")
                        color: Theme.color("textSecondary")
                        font.pixelSize: Theme.fontSize("body")
                    }
                    Button { text: "21:00"; flat: true }
                    Text {
                        text: qsTr("to")
                        color: Theme.color("textSecondary")
                        font.pixelSize: Theme.fontSize("body")
                    }
                    Button { text: "07:00"; flat: true }
                }
            }
        }

        SettingGroup {
            title: qsTr("Multiple Displays")
            Layout.fillWidth: true

            SettingCard {
                label: qsTr("Arrangement")
                description: qsTr("Mirror or extend your desktop across displays.")
                Layout.fillWidth: true

                Button {
                    text: qsTr("Configure…")
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
