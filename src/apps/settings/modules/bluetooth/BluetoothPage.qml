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
    property var componentIssues: []
    property bool hasBlockingIssues: false

    function refreshComponentIssues() {
        if (typeof ComponentHealth === "undefined" || !ComponentHealth) {
            componentIssues = []
            hasBlockingIssues = false
            return
        }
        componentIssues = ComponentHealth.missingComponentsForDomain("bluetooth")
        if (ComponentHealth.hasBlockingMissingForDomain) {
            hasBlockingIssues = !!ComponentHealth.hasBlockingMissingForDomain("bluetooth")
        } else {
            hasBlockingIssues = (componentIssues || []).some(function(issue) {
                var level = String((issue || {}).severity || "required").toLowerCase()
                return level === "required"
            })
        }
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
            title: qsTr("Bluetooth")
            Layout.fillWidth: true

            MissingComponentsCard {
                visible: (root.componentIssues || []).length > 0
                Layout.fillWidth: true
                issues: root.componentIssues
                message: qsTr("Bluetooth runtime component missing.")
                installHandler: function(componentId) {
                    return ComponentHealth.installComponent(componentId)
                }
                onRefreshRequested: root.refreshComponentIssues()
            }

            SettingCard {
                objectName: "enabled"
                highlighted: root.highlightSettingId === "enabled"
                label: qsTr("Bluetooth")
                description: qsTr("Connect wireless accessories.")
                Layout.fillWidth: true

                SettingToggle {
                    checked: Boolean(root.enabledBinding.value)
                    enabled: !root.hasBlockingIssues
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
                    enabled: !root.hasBlockingIssues
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

    Component.onCompleted: refreshComponentIssues()
}
