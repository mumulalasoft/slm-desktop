import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../../../../../Qml/apps/settings/components"

Flickable {
    id: root
    anchors.fill: parent
    contentHeight: mainLayout.implicitHeight + 40
    clip: true

    property string highlightSettingId: ""
    property var layoutBinding: SettingsApp.createBinding("settings:keyboard/layout", "English (US)")
    property var repeatBinding: SettingsApp.createBindingFor("keyboard", "repeat", true)

    ColumnLayout {
        id: mainLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 24
        anchors.top: parent.top
        anchors.topMargin: 24
        spacing: 20

        SettingGroup {
            title: qsTr("Keyboard")
            Layout.fillWidth: true

            SettingCard {
                objectName: "layout"
                highlighted: root.highlightSettingId === "layout"
                label: qsTr("Input Source")
                description: qsTr("Choose keyboard language layout.")
                Layout.fillWidth: true
                ComboBox {
                    id: layoutCombo
                    model: ["English (US)", "Indonesian", "Japanese"]
                    currentIndex: Math.max(0, model.indexOf(String(root.layoutBinding.value)))
                    Layout.preferredWidth: 220
                    onActivated: root.layoutBinding.value = currentText
                }
            }

            SettingCard {
                objectName: "repeat"
                highlighted: root.highlightSettingId === "repeat"
                label: qsTr("Key Repeat")
                description: qsTr("Repeat keys while pressing.")
                Layout.fillWidth: true
                SettingToggle {
                    checked: Boolean(root.repeatBinding.value)
                    onToggled: root.repeatBinding.value = checked
                }
            }
        }
    }

    Connections {
        target: root.layoutBinding
        function onValueChanged() {
            layoutCombo.currentIndex = Math.max(0, layoutCombo.model.indexOf(String(root.layoutBinding.value)))
        }
    }
}
