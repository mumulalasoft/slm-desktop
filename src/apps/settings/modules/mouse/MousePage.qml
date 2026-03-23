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
    property var speedBinding: SettingsApp.createBindingFor("mouse", "speed", 45)
    property var naturalScrollBinding: SettingsApp.createBindingFor("mouse", "natural-scroll", true)

    ColumnLayout {
        id: mainLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 24
        anchors.top: parent.top
        anchors.topMargin: 24
        spacing: 20

        SettingGroup {
            title: qsTr("Mouse & Trackpad")
            Layout.fillWidth: true

            SettingCard {
                objectName: "speed"
                highlighted: root.highlightSettingId === "speed"
                label: qsTr("Tracking Speed")
                description: qsTr("Control pointer acceleration.")
                Layout.fillWidth: true
                Slider {
                    from: 0
                    to: 100
                    value: Number(root.speedBinding.value) * 100
                    onMoved: root.speedBinding.value = Number(value / 100.0)
                    Layout.preferredWidth: 220
                }
            }

            SettingCard {
                objectName: "natural-scroll"
                highlighted: root.highlightSettingId === "natural-scroll"
                label: qsTr("Natural Scrolling")
                description: qsTr("Content follows finger direction.")
                Layout.fillWidth: true
                SettingToggle {
                    checked: Boolean(root.naturalScrollBinding.value)
                    onToggled: root.naturalScrollBinding.value = checked
                }
            }
        }
    }
}
