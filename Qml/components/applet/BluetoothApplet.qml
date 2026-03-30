import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.impl 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle

Item {
    id: root

    property var bluetoothManager: BluetoothManager
    property var popupHost: null
    property string pendingDisconnectAddress: ""
    property string pendingDisconnectName: ""
    property bool popupHint: false
    property double lastMenuCloseMs: 0
    readonly property int iconSize: 22
    readonly property int popupGap: Theme.metric("spacingXs")
    readonly property int rowGap: Theme.metric("spacingMd")
    readonly property int dialogGap: Theme.metric("spacingMd")
    readonly property bool popupOpen: popupHint || bluetoothMenu.opened || disconnectConfirmDialog.visible

    Timer {
        id: popupHintTimer
        interval: 320
        repeat: false
        onTriggered: root.popupHint = false
    }

    implicitWidth: indicatorButton.implicitWidth
    implicitHeight: indicatorButton.implicitHeight

    function openMenuSafely() {
        if ((Date.now() - lastMenuCloseMs) < 180) {
            return
        }
        root.popupHint = true
        popupHintTimer.restart()
        Qt.callLater(function() { bluetoothMenu.open() })
    }

    function iconSourceByName(name) {
        var iconName = (name || "").toString().trim()
        if (iconName.length === 0) {
            iconName = "bluetooth-disabled-symbolic"
        }
        return "image://themeicon/" + iconName + "?v=" +
                ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                 ? ThemeIconController.revision : 0)
    }

    ToolButton {
        id: indicatorButton
        anchors.fill: parent
        padding: 0
        onClicked: {
            if ((Date.now() - root.lastMenuCloseMs) < 220) {
                return
            }
            if (bluetoothMenu.opened || bluetoothMenu.visible) {
                root.lastMenuCloseMs = Date.now()
                bluetoothMenu.close()
                return
            }
            root.openMenuSafely()
        }

        contentItem: Item {
            implicitWidth: root.iconSize
            implicitHeight: root.iconSize

            IconImage {
                anchors.centerIn: parent
                width: root.iconSize
                height: root.iconSize
                fillMode: Image.PreserveAspectFit
                source: root.iconSourceByName(root.bluetoothManager ? root.bluetoothManager.iconName : "bluetooth-disabled-symbolic")
                color: Theme.color("textOnGlass")
                opacity: (root.bluetoothManager && root.bluetoothManager.powered) ? 1.0 : 0.65
            }
        }

        background: Rectangle {
            radius: Theme.radiusControl
            color: indicatorButton.hovered ? Theme.color("accentSoft") : "transparent"
            border.width: Theme.borderWidthThin
            border.color: indicatorButton.hovered ? Theme.color("panelBorder") : "transparent"
            Behavior on color { ColorAnimation { duration: Theme.durationSm; easing.type: Theme.easingDefault } }
            Behavior on border.color { ColorAnimation { duration: Theme.durationSm; easing.type: Theme.easingDefault } }
        }
    }

    IndicatorMenu {
        id: bluetoothMenu
        anchorItem: indicatorButton
        popupGap: root.popupGap
        popupWidth: Theme.metric("popupWidthS")
        onAboutToShow: {
            root.popupHint = false
            if (root.bluetoothManager) {
                root.bluetoothManager.refresh()
            }
        }
        onAboutToHide: {
            root.popupHint = false
            root.lastMenuCloseMs = Date.now()
        }

        MenuItem {
            enabled: false
            contentItem: IndicatorSectionLabel {
                text: root.bluetoothManager ? root.bluetoothManager.statusText : "Bluetooth"
                emphasized: true
            }
        }

        MenuItem {
            contentItem: IndicatorSectionRow {
                text: "Bluetooth"
                rowSpacing: root.rowGap
                Switch {
                    enabled: root.bluetoothManager && root.bluetoothManager.available
                    checked: root.bluetoothManager && root.bluetoothManager.powered
                    onToggled: {
                        if (root.bluetoothManager) {
                            root.bluetoothManager.setPowered(checked)
                        }
                    }
                }
            }
        }

        MenuSeparator {}

        MenuItem {
            enabled: false
            contentItem: IndicatorSectionLabel {
                text: "Connected Devices"
                emphasized: true
            }
            visible: root.bluetoothManager && root.bluetoothManager.powered
        }

        Instantiator {
            id: connectedDevicesInstantiator
            model: (root.bluetoothManager && root.bluetoothManager.powered)
                   ? root.bluetoothManager.connectedDeviceItems : []
            delegate: MenuItem {
                enabled: true
                text: "• " + (modelData.name || "")
                onTriggered: {
                    if (modelData.address) {
                        root.pendingDisconnectAddress = modelData.address
                        root.pendingDisconnectName = modelData.name || modelData.address
                        disconnectConfirmDialog.open()
                    }
                }
            }
            onObjectAdded: function(index, object) {
                bluetoothMenu.insertItem(4 + index, object)
            }
            onObjectRemoved: function(index, object) {
                bluetoothMenu.removeItem(object)
            }
        }

        MenuItem {
            enabled: false
            visible: root.bluetoothManager && root.bluetoothManager.powered &&
                     root.bluetoothManager.connectedDevices.length === 0
            text: "No connected devices"
        }

        MenuSeparator {}

        MenuItem {
            text: "Open Bluetooth Settings"
            onTriggered: {
                if (root.bluetoothManager) {
                    root.bluetoothManager.openBluetoothSettings()
                }
            }
        }
    }

    AppDialog {
        id: disconnectConfirmDialog
        x: Math.round(((parent ? parent.width : 320) - width) * 0.5)
        y: Math.round((parent ? parent.height : 180) * 0.22)
        dialogWidth: 320
        title: "Disconnect Device"

        onRejected: {
            root.pendingDisconnectAddress = ""
            root.pendingDisconnectName = ""
        }

        bodyComponent: Component {
            Column {
                spacing: root.dialogGap
                width: parent ? parent.width : 320

                Text {
                    width: parent.width
                    text: "Disconnect " + root.pendingDisconnectName + "?"
                    color: Theme.color("textPrimary")
                    font.family: Theme.fontFamilyUi
                    wrapMode: Text.Wrap
                    lineHeightMode: Text.ProportionalHeight
                    lineHeight: Theme.lineHeight("normal")
                    font.pixelSize: Theme.fontSize("smallPlus")
                    font.weight: Theme.fontWeight("bold")
                }
            }
        }

        footerComponent: Component {
            DialogButtonBox {
                standardButtons: DialogButtonBox.Cancel
                Button {
                    text: "Disconnect"
                    onClicked: {
                        if (root.bluetoothManager && root.pendingDisconnectAddress.length > 0) {
                            root.bluetoothManager.disconnectDevice(root.pendingDisconnectAddress)
                        }
                        root.pendingDisconnectAddress = ""
                        root.pendingDisconnectName = ""
                        disconnectConfirmDialog.close()
                    }
                }
                onRejected: disconnectConfirmDialog.close()
            }
        }
    }
}
