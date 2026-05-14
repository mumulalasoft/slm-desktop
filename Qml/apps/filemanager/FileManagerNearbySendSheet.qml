import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle as DSStyle

DSStyle.AppDialog {
    id: root

    required property var hostRoot

    readonly property int iconRevision: ((typeof ThemeIconController !== "undefined" && ThemeIconController)
                                         ? ThemeIconController.revision : 0)

    title: qsTr("Send to Nearby Device")
    dialogWidth: 420
    showDefaultCloseFooter: true

    property var pendingPaths: []
    property bool discovering: false
    property string sendError: ""
    property string sendingToDeviceId: ""

    function openForPaths(paths) {
        root.pendingPaths = paths || []
        root.sendError = ""
        root.sendingToDeviceId = ""
        root.open()
        root.refreshDevices()
    }

    function refreshDevices() {
        if (!root.hostRoot || !root.hostRoot.fileManagerApiRef) return
        root.discovering = true
        root.hostRoot.fileManagerApiRef.startNearbyDiscovery()
        refreshTimer.restart()
        devicePollTimer.restart()
    }

    function sendToDevice(deviceId, deviceName) {
        var paths = root.pendingPaths
        if (!paths || paths.length === 0) return
        if (!root.hostRoot || !root.hostRoot.fileManagerApiRef) return
        root.sendingToDeviceId = deviceId
        root.sendError = ""
        var api = root.hostRoot.fileManagerApiRef
        for (var i = 0; i < paths.length; ++i) {
            var res = api.sendFileToNearbyDevice(deviceId, String(paths[i]))
            if (!res || !res.ok) {
                root.sendError = String(res ? (res.error || "send-failed") : "send-failed")
                root.sendingToDeviceId = ""
                return
            }
        }
        root.close()
    }

    onClosed: {
        devicePollTimer.stop()
        refreshTimer.stop()
        root.sendingToDeviceId = ""
        root.sendError = ""
        root.discovering = false
        if (root.hostRoot && root.hostRoot.fileManagerApiRef) {
            root.hostRoot.fileManagerApiRef.stopNearbyDiscovery()
        }
    }

    Timer {
        id: refreshTimer
        interval: 8000
        repeat: false
        onTriggered: root.discovering = false
    }

    Timer {
        id: devicePollTimer
        interval: 1500
        repeat: true
        onTriggered: {
            if (root.hostRoot && root.hostRoot.fileManagerApiRef) {
                var devs = root.hostRoot.fileManagerApiRef.nearbyDevices()
                deviceModel.clear()
                for (var i = 0; i < devs.length; ++i) {
                    var d = devs[i]
                    if (String(d.trustLevel || "") !== "blocked") {
                        deviceModel.append({
                            "deviceId": String(d.deviceId || ""),
                            "name": String(d.name || d.deviceId || qsTr("Unknown Device")),
                            "trustLevel": String(d.trustLevel || "untrusted"),
                            "deviceType": String(d.deviceType || "")
                        })
                    }
                }
            }
        }
    }

    ListModel { id: deviceModel }

    bodyComponent: Component {
        Item {
            implicitHeight: bodyColumn.implicitHeight + 16

            ColumnLayout {
                id: bodyColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 8
                spacing: 12

                DSStyle.Label {
                    visible: root.pendingPaths.length > 0
                    text: root.pendingPaths.length === 1
                          ? qsTr("Sending: %1").arg(String(root.pendingPaths[0] || "").split("/").pop())
                          : qsTr("Sending %1 files").arg(root.pendingPaths.length)
                    color: Theme.color("textSecondary")
                    font.pixelSize: Theme.fontSize("caption")
                    Layout.fillWidth: true
                    elide: Text.ElideMiddle
                }

                RowLayout {
                    spacing: 8
                    Layout.fillWidth: true

                    DSStyle.Label {
                        text: root.discovering
                              ? qsTr("Scanning for nearby devices…")
                              : (deviceModel.count === 0
                                 ? qsTr("No nearby devices found")
                                 : qsTr("%1 device(s) nearby").arg(deviceModel.count))
                        color: Theme.color("textSecondary")
                        font.pixelSize: Theme.fontSize("caption")
                        Layout.fillWidth: true
                    }

                    DSStyle.Button {
                        text: root.discovering ? qsTr("Scanning…") : qsTr("Scan")
                        enabled: !root.discovering
                        flat: true
                        onClicked: root.refreshDevices()
                    }
                }

                DSStyle.Label {
                    visible: root.sendError.length > 0
                    text: qsTr("Failed to send: %1").arg(root.sendError)
                    color: Theme.color("destructive")
                    font.pixelSize: Theme.fontSize("caption")
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                ListView {
                    id: deviceList
                    Layout.fillWidth: true
                    implicitHeight: Math.min(contentHeight, 280)
                    model: deviceModel
                    clip: true
                    spacing: 4

                    delegate: Rectangle {
                        id: deviceItem
                        width: deviceList.width
                        height: Math.max(52, implicitHeight)
                        radius: Theme.radiusMd
                        color: deviceMouse.containsMouse
                               ? Theme.color("hoverItem")
                               : "transparent"

                        readonly property bool sending: root.sendingToDeviceId === model.deviceId
                        readonly property bool trusted: model.trustLevel === "trusted"
                                                        || model.trustLevel === "full"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            anchors.topMargin: 8
                            anchors.bottomMargin: 8
                            spacing: 10

                            Image {
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 28
                                fillMode: Image.PreserveAspectFit
                                source: "image://themeicon/"
                                        + (model.deviceType === "phone"
                                           ? "phone-symbolic"
                                           : (model.deviceType === "tablet"
                                              ? "tablet-symbolic"
                                              : "computer-symbolic"))
                                        + "?v=" + root.iconRevision
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                DSStyle.Label {
                                    text: model.name
                                    font.pixelSize: Theme.fontSize("body")
                                    color: Theme.color("textPrimary")
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                DSStyle.Label {
                                    text: deviceItem.trusted ? qsTr("Trusted") : qsTr("Not trusted")
                                    font.pixelSize: Theme.fontSize("caption")
                                    color: deviceItem.trusted
                                           ? Theme.color("textSecondary")
                                           : Theme.color("textTertiary")
                                }
                            }

                            DSStyle.Button {
                                text: deviceItem.sending ? qsTr("Sending…") : qsTr("Send")
                                enabled: !deviceItem.sending && root.sendingToDeviceId.length === 0
                                flat: true
                                onClicked: root.sendToDevice(model.deviceId, model.name)
                            }
                        }

                        MouseArea {
                            id: deviceMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: root.sendToDevice(model.deviceId, model.name)
                        }
                    }

                    DSStyle.Label {
                        visible: deviceModel.count === 0 && !root.discovering
                        anchors.centerIn: parent
                        text: qsTr("Make sure the target device has SLM Sharing enabled and is on the same network.")
                        color: Theme.color("textSecondary")
                        font.pixelSize: Theme.fontSize("caption")
                        wrapMode: Text.WordWrap
                        width: parent.width - 24
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }
    }
}
