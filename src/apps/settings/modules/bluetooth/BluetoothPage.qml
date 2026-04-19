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
    property var bluetoothManager: (typeof BluetoothManager !== "undefined") ? BluetoothManager : null
    property var enabledBinding: SettingsApp.createBindingFor("bluetooth", "enabled", false)
    property string pairingStatus: ""
    property var componentIssues: []
    property bool hasBlockingIssues: false
    property bool bluetoothEnabledPreference: false
    property bool bluetoothPreferenceKnown: false
    readonly property bool bluetoothAvailable: !!bluetoothManager && bluetoothManager.available
    readonly property bool bluetoothPowered: !!bluetoothManager && bluetoothManager.powered
    readonly property var connectedDeviceItems: (!!bluetoothManager && bluetoothManager.powered)
                                               ? bluetoothManager.connectedDeviceItems : []

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

    function syncGlobalBluetoothState(enabled) {
        if (typeof DesktopSettings === "undefined" || !DesktopSettings || !DesktopSettings.setSettingValue) {
            return
        }
        DesktopSettings.setSettingValue("bluetooth.enabled", !!enabled)
    }

    function loadBluetoothPreference() {
        if (typeof DesktopSettings === "undefined" || !DesktopSettings || !DesktopSettings.settingValue) {
            return
        }
        root.bluetoothEnabledPreference = !!DesktopSettings.settingValue("bluetooth.enabled", false)
        root.bluetoothPreferenceKnown = true
    }

    function effectiveBluetoothEnabled() {
        if (root.bluetoothPreferenceKnown) {
            return root.bluetoothEnabledPreference
        }
        return root.bluetoothPowered
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
                    checked: root.effectiveBluetoothEnabled()
                    enabled: !root.hasBlockingIssues && root.bluetoothAvailable
                    onToggled: {
                        root.syncGlobalBluetoothState(checked)
                        root.bluetoothEnabledPreference = checked
                        root.bluetoothPreferenceKnown = true
                        if (!root.bluetoothManager) {
                            root.enabledBinding.value = checked
                            return
                        }
                        root.bluetoothManager.setPowered(checked)
                        root.enabledBinding.value = checked
                    }
                }
            }

            SettingCard {
                objectName: "pairing"
                highlighted: root.highlightSettingId === "pairing"
                label: qsTr("Pair New Device")
                description: qsTr("Discover nearby devices.")
                Layout.fillWidth: true
                Button {
                    text: qsTr("Open Pairing")
                    enabled: !root.hasBlockingIssues && root.bluetoothAvailable
                    onClicked: {
                        if (!root.bluetoothManager) {
                            root.pairingStatus = qsTr("Bluetooth service is unavailable.")
                            return
                        }
                        if (root.bluetoothManager.openBluetoothSettings()) {
                            root.pairingStatus = qsTr("Bluetooth settings opened.")
                        } else {
                            root.pairingStatus = qsTr("Unable to open Bluetooth settings.")
                        }
                    }
                }
            }

            SettingCard {
                label: qsTr("Status")
                description: root.pairingStatus.length > 0
                                 ? root.pairingStatus
                                 : (root.bluetoothManager ? root.bluetoothManager.statusText
                                                          : qsTr("Bluetooth service is unavailable."))
                Layout.fillWidth: true
            }

            SettingCard {
                visible: root.bluetoothPowered
                label: qsTr("Connected Devices")
                description: root.connectedDeviceItems.length > 0
                                 ? root.connectedDeviceItems.map(function(item) { return item.name || "" }).join(", ")
                                 : qsTr("No connected devices.")
                Layout.fillWidth: true
            }
        }
    }

    Connections {
        target: root.bluetoothManager
        function onChanged() {
            root.enabledBinding.value = root.effectiveBluetoothEnabled()
        }
    }

    Connections {
        target: (typeof DesktopSettings !== "undefined") ? DesktopSettings : null
        function onSettingChanged(path) {
            if (String(path || "") === "bluetooth.enabled") {
                root.loadBluetoothPreference()
                root.enabledBinding.value = root.effectiveBluetoothEnabled()
            }
        }
        function onAvailableChanged() {
            root.loadBluetoothPreference()
            root.enabledBinding.value = root.effectiveBluetoothEnabled()
        }
    }

    Component.onCompleted: {
        refreshComponentIssues()
        loadBluetoothPreference()
        if (root.bluetoothManager && root.bluetoothManager.refresh) {
            root.bluetoothManager.refresh()
        }
        root.enabledBinding.value = root.effectiveBluetoothEnabled()
    }
}
