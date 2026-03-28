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
    property bool installBusy: false
    property string installStatus: ""

    function refreshComponentIssues() {
        if (typeof ComponentHealth === "undefined" || !ComponentHealth) {
            componentIssues = []
            return
        }
        componentIssues = ComponentHealth.missingComponentsForDomain("bluetooth")
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

            SettingCard {
                visible: (root.componentIssues || []).length > 0
                label: qsTr("Missing Components")
                description: qsTr("Bluetooth stack is incomplete on this system.")
                Layout.fillWidth: true

                ColumnLayout {
                    spacing: 6
                    Repeater {
                        model: root.componentIssues || []
                        delegate: RowLayout {
                            Layout.fillWidth: true
                            Text {
                                Layout.fillWidth: true
                                text: String((modelData || {}).title || (modelData || {}).componentId || "Unknown")
                                      + " — "
                                      + String((modelData || {}).guidance || (modelData || {}).description || "")
                                wrapMode: Text.WordWrap
                                color: Theme.color("textPrimary")
                            }
                            Button {
                                visible: !!(modelData || {}).autoInstallable
                                enabled: !root.installBusy
                                text: root.installBusy ? qsTr("Installing...") : qsTr("Install")
                                onClicked: {
                                    root.installBusy = true
                                    var res = ComponentHealth.installComponent(String((modelData || {}).componentId || ""))
                                    root.installBusy = false
                                    root.installStatus = (!!res && !!res.ok)
                                            ? qsTr("Install completed. Rechecking...")
                                            : (qsTr("Install failed: ") + String((res && res.error) ? res.error : "unknown"))
                                    root.refreshComponentIssues()
                                }
                            }
                        }
                    }
                    Text {
                        visible: root.installStatus.length > 0
                        text: root.installStatus
                        color: Theme.color("textSecondary")
                        wrapMode: Text.WordWrap
                    }
                }
            }

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

    Component.onCompleted: refreshComponentIssues()
}
