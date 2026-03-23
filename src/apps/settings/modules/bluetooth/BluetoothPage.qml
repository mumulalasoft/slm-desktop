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
    property var enabledBinding: SettingsApp.createBindingFor("bluetooth", "enabled", false)
    property var pairingBinding: SettingsApp.createBindingFor("bluetooth", "pairing", "")
    property string pairingStatus: ""

    ColumnLayout {
        id: mainLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 24
        anchors.top: parent.top
        anchors.topMargin: 24
        spacing: 20

        SettingGroup {
            title: qsTr("Bluetooth")
            Layout.fillWidth: true

            SettingCard {
                objectName: "enabled"
                highlighted: root.highlightSettingId === "enabled"
                label: qsTr("Bluetooth")
                description: qsTr("Connect wireless accessories.")
                Layout.fillWidth: true

                SettingToggle {
                    checked: Boolean(root.enabledBinding.value)
                    onToggled: root.enabledBinding.value = checked
                }
            }

            SettingCard {
                objectName: "pairing"
                highlighted: root.highlightSettingId === "pairing"
                label: qsTr("Pair New Device")
                description: qsTr("Discover nearby devices.")
                Layout.fillWidth: true
                Button {
                    text: qsTr("Pair...")
                    onClicked: {
                        root.pairingStatus = qsTr("Discovery requested…")
                        root.pairingBinding.value = undefined
                    }
                }
            }

            SettingCard {
                label: qsTr("Status")
                description: root.pairingStatus.length > 0
                                 ? root.pairingStatus
                                 : qsTr("Use Pair New Device to start discovery.")
                Layout.fillWidth: true
            }
        }
    }
}
