import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Slm_Desktop
import SlmStyle as DSStyle

Popup {
    id: root

    required property var hostRoot

    readonly property var currentType: hostRoot.connectServerTypes[Math.max(0, hostRoot.connectServerTypeIndex)]
                                       || hostRoot.connectServerTypes[0]
    readonly property string currentScheme: String((currentType && currentType.scheme) ? currentType.scheme : "smb")
    readonly property bool isWindowsShare: currentScheme === "smb"
    readonly property bool isPublicFtp: hostRoot.connectServerTypeIndex === 0
    readonly property bool showUserDetails: !isPublicFtp
    readonly property color fieldColor: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.06)
    readonly property color fieldFocusColor: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.11) : Qt.rgba(0, 0, 0, 0.09)
    readonly property color dividerColor: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(0, 0, 0, 0.10)

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
    closePolicy: Popup.CloseOnEscape
    width: 488
    height: showUserDetails ? (isWindowsShare ? 420 : 384) : 342
    x: Math.round((hostRoot.width - width) * 0.5)
    y: Math.round((hostRoot.height - height) * 0.34)
    padding: 0
    onClosed: {
        hostRoot.connectServerBusy = false
        hostRoot.pendingConnectServerUri = ""
        hostRoot.connectServerError = ""
    }

    background: Rectangle {
        radius: Theme.radiusControlLarge
        color: Theme.darkMode ? Qt.rgba(0.14, 0.14, 0.15, 1.0) : Qt.rgba(0.96, 0.96, 0.96, 1.0)
        border.width: Theme.borderWidthThin
        border.color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.14)
        antialiasing: true

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 32
            radius: parent.radius
            color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.03) : Qt.rgba(1, 1, 1, 0.50)
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 31
            height: 1
            color: root.dividerColor
        }
    }

    contentItem: Item {
        anchors.fill: parent

        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: actionRow.top
            anchors.margins: 12
            anchors.topMargin: 48
            anchors.bottomMargin: 10
            spacing: 8

            DSStyle.Label {
                Layout.fillWidth: true
                text: "Server Details"
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("body")
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 9
                rowSpacing: 6

                DSStyle.Label {
                    text: "Type:"
                    color: Theme.color("textPrimary")
                    font.pixelSize: Theme.fontSize("body")
                    Layout.preferredWidth: 92
                    horizontalAlignment: Text.AlignRight
                }

                DSStyle.ComboBox {
                    id: connectProtocolCombo
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    enabled: !hostRoot.connectServerBusy
                    model: hostRoot.connectServerTypes
                    textRole: "label"
                    currentIndex: hostRoot.connectServerTypeIndex
                    onActivated: function(index) {
                        hostRoot.connectServerTypeIndex = index
                        var row = hostRoot.connectServerTypes[Math.max(0, index)] || hostRoot.connectServerTypes[0]
                        hostRoot.connectServerPort = Number(row.port || 21)
                        hostRoot.connectServerError = ""
                    }
                }

                DSStyle.Label {
                    text: "Server:"
                    color: Theme.color("textPrimary")
                    font.pixelSize: Theme.fontSize("body")
                    Layout.preferredWidth: 92
                    horizontalAlignment: Text.AlignRight
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 7

                    TextField {
                        id: connectServerField
                        Layout.fillWidth: true
                        Layout.preferredHeight: 28
                        enabled: !hostRoot.connectServerBusy
                        placeholderText: "Server name or IP address"
                        text: hostRoot.connectServerHost
                        selectByMouse: true
                        color: Theme.color("textPrimary")
                        placeholderTextColor: Theme.color("accent")
                        font.family: Theme.fontFamilyUi
                        font.pixelSize: Theme.fontSize("body")
                        leftPadding: 8
                        rightPadding: 8
                        verticalAlignment: TextInput.AlignVCenter
                        background: Rectangle {
                            radius: Theme.radiusSmPlus
                            color: connectServerField.activeFocus ? root.fieldFocusColor : root.fieldColor
                            border.width: Theme.borderWidthNone
                        }
                        onTextChanged: {
                            hostRoot.connectServerHost = text
                            if (hostRoot.connectServerError.length > 0) {
                                hostRoot.connectServerError = ""
                            }
                        }
                        Keys.onReturnPressed: hostRoot.submitConnectServer()
                        Keys.onEnterPressed: hostRoot.submitConnectServer()
                    }

                    DSStyle.Label {
                        text: "Port:"
                        color: Theme.color("textPrimary")
                        font.pixelSize: Theme.fontSize("body")
                        Layout.preferredWidth: 38
                        horizontalAlignment: Text.AlignRight
                    }

                    SpinBox {
                        id: connectServerPortSpin
                        enabled: !hostRoot.connectServerBusy
                        from: 1
                        to: 65535
                        value: Math.max(1, Math.min(65535, Number(hostRoot.connectServerPort || 21)))
                        editable: true
                        Layout.preferredWidth: 100
                        Layout.preferredHeight: 28
                        font.family: Theme.fontFamilyUi
                        font.pixelSize: Theme.fontSize("body")
                        background: Rectangle {
                            radius: Theme.radiusSmPlus
                            color: root.fieldColor
                            border.width: Theme.borderWidthNone
                        }
                        onValueModified: hostRoot.connectServerPort = Number(value)
                    }
                }

                DSStyle.Label {
                    visible: root.isWindowsShare
                    text: "Share:"
                    color: Theme.color("textPrimary")
                    font.pixelSize: Theme.fontSize("body")
                    Layout.preferredWidth: 92
                    horizontalAlignment: Text.AlignRight
                }

                TextField {
                    visible: root.isWindowsShare
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    enabled: !hostRoot.connectServerBusy
                    placeholderText: "Name of share on server (Optional)"
                    text: hostRoot.connectServerShare
                    selectByMouse: true
                    color: Theme.color("textPrimary")
                    placeholderTextColor: Theme.color("accent")
                    font.family: Theme.fontFamilyUi
                    font.pixelSize: Theme.fontSize("body")
                    leftPadding: 8
                    rightPadding: 8
                    verticalAlignment: TextInput.AlignVCenter
                    background: Rectangle {
                        radius: Theme.radiusSmPlus
                        color: parent.activeFocus ? root.fieldFocusColor : root.fieldColor
                        border.width: Theme.borderWidthNone
                    }
                    onTextChanged: {
                        hostRoot.connectServerShare = text
                        if (hostRoot.connectServerError.length > 0) {
                            hostRoot.connectServerError = ""
                        }
                    }
                    Keys.onReturnPressed: hostRoot.submitConnectServer()
                    Keys.onEnterPressed: hostRoot.submitConnectServer()
                }

                DSStyle.Label {
                    text: "Folder:"
                    color: Theme.color("textPrimary")
                    font.pixelSize: Theme.fontSize("body")
                    Layout.preferredWidth: 92
                    horizontalAlignment: Text.AlignRight
                }

                TextField {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    enabled: !hostRoot.connectServerBusy
                    placeholderText: "/"
                    text: hostRoot.connectServerFolder
                    selectByMouse: true
                    color: Theme.color("textPrimary")
                    font.family: Theme.fontFamilyUi
                    font.pixelSize: Theme.fontSize("body")
                    leftPadding: 8
                    rightPadding: 8
                    verticalAlignment: TextInput.AlignVCenter
                    background: Rectangle {
                        radius: Theme.radiusSmPlus
                        color: parent.activeFocus ? root.fieldFocusColor : root.fieldColor
                        border.width: Theme.borderWidthNone
                    }
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

            DSStyle.Label {
                Layout.fillWidth: true
                Layout.topMargin: 8
                visible: root.showUserDetails
                text: "User Details"
                color: Theme.color("textSecondary")
                font.pixelSize: Theme.fontSize("body")
            }

            GridLayout {
                Layout.fillWidth: true
                visible: root.showUserDetails
                columns: 2
                columnSpacing: 9
                rowSpacing: 6

                DSStyle.Label {
                    visible: root.isWindowsShare
                    text: "Domain name:"
                    color: Theme.color("textPrimary")
                    font.pixelSize: Theme.fontSize("body")
                    Layout.preferredWidth: 92
                    horizontalAlignment: Text.AlignRight
                }

                TextField {
                    visible: root.isWindowsShare
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    enabled: !hostRoot.connectServerBusy
                    text: hostRoot.connectServerDomain
                    selectByMouse: true
                    color: Theme.color("textPrimary")
                    font.family: Theme.fontFamilyUi
                    font.pixelSize: Theme.fontSize("body")
                    leftPadding: 8
                    rightPadding: 8
                    verticalAlignment: TextInput.AlignVCenter
                    background: Rectangle {
                        radius: Theme.radiusSmPlus
                        color: parent.activeFocus ? root.fieldFocusColor : root.fieldColor
                        border.width: Theme.borderWidthNone
                    }
                    onTextChanged: hostRoot.connectServerDomain = text
                }

                DSStyle.Label {
                    text: "User name:"
                    color: Theme.color("textPrimary")
                    font.pixelSize: Theme.fontSize("body")
                    Layout.preferredWidth: 92
                    horizontalAlignment: Text.AlignRight
                }

                TextField {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    enabled: !hostRoot.connectServerBusy
                    text: hostRoot.connectServerUser
                    placeholderText: "Optional"
                    selectByMouse: true
                    color: Theme.color("textPrimary")
                    placeholderTextColor: Theme.color("textSecondary")
                    font.family: Theme.fontFamilyUi
                    font.pixelSize: Theme.fontSize("body")
                    leftPadding: 8
                    rightPadding: 8
                    verticalAlignment: TextInput.AlignVCenter
                    background: Rectangle {
                        radius: Theme.radiusSmPlus
                        color: parent.activeFocus ? root.fieldFocusColor : root.fieldColor
                        border.width: Theme.borderWidthNone
                    }
                    onTextChanged: hostRoot.connectServerUser = text
                    Keys.onReturnPressed: hostRoot.submitConnectServer()
                    Keys.onEnterPressed: hostRoot.submitConnectServer()
                }

                DSStyle.Label {
                    text: "Password:"
                    color: Theme.color("textPrimary")
                    font.pixelSize: Theme.fontSize("body")
                    Layout.preferredWidth: 92
                    horizontalAlignment: Text.AlignRight
                }

                TextField {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    enabled: !hostRoot.connectServerBusy
                    text: hostRoot.connectServerPassword
                    echoMode: TextInput.Password
                    selectByMouse: true
                    color: Theme.color("textPrimary")
                    font.family: Theme.fontFamilyUi
                    font.pixelSize: Theme.fontSize("body")
                    leftPadding: 8
                    rightPadding: 8
                    verticalAlignment: TextInput.AlignVCenter
                    background: Rectangle {
                        radius: Theme.radiusSmPlus
                        color: parent.activeFocus ? root.fieldFocusColor : root.fieldColor
                        border.width: Theme.borderWidthNone
                    }
                    onTextChanged: hostRoot.connectServerPassword = text
                    Keys.onReturnPressed: hostRoot.submitConnectServer()
                    Keys.onEnterPressed: hostRoot.submitConnectServer()
                }
            }

            DSStyle.Label {
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

        RowLayout {
            id: actionRow
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 12
            spacing: 7

            DSStyle.Button {
                text: "Cancel"
                enabled: !hostRoot.connectServerBusy
                onClicked: root.close()
            }

            DSStyle.Button {
                text: hostRoot.connectServerBusy ? "Connecting..." : "Connect"
                enabled: !hostRoot.connectServerBusy && String(hostRoot.connectServerHost || "").trim().length > 0
                onClicked: hostRoot.submitConnectServer()
            }
        }
    }
}
