import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Desktop_Shell

Popup {
    id: root

    required property var hostRoot

    function openAndFocus() {
        open()
        Qt.callLater(function() {
            if (connectServerField) {
                connectServerField.forceActiveFocus()
            }
        })
    }

    modal: true
    focus: true
    parent: Overlay.overlay
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    width: 560
    height: 248
    x: Math.round((hostRoot.width - width) * 0.5)
    y: Math.round((hostRoot.height - height) * 0.34)
    padding: 0
    onClosed: {
        hostRoot.connectServerBusy = false
        hostRoot.pendingConnectServerUri = ""
        hostRoot.connectServerError = ""
    }

    background: Rectangle {
        radius: Theme.radiusCard
        color: Theme.color("windowCard")
        border.width: Theme.borderWidthThin
        border.color: Theme.color("panelBorder")
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 8

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.color("panelBorder")
            opacity: Theme.opacityMuted
        }

        Label {
            Layout.fillWidth: true
            text: "Server Details"
            color: Theme.color("textSecondary")
            font.pixelSize: Theme.fontSize("small")
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Label {
                text: "Type:"
                color: Theme.color("textPrimary")
                font.pixelSize: Theme.fontSize("body")
                Layout.preferredWidth: 70
                horizontalAlignment: Text.AlignRight
            }
            ComboBox {
                id: connectProtocolCombo
                Layout.fillWidth: true
                enabled: !hostRoot.connectServerBusy
                model: hostRoot.connectServerTypes
                textRole: "label"
                currentIndex: hostRoot.connectServerTypeIndex
                onActivated: function(index) {
                    hostRoot.connectServerTypeIndex = index
                    var row = hostRoot.connectServerTypes[Math.max(0, index)] || hostRoot.connectServerTypes[0]
                    hostRoot.connectServerPort = Number(row.port || 21)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label {
                text: "Server:"
                color: Theme.color("textPrimary")
                font.pixelSize: Theme.fontSize("body")
                Layout.preferredWidth: 70
                horizontalAlignment: Text.AlignRight
            }
            TextField {
                id: connectServerField
                Layout.fillWidth: true
                enabled: !hostRoot.connectServerBusy
                placeholderText: "Server name or IP"
                text: hostRoot.connectServerHost
                selectByMouse: true
                onTextChanged: {
                    hostRoot.connectServerHost = text
                    if (hostRoot.connectServerError.length > 0) {
                        hostRoot.connectServerError = ""
                    }
                }
                Keys.onReturnPressed: hostRoot.submitConnectServer()
                Keys.onEnterPressed: hostRoot.submitConnectServer()
            }

            Label {
                text: "Port:"
                color: Theme.color("textPrimary")
                font.pixelSize: Theme.fontSize("body")
                Layout.preferredWidth: 50
                horizontalAlignment: Text.AlignRight
            }

            SpinBox {
                id: connectServerPortSpin
                enabled: !hostRoot.connectServerBusy
                from: 1
                to: 65535
                value: Math.max(1, Math.min(65535, Number(hostRoot.connectServerPort || 21)))
                editable: true
                Layout.preferredWidth: 118
                onValueModified: {
                    hostRoot.connectServerPort = Number(value)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label {
                text: "Folder:"
                color: Theme.color("textPrimary")
                font.pixelSize: Theme.fontSize("body")
                Layout.preferredWidth: 70
                horizontalAlignment: Text.AlignRight
            }

            TextField {
                Layout.fillWidth: true
                enabled: !hostRoot.connectServerBusy
                placeholderText: "/"
                text: hostRoot.connectServerFolder
                selectByMouse: true
                onTextChanged: {
                    hostRoot.connectServerFolder = text
                    if (hostRoot.connectServerError.length > 0) {
                        hostRoot.connectServerError = ""
                    }
                }
                Keys.onReturnPressed: hostRoot.submitConnectServer()
                Keys.onEnterPressed: hostRoot.submitConnectServer()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Item { Layout.fillWidth: true }

            Button {
                text: "Cancel"
                enabled: !hostRoot.connectServerBusy
                onClicked: root.close()
            }

            Button {
                text: hostRoot.connectServerBusy ? "Connecting..." : "Connect"
                enabled: !hostRoot.connectServerBusy && String(hostRoot.connectServerHost || "").trim().length > 0
                onClicked: hostRoot.submitConnectServer()
            }
        }

        Label {
            Layout.fillWidth: true
            visible: hostRoot.connectServerError.length > 0
            text: hostRoot.connectServerError
            color: Theme.color("error")
            font.pixelSize: Theme.fontSize("small")
            wrapMode: Text.WrapAnywhere
            lineHeightMode: Text.ProportionalHeight
            lineHeight: Theme.lineHeight("normal")
        }
    }
}
